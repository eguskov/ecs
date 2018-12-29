#include "script.h"

#include <ASSERT.h>

#include <iostream>
#include <regex>
#include <fstream>

#include <ecs/ecs.h>
#include <ecs/component.h>
#include <ecs/system.h>

#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scripthandle/scripthandle.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptmath/scriptmath.h>
#include <scriptdictionary/scriptdictionary.h>
#include <scriptbuilder/scriptbuilder.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

#include "scriptECS.h"
#include "scriptQuery.h"

namespace script
{
  static void message(const asSMessageInfo *msg, void *param)
  {
    const char *type = "ERR ";
    if (msg->type == asMSGTYPE_WARNING)
      type = "WARN";
    else if (msg->type == asMSGTYPE_INFORMATION)
      type = "INFO";

    printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
  }

  static void print(const std::string &str)
  {
    DEBUG_LOG("[DEBUG Script]: " << str);
  }

  static void print_release(const std::string &str)
  {
    std::cout << "[Script]: " << str << std::endl;
  }

  static asIScriptEngine *engine = nullptr;

  template<typename T>
  static T& alloc_result(const T &v)
  {
    T &ref = *(T*)alloc_frame_mem(sizeof(T));
    ref = eastl::move(v);
    return ref;
  }

  #define DEF_OP_1(name, ret, code) \
    template<typename T> \
    struct name \
    { \
      static ret exec(const T& v1) \
      { \
        return _ALLOC(T, code); \
      } \
      using ExecType = ret (*) (const T&); \
      static constexpr ExecType execPtr = &name<T>::exec; \
    }; \

  #define DEF_OP_2(name, ret, code) \
    template<typename T> \
    struct name \
    { \
      static ret exec(const T& v1, const T& v2) \
      { \
        return _ALLOC(T, code); \
      } \
      using ExecType = ret (*) (const T&, const T&); \
      static constexpr ExecType execPtr = &name<T>::exec; \
    }; \

  #define DEF_OP_ASSIGN(name, code) \
    template<typename T> \
    struct name \
    { \
      static T& exec(T& v1, const T& v2) \
      { \
        code; \
        return v1; \
      } \
      using ExecType = T& (*) (T&, const T&); \
      static constexpr ExecType execPtr = &name<T>::exec; \
    }; \

  #define _ALLOC(T, code) alloc_result<T>(code)

  DEF_OP_1(opNormalize, T&, glm::normalize(v1));
  DEF_OP_1(opNeg, T&, -v1);

  DEF_OP_2(opAdd, T&, v1 + v2);
  DEF_OP_2(opSub, T&, v1 - v2);
  DEF_OP_2(opMod, T&, glm::cross(v1, v2));

  #undef _ALLOC

  #define _ALLOC(T, code) code

  DEF_OP_1(opLength, float, glm::length(v1));
  DEF_OP_2(opMul, float, glm::dot(v1, v2));

  #undef _ALLOC

  DEF_OP_ASSIGN(opAssign, v1 = v2);
  DEF_OP_ASSIGN(opAddAssign, v1 += v2);
  DEF_OP_ASSIGN(opSubAssign, v1 -= v2);

  template<typename T>
  struct opMulScalar
  {
    static T& exec(const T &v1, float s)
    {
      return alloc_result(v1 * s);
    }
    using ExecType = T& (*) (const T &, float);
    static constexpr ExecType execPtr = &opMulScalar<T>::exec;
  };

  template<typename T>
  struct opMulScalarAssign
  {
    static T& exec(T &v1, float s)
    {
      v1 *= s;
      return v1;
    }
    using ExecType = T& (*) (T &, float);
    static constexpr ExecType execPtr = &opMulScalarAssign<T>::exec;
  };

  template<typename T>
  struct opImplConv
  {
    static T exec(const T& v1)
    {
      return v1;
    }
    using ExecType = T (*) (const T&);
    static constexpr ExecType execPtr = &opImplConv<T>::exec;
  };

  template<>
  struct opImplConv<JFrameValue>
  {
    static JFrameValue& exec(JFrameValue& v1)
    {
      return v1;
    }
    using ExecType = JFrameValue& (*) (JFrameValue&);
    static constexpr ExecType execPtr = &opImplConv<JFrameValue>::exec;
  };

  static void initIterator(asITypeInfo *type, void *data)
  {
  }

  static void releaseIterator(void *it)
  {
  }

  ScriptQuery* create_script_query(asITypeInfo *type, void *data)
  {
    ScriptQuery *sq = new (RawAllocator<ScriptQuery>::alloc()) ScriptQuery;

    asIScriptContext *ctx = asGetActiveContext();
    ASSERT(ctx != nullptr);

    ScriptECS *scriptECS = (ScriptECS *)ctx->GetUserData(1000);
    ASSERT(scriptECS != nullptr);

    auto res = scriptECS->dataQueries.find(type->GetSubTypeId());
    ASSERT(res != scriptECS->dataQueries.end());

    sq->type = engine->GetTypeInfoById(res->first.id);
    ASSERT(sq->type != nullptr);
    sq->query = &res->second;

    return sq;
  }

  int* create_script_query_count(asITypeInfo *type, void *data)
  {
    asIScriptContext *ctx = asGetActiveContext();
    ASSERT(ctx != nullptr);

    ScriptECS *scriptECS = (ScriptECS *)ctx->GetUserData(1000);
    ASSERT(scriptECS != nullptr);

    auto res = scriptECS->dataQueries.find(type->GetSubTypeId());
    ASSERT(res != scriptECS->dataQueries.end());

    return new (RawAllocator<int>::alloc()) int(res->second.entitiesCount);
  }

  int create_script_query_count_get(size_t *sz)
  {
    return *sz;
  }

  static JFrameAllocator json_frame_allocator; 

  void* create_map()
  {
    return new (RawAllocator<JFrameValue>::alloc()) JFrameValue(rapidjson::kObjectType);
  }

  void* create_array()
  {
    return new (RawAllocator<JFrameValue>::alloc()) JFrameValue(rapidjson::kArrayType);
  }

  JFrameValue& set_map_int(JFrameValue &m, const std::string &key, int value)
  {
    const char *k = key.c_str();
    if (m.HasMember(k))
      m[k] = value;
    else
    {
      JFrameValue jk(rapidjson::kStringType);
      jk.SetString(k, key.length(), json_frame_allocator);

      JFrameValue jv(rapidjson::kNumberType);
      jv.SetInt(value);

      m.AddMember(eastl::move(jk), eastl::move(jv), json_frame_allocator);
    }

    return m;
  }

  JFrameValue& set_map_bool(JFrameValue &m, const std::string &key, bool value)
  {
    const char *k = key.c_str();
    if (m.HasMember(k))
      m[k] = value;
    else
    {
      JFrameValue jk(rapidjson::kStringType);
      jk.SetString(k, key.length(), json_frame_allocator);

      JFrameValue jv(rapidjson::kNumberType);
      jv.SetBool(value);

      m.AddMember(eastl::move(jk), eastl::move(jv), json_frame_allocator);
    }

    return m;
  }

  JFrameValue& set_map_float(JFrameValue &m, const std::string &key, float value)
  {
    const char *k = key.c_str();
    if (m.HasMember(k))
      m[k] = value;
    else
    {
      JFrameValue jk(rapidjson::kStringType);
      jk.SetString(k, key.length(), json_frame_allocator);

      JFrameValue jv(rapidjson::kNumberType);
      jv.SetFloat(value);

      m.AddMember(eastl::move(jk), eastl::move(jv), json_frame_allocator);
    }

    return m;
  }

  JFrameValue& set_map_string(JFrameValue &m, const std::string &key, const std::string &value)
  {
    JFrameValue jv(rapidjson::kStringType);
    jv.SetString(value.c_str(), value.length(), json_frame_allocator);

    const char *k = key.c_str();
    if (m.HasMember(k))
      m[k] = eastl::move(jv);
    else
    {
      JFrameValue jk(rapidjson::kStringType);
      jk.SetString(k, key.length(), json_frame_allocator);

      m.AddMember(eastl::move(jk), eastl::move(jv), json_frame_allocator);
    }

    return m;
  }

  JFrameValue& set_map_map(JFrameValue &m, const std::string &key, const JFrameValue &value)
  {
    JFrameValue jv(rapidjson::kObjectType);
    jv.CopyFrom(value, json_frame_allocator);

    const char *k = key.c_str();
    if (m.HasMember(k))
      m[k] = eastl::move(jv);
    else
    {
      JFrameValue jk(rapidjson::kStringType);
      jk.SetString(k, key.length(), json_frame_allocator);

      m.AddMember(eastl::move(jk), eastl::move(jv), json_frame_allocator);
    }

    return m;
  }

  JFrameValue& set_map_array(JFrameValue &m, const std::string &key, const JFrameValue &value)
  {
    JFrameValue jv(rapidjson::kArrayType);
    jv.CopyFrom(value, json_frame_allocator);

    const char *k = key.c_str();
    if (m.HasMember(k))
      m[k] = eastl::move(jv);
    else
    {
      JFrameValue jk(rapidjson::kStringType);
      jk.SetString(k, key.length(), json_frame_allocator);

      m.AddMember(eastl::move(jk), eastl::move(jv), json_frame_allocator);
    }

    return m;
  }

  bool get_map_bool(const JFrameValue &m, const std::string &key)
  {
    if (m.IsObject() && m.HasMember(key.c_str()))
      return m[key.c_str()].GetBool();
    return false;
  }

  int get_map_int(const JFrameValue &m, const std::string &key)
  {
    if (m.IsObject() && m.HasMember(key.c_str()))
      return m[key.c_str()].GetInt();
    return 0;
  }

  float get_map_float(const JFrameValue &m, const std::string &key)
  {
    if (m.IsObject() && m.HasMember(key.c_str()))
      return m[key.c_str()].GetFloat();
    return 0.f;
  }

  std::string get_map_string(const JFrameValue &m, const std::string &key)
  {
    if (m.IsObject() && m.HasMember(key.c_str()))
      return m[key.c_str()].GetString();
    return "";
  }

  const JFrameValue& get_map_map(const JFrameValue &m, const std::string &key)
  {
    if (m.IsObject() && m.HasMember(key.c_str()))
      return m[key.c_str()];
    return *(JFrameValue*)create_map();
  }

  const JFrameValue& get_map_array(const JFrameValue &m, const std::string &key)
  {
    if (m.IsObject() && m.HasMember(key.c_str()))
      return m[key.c_str()];
    return *(JFrameValue*)create_array();
  }

  JFrameValue& push_array_bool(JFrameValue &arr, bool value)
  {
    JFrameValue jv(rapidjson::kNumberType);
    jv.SetBool(value);
    arr.PushBack(eastl::move(jv), json_frame_allocator);
    return arr;
  }

  JFrameValue& push_array_int(JFrameValue &arr, int value)
  {
    JFrameValue jv(rapidjson::kNumberType);
    jv.SetInt(value);
    arr.PushBack(eastl::move(jv), json_frame_allocator);
    return arr;
  }

  JFrameValue& push_array_float(JFrameValue &arr, float value)
  {
    JFrameValue jv(rapidjson::kNumberType);
    jv.SetFloat(value);
    arr.PushBack(eastl::move(jv), json_frame_allocator);
    return arr;
  }

  JFrameValue& push_array_string(JFrameValue &arr, const std::string &value)
  {
    JFrameValue jv(rapidjson::kStringType);
    jv.SetString(value.c_str(), value.length(), json_frame_allocator);
    arr.PushBack(eastl::move(jv), json_frame_allocator);
    return arr;
  }

  JFrameValue& push_array_map(JFrameValue &arr, const JFrameValue &value)
  {
    JFrameValue jv(rapidjson::kObjectType);
    jv.CopyFrom(value, json_frame_allocator);
    arr.PushBack(eastl::move(jv), json_frame_allocator);
    return arr;
  }

  JFrameValue& push_array_array(JFrameValue &arr, const JFrameValue &value)
  {
    JFrameValue jv(rapidjson::kArrayType);
    jv.CopyFrom(value, json_frame_allocator);
    arr.PushBack(eastl::move(jv), json_frame_allocator);
    return arr;
  }

  int array_get_size(const JFrameValue &arr)
  {
    return arr.Size();
  }

  bool array_get_item_bool(const JFrameValue &arr, int index)
  {
    return arr[index].GetBool();
  }

  int array_get_item_int(const JFrameValue &arr, int index)
  {
    return arr[index].GetInt();
  }

  float array_get_item_float(const JFrameValue &arr, int index)
  {
    return arr[index].GetFloat();
  }

  std::string array_get_item_string(const JFrameValue &arr, int index)
  {
    return arr[index].GetString();
  }

  const JFrameValue& array_get_item_map(const JFrameValue &arr, int index)
  {
    return arr[index];
  }

  const JFrameValue& array_get_item_array(const JFrameValue &arr, int index)
  {
    return arr[index];
  }

  bool is_map(const JFrameValue &v)
  {
    return v.IsObject();
  }

  bool is_array(const JFrameValue &v)
  {
    return v.IsArray();
  }

  void* create_array_from_list(void *data)
  {
    asUINT num = *(asUINT*)data;
    ASSERT(num > 0);

    JFrameValue *arr = new (RawAllocator<JFrameValue>::alloc()) JFrameValue(rapidjson::kArrayType);

    uint8_t *ptr = ((uint8_t*)data) + 4;
    for (asUINT i = 0; i < num; ++i)
    {
      int32_t &typeId = *(int32_t*)ptr;
      ptr += sizeof(int32_t);

      if (typeId == asTYPEID_INT32)
      {
        push_array_int(*arr, *(int*)ptr);
        ptr += sizeof(int32_t);
      }
      else if (typeId == asTYPEID_FLOAT)
      {
        push_array_float(*arr, *(float*)ptr);
        ptr += sizeof(float);
      }
      else if (typeId == asTYPEID_BOOL)
      {
        push_array_bool(*arr, *(bool*)ptr);
        ptr += sizeof(int32_t);
      }
      else
      {
        asITypeInfo *info = engine->GetTypeInfoById(typeId);
        ASSERT(info != nullptr);
        if (::strcmp(info->GetName(), "string") == 0)
        {
          push_array_string(*arr, *(std::string*)ptr);
          ptr += sizeof(std::string);
        }
        else if (::strcmp(info->GetName(), "Map") == 0)
        {
          push_array_map(*arr, **(JFrameValue**)ptr);
          ptr += sizeof(JFrameValue*);
        }
        else if (::strcmp(info->GetName(), "Array") == 0)
        {
          push_array_array(*arr, **(JFrameValue**)ptr);
          ptr += sizeof(JFrameValue*);
        }
        else
          ASSERT(false);
      }
    }

    return arr;
  }

  void* create_map_from_list(void *data)
  {
    asUINT num = *(asUINT*)data;
    // ASSERT(num > 0);

    JFrameValue *m = new (RawAllocator<JFrameValue>::alloc()) JFrameValue(rapidjson::kObjectType);

    uint8_t *ptr = ((uint8_t*)data) + 4;
    for (asUINT i = 0; i < num; ++i)
    {
      std::string &key = *(std::string*)ptr;
      ptr += sizeof(std::string);

      int32_t &typeId = *(int32_t*)ptr;
      ptr += sizeof(int32_t);

      if (typeId == asTYPEID_INT32)
      {
        set_map_int(*m, key, *(int*)ptr);
        ptr += sizeof(int32_t);
      }
      else if (typeId == asTYPEID_FLOAT)
      {
        set_map_float(*m, key, *(float*)ptr);
        ptr += sizeof(float);
      }
      else if (typeId == asTYPEID_BOOL)
      {
        set_map_bool(*m, key, *(bool*)ptr);
        ptr += sizeof(int32_t);
      }
      else
      {
        asITypeInfo *info = engine->GetTypeInfoById(typeId);
        ASSERT(info != nullptr);
        if (::strcmp(info->GetName(), "string") == 0)
        {
          set_map_string(*m, key, *(std::string*)ptr);
          ptr += sizeof(std::string);
        }
        else if (::strcmp(info->GetName(), "Map") == 0)
        {
          set_map_map(*m, key, **(JFrameValue**)ptr);
          ptr += sizeof(JFrameValue*);
        }
        else if (::strcmp(info->GetName(), "Array") == 0)
        {
          set_map_array(*m, key, **(JFrameValue**)ptr);
          ptr += sizeof(JFrameValue*);
        }
        else
          ASSERT(false);
      }
    }

    return m;
  }

  void create_entity(const std::string &templ, const JFrameValue &m)
  {
    g_mgr->createEntity(templ.c_str(), m);
  }

  void create_entities_from_file(const std::string &filename)
  {
    // TODO: Replace with allocation
    static char readBuffer[4 << 20];

    FILE* fp = nullptr;
    fopen_s(&fp, filename.c_str(), "rb");
    if (fp)
    {
      rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
      JFrameDocument doc;
      doc.ParseStream(is);
      fclose(fp);

      ASSERT(doc.HasMember("$entities"));
      ASSERT(doc["$entities"].IsArray());
      for (int i = 0; i < (int)doc["$entities"].Size(); ++i)
      {
        const JFrameValue &ent = doc["$entities"][i];
        g_mgr->createEntity(ent["$template"].GetString(), ent["$components"]);
      }
    }
  }

  void delete_entity(const EntityId &eid)
  {
    g_mgr->deleteEntity(eid);
  }

  bool init()
  {
    engine = asCreateScriptEngine();

    engine->SetMessageCallback(asFUNCTION(message), 0, asCALL_CDECL);

    // RegisterScriptHandle(engine);
    RegisterScriptArray(engine, true);
    // TODO: Replace std::string with eastl::string
    // TODO: Optimize string's cache if possible
    RegisterStdString(engine);
    RegisterStdStringUtils(engine);
    RegisterScriptMath(engine);
    // RegisterScriptDictionary(engine);

    engine->RegisterObjectType("QueryIterator<class T>", sizeof(ScriptQuery::Iterator), asOBJ_VALUE | asOBJ_TEMPLATE | asGetTypeTraits<ScriptQuery::Iterator>());
    engine->RegisterObjectBehaviour("QueryIterator<T>", asBEHAVE_CONSTRUCT, "void f(int&in)", asFUNCTIONPR(initIterator, (asITypeInfo*, void*), void), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("QueryIterator<T>", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR(releaseIterator, (void*), void), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("QueryIterator<T>", "QueryIterator<T>& opAssign(const QueryIterator<T>&in)", asMETHODPR(ScriptQuery::Iterator, operator=, (const ScriptQuery::Iterator&), ScriptQuery::Iterator&), asCALL_THISCALL);
    engine->RegisterObjectMethod("QueryIterator<T>", "bool hasNext() const", asMETHODPR(ScriptQuery::Iterator, hasNext, () const, bool), asCALL_THISCALL);
    engine->RegisterObjectMethod("QueryIterator<T>", "void opPreInc()", asMETHODPR(ScriptQuery::Iterator, operator++, (), void), asCALL_THISCALL);
    engine->RegisterObjectMethod("QueryIterator<T>", "T@ get()", asMETHODPR(ScriptQuery::Iterator, get, (), void*), asCALL_THISCALL);

    engine->RegisterObjectType("Count<class T>", 0, asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT);
    engine->RegisterObjectBehaviour("Count<T>", asBEHAVE_FACTORY, "Count<T>@ f(int&in)", asFUNCTION(create_script_query_count), asCALL_CDECL);
    engine->RegisterObjectMethod("Count<T>", "int get()", asFUNCTION(create_script_query_count_get), asCALL_CDECL_OBJFIRST);

    engine->RegisterObjectType("Map", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectBehaviour("Map", asBEHAVE_FACTORY, "Map@ f()", asFUNCTION(create_map), asCALL_CDECL);
    engine->RegisterObjectBehaviour("Map", asBEHAVE_LIST_FACTORY, "Map@ f(int &in) {repeat {string, ?}}", asFUNCTION(create_map_from_list), asCALL_CDECL);

    engine->RegisterObjectType("Array", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectBehaviour("Array", asBEHAVE_FACTORY, "Array@ f()", asFUNCTION(create_array), asCALL_CDECL);
    engine->RegisterObjectBehaviour("Array", asBEHAVE_LIST_FACTORY, "Array@ f(int &in) {repeat ?}", asFUNCTION(create_array_from_list), asCALL_CDECL);

    engine->RegisterObjectMethod("Map", "Map@ set(string&in, bool)", asFUNCTION(set_map_bool), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "Map@ set(string&in, int)", asFUNCTION(set_map_int), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "Map@ set(string&in, float)", asFUNCTION(set_map_float), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "Map@ set(string&in, string&in)", asFUNCTION(set_map_string), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "Map@ set(string&in, Map@)", asFUNCTION(set_map_map), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "Map@ set(string&in, Array@)", asFUNCTION(set_map_array), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "bool getBool(string&in)", asFUNCTION(get_map_bool), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "int getInt(string&in)", asFUNCTION(get_map_int), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "float getFloat(string&in)", asFUNCTION(get_map_float), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "string getString(string&in)", asFUNCTION(get_map_string), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "Map@ getMap(string&in)", asFUNCTION(get_map_map), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "Array@ getArray(string&in)", asFUNCTION(get_map_array), asCALL_CDECL_OBJFIRST);
    register_component_function("Map", "Array@ opImplConv() const", asFUNCTION(opImplConv<JFrameValue>::execPtr));

    engine->RegisterObjectMethod("Array", "Array@ push(bool)", asFUNCTION(push_array_bool), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "Array@ push(int)", asFUNCTION(push_array_int), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "Array@ push(float)", asFUNCTION(push_array_float), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "Array@ push(string&in)", asFUNCTION(push_array_string), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "Array@ push(Map@)", asFUNCTION(push_array_map), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "Array@ push(Array@)", asFUNCTION(push_array_array), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "int size()", asFUNCTION(array_get_size), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "bool getBool(int)", asFUNCTION(array_get_item_bool), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "int getInt(int)", asFUNCTION(array_get_item_int), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "float getFloat(int)", asFUNCTION(array_get_item_float), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "string getString(int)", asFUNCTION(array_get_item_string), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "Map@ getMap(int)", asFUNCTION(array_get_item_map), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "Array@ getArray(int)", asFUNCTION(array_get_item_array), asCALL_CDECL_OBJFIRST);
    register_component_function("Array", "Map@ opImplConv() const", asFUNCTION(opImplConv<JFrameValue>::execPtr));

    engine->RegisterObjectMethod("Map", "bool isMap() const", asFUNCTION(is_map), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Map", "bool isArray() const", asFUNCTION(is_array), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "bool isMap() const", asFUNCTION(is_map), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("Array", "bool isArray() const", asFUNCTION(is_array), asCALL_CDECL_OBJFIRST);

    engine->RegisterObjectType("Query<class T>", 0, asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT);
    engine->RegisterObjectBehaviour("Query<T>", asBEHAVE_FACTORY, "Query<T>@ f(int&in)", asFUNCTIONPR(create_script_query, (asITypeInfo*, void*), ScriptQuery*), asCALL_CDECL);
    engine->RegisterObjectMethod("Query<T>", "QueryIterator<T> perform()", asMETHODPR(ScriptQuery, perform, (), ScriptQuery::Iterator), asCALL_THISCALL);

    register_component<EntityId>("EntityId");
    register_component_property("EntityId", "uint handle", 0);
    register_component_function("EntityId", "EntityId& opAssign(const EntityId&in)", asFUNCTION(opAssign<EntityId>::execPtr));

    register_component<bool>("boolean");
    register_component_property("boolean", "bool v", 0);
    register_component_function("boolean", "boolean& opAssign(const boolean&in)", asFUNCTION(opAssign<bool>::execPtr));
    register_component_function("boolean", "boolean& opAssign(const bool&in)", asFUNCTION(opAssign<bool>::execPtr));
    register_component_function("boolean", "bool opImplConv() const", asFUNCTION(opImplConv<bool>::execPtr));

    register_component<float>("real");
    register_component_property("real", "float v", 0);
    register_component_function("real", "real& opAssign(const real&in)", asFUNCTION(opAssign<float>::execPtr));
    register_component_function("real", "real& opAssign(const float&in)", asFUNCTION(opAssign<float>::execPtr));
    register_component_function("real", "float opImplConv() const", asFUNCTION(opImplConv<float>::execPtr));

    register_component<glm::vec2>("vec2");
    register_component_property("vec2", "float x", offsetof(glm::vec2, x));
    register_component_property("vec2", "float y", offsetof(glm::vec2, y));
    register_component_function("vec2", "vec2& opAssign(const vec2&in)", asFUNCTION(opAssign<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2& opAddAssign(const vec2&in)", asFUNCTION(opAddAssign<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2& opSubAssign(const vec2&in)", asFUNCTION(opSubAssign<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2@ opAdd(const vec2&in) const", asFUNCTION(opAdd<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2@ opSub(const vec2&in) const", asFUNCTION(opSub<glm::vec2>::execPtr));
    register_component_function("vec2", "float opMul(const vec2&in) const", asFUNCTION(opMul<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2@ opMul(float) const", asFUNCTION(opMulScalar<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2@ opMul_r(float) const", asFUNCTION(opMulScalar<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2@ opMulAssign(float) const", asFUNCTION(opMulScalarAssign<glm::vec2>::execPtr));
    register_component_function("vec2", "vec2@ opNeg() const", asFUNCTION(opNeg<glm::vec2>::execPtr));

    register_component<glm::vec3>("vec3");
    register_component_property("vec3", "float x", offsetof(glm::vec3, x));
    register_component_property("vec3", "float y", offsetof(glm::vec3, y));
    register_component_property("vec3", "float z", offsetof(glm::vec3, z));
    register_component_function("vec3", "vec3& opAssign(const vec3&in)", asFUNCTION(opAssign<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3& opAddAssign(const vec3&in)", asFUNCTION(opAddAssign<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3& opSubAssign(const vec3&in)", asFUNCTION(opSubAssign<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3@ opAdd(const vec3&in) const", asFUNCTION(opAdd<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3@ opSub(const vec3&in) const", asFUNCTION(opSub<glm::vec3>::execPtr));
    register_component_function("vec3", "float opMul(const vec3&in) const", asFUNCTION(opMul<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3@ opMod(const vec3&in) const", asFUNCTION(opMod<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3@ opMul(float) const", asFUNCTION(opMulScalar<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3@ opMul_r(float) const", asFUNCTION(opMulScalar<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3@ opMulAssign(float) const", asFUNCTION(opMulScalarAssign<glm::vec3>::execPtr));
    register_component_function("vec3", "vec3@ opNeg() const", asFUNCTION(opNeg<glm::vec3>::execPtr));

    register_component<glm::vec4>("vec4");
    register_component_property("vec4", "float x", offsetof(glm::vec4, x));
    register_component_property("vec4", "float y", offsetof(glm::vec4, y));
    register_component_property("vec4", "float z", offsetof(glm::vec4, z));
    register_component_property("vec4", "float w", offsetof(glm::vec4, w));
    register_component_function("vec4", "vec4& opAssign(const vec4&in)", asFUNCTION(opAssign<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4& opAddAssign(const vec4&in)", asFUNCTION(opAddAssign<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4& opSubAssign(const vec4&in)", asFUNCTION(opSubAssign<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4@ opAdd(const vec4&in) const", asFUNCTION(opAdd<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4@ opSub(const vec4&in) const", asFUNCTION(opSub<glm::vec4>::execPtr));
    register_component_function("vec4", "float opMul(const vec4&in) const", asFUNCTION(opMul<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4@ opMul(float) const", asFUNCTION(opMulScalar<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4@ opMul_r(float) const", asFUNCTION(opMulScalar<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4@ opMulAssign(float) const", asFUNCTION(opMulScalarAssign<glm::vec4>::execPtr));
    register_component_function("vec4", "vec4@ opNeg() const", asFUNCTION(opNeg<glm::vec4>::execPtr));

    register_function("float dot(const vec2&in, const vec2&in)", asFUNCTION(opMul<glm::vec2>::execPtr));
    register_function("float length(const vec2&in)", asFUNCTION(opLength<glm::vec2>::execPtr));
    register_function("vec2@ normalize(const vec2&in)", asFUNCTION(opNormalize<glm::vec2>::execPtr));

    register_function("vec3@ cross(const vec3&in, const vec3&in)", asFUNCTION(opMod<glm::vec3>::execPtr));
    register_function("float dot(const vec3&in, const vec3&in)", asFUNCTION(opMul<glm::vec3>::execPtr));
    register_function("float length(const vec3&in)", asFUNCTION(opLength<glm::vec3>::execPtr));
    register_function("vec3@ normalize(const vec3&in)", asFUNCTION(opNormalize<glm::vec3>::execPtr));

    register_function("float dot(const vec4&in, const vec4&in)", asFUNCTION(opMul<glm::vec4>::execPtr));
    register_function("float length(const vec4&in)", asFUNCTION(opLength<glm::vec4>::execPtr));
    register_function("vec4@ normalize(const vec4&in)", asFUNCTION(opNormalize<glm::vec4>::execPtr));

    int r = 0;
    r = engine->RegisterGlobalFunction("void print(string &in)", asFUNCTION(print), asCALL_CDECL);
    ASSERT(r >= 0);
    r = engine->RegisterGlobalFunction("void print_release(string &in)", asFUNCTION(print_release), asCALL_CDECL);
    ASSERT(r >= 0);

    engine->RegisterGlobalFunction("void create_entity(string&in, Map@)", asFUNCTION(create_entity), asCALL_CDECL);
    engine->RegisterGlobalFunction("void create_entities_from_file(string&in)", asFUNCTION(create_entities_from_file), asCALL_CDECL);
    engine->RegisterGlobalFunction("void delete_entity(const EntityId@)", asFUNCTION(delete_entity), asCALL_CDECL);

    return true;
  }

  void release()
  {
    if (!engine)
      return;

    engine->Release();
    engine = nullptr;
  }

  bool build_module(const char *name, const char *path, const eastl::function<void(CScriptBuilder&, asIScriptModule&)> &callback)
  {
    ASSERT(engine != nullptr);
    ASSERT(name && name[0]);

    if (engine->GetModule(name) != nullptr)
    {
      engine->DiscardModule(name);
    }

    CScriptBuilder builder;
    int r = builder.StartNewModule(engine, name);
    ASSERT(r >= 0);
    r = builder.AddSectionFromFile(path);
    ASSERT(r >= 0);
    r = builder.BuildModule();
    ASSERT(r >= 0);

    asIScriptModule *module = engine->GetModule(name);
    ASSERT(module != nullptr);

    if (module)
      callback(builder, *module);

    return module != nullptr;
  }

  template <typename T, typename Allocator>
  struct set_helper;

  template <typename Allocator>
  struct set_helper<const char*, Allocator>
  {
    static void set(JFrameValue &jv, const char *value, Allocator &allocator) { jv.SetString(value, allocator); }
  };

  template <typename Allocator>
  struct set_helper<int, Allocator>
  {
    static void set(JFrameValue &jv, int value, Allocator &allocator) { jv.SetInt(value); }
  };

  template <typename Allocator>
  struct set_helper<JFrameValue, Allocator>
  {
    static void set(JFrameValue &jv, const JFrameValue &value, Allocator &allocator) { jv.SetObject(); jv.CopyFrom(value, allocator); }
  };

  template <typename T, typename Allocator>
  static void add_member(const char *key, const T &value, JFrameValue &dst, Allocator &allocator)
  {
    JFrameValue jk, jv;

    set_helper<decltype(key), Allocator>::set(jk, key, allocator);
    set_helper<T, Allocator>::set(jv, value, allocator);

    dst.AddMember(eastl::move(jk), eastl::move(jv), allocator);
  }

  template <typename Allocator>
  static void add_type(const char *key, int type_id, JFrameValue &dst, Allocator &allocator)
  {
    if (type_id == asTYPEID_VOID)
      add_member<const char*>(key, "void", dst, allocator);
    else if (type_id == asTYPEID_BOOL)
      add_member<const char*>(key, "bool", dst, allocator);
    else if (type_id == asTYPEID_INT8)
      add_member<const char*>(key, "int8", dst, allocator);
    else if (type_id == asTYPEID_INT16)
      add_member<const char*>(key, "int16", dst, allocator);
    else if (type_id == asTYPEID_INT32)
      add_member<const char*>(key, "int32", dst, allocator);
    else if (type_id == asTYPEID_INT64)
      add_member<const char*>(key, "int64", dst, allocator);
    else if (type_id == asTYPEID_UINT8)
      add_member<const char*>(key, "uint8", dst, allocator);
    else if (type_id == asTYPEID_UINT16)
      add_member<const char*>(key, "uint16", dst, allocator);
    else if (type_id == asTYPEID_UINT32)
      add_member<const char*>(key, "uint32", dst, allocator);
    else if (type_id == asTYPEID_UINT64)
      add_member<const char*>(key, "uint64", dst, allocator);
    else if (type_id == asTYPEID_FLOAT)
      add_member<const char*>(key, "float", dst, allocator);
    else if (type_id == asTYPEID_DOUBLE)
      add_member<const char*>(key, "double", dst, allocator);
    else
      add_member(key, engine->GetTypeInfoById(type_id)->GetName(), dst, allocator);
  }

  template <typename Allocator>
  static void add_function(const asIScriptFunction *fn, JFrameValue &dst, Allocator &allocator)
  {
    add_member("name", fn->GetName(), dst, allocator);
    add_member("decl", fn->GetDeclaration(false), dst, allocator);
    add_type("return", fn->GetReturnTypeId(), dst, allocator);

    JFrameValue params(rapidjson::kArrayType);
    for (int j = 0; j < (int)fn->GetParamCount(); ++j)
    {
      JFrameValue paramItem(rapidjson::kObjectType);
      int typeId = -1;
      fn->GetParam(j, &typeId);
      add_type("type", typeId, paramItem, allocator);
      params.PushBack(eastl::move(paramItem), allocator);
    }
    add_member("params", params, dst, allocator);
  }

  bool save_all_bindings_to_file(const char *filename)
  {
    if (!engine)
      return false;

    std::ofstream ofs(filename, std::ofstream::out);
    if (ofs.fail())
    {
      DEBUG_LOG("Error: cannot open file: " << filename);
      return false;
    }

    JFrameDocument doc;
    auto &docObject = doc.SetObject();
    save_all_bindings_to_document(doc);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 2);
    doc.Accept(writer);
    ofs << buffer.GetString() << std::endl;

    return true;
  }

  void save_all_bindings_to_document(JFrameValue &doc)
  {
    auto &docAlloc = json_frame_allocator;

    JFrameValue funcs(rapidjson::kArrayType);
    for (int i = 0; i < (int)engine->GetGlobalFunctionCount(); ++i)
    {
      const asIScriptFunction *fn = engine->GetGlobalFunctionByIndex(i);

      JFrameValue item(rapidjson::kObjectType);
      add_function(fn, item, docAlloc);

      funcs.PushBack(eastl::move(item), docAlloc);
    }
    add_member("funcs", funcs, doc, docAlloc);

    JFrameValue types(rapidjson::kArrayType);
    for (int i = 0; i < (int)engine->GetObjectTypeCount(); ++i)
    {
      asITypeInfo *info = engine->GetObjectTypeByIndex(i);

      JFrameValue item(rapidjson::kObjectType);
      add_member("name", info->GetName(), item, docAlloc);
      add_member<int>("flags", info->GetFlags(), item, docAlloc);

      JFrameValue props(rapidjson::kArrayType);
      for (int j = 0; j < (int)info->GetPropertyCount(); ++j)
      {
        JFrameValue paramItem(rapidjson::kObjectType);
        const char *name = nullptr;
        int typeId = -1;
        info->GetProperty(j, &name, &typeId);
        add_member("name", name, paramItem, docAlloc);
        add_type("type", typeId, paramItem, docAlloc);
        props.PushBack(eastl::move(paramItem), docAlloc);
      }
      add_member("props", props, item, docAlloc);

      JFrameValue methods(rapidjson::kArrayType);
      for (int j = 0; j < (int)info->GetMethodCount(); ++j)
      {
        const asIScriptFunction *fn = info->GetMethodByIndex(j);

        JFrameValue methodItem(rapidjson::kObjectType);
        add_function(fn, methodItem, docAlloc);

        methods.PushBack(eastl::move(methodItem), docAlloc);
      }
      add_member("methods", methods, item, docAlloc);

      types.PushBack(eastl::move(item), docAlloc);
    }
    add_member("types", types, doc, docAlloc);
  }

  void save_all_bindings_to_document(asIScriptModule *module, JFrameValue &doc)
  {
    auto &docAlloc = json_frame_allocator;

    JFrameValue funcs(rapidjson::kArrayType);
    for (int i = 0; i < (int)module->GetFunctionCount(); ++i)
    {
      const asIScriptFunction *fn = module->GetFunctionByIndex(i);

      JFrameValue item(rapidjson::kObjectType);
      add_function(fn, item, docAlloc);

      funcs.PushBack(eastl::move(item), docAlloc);
    }
    add_member("funcs", funcs, doc, docAlloc);

    JFrameValue types(rapidjson::kArrayType);
    for (int i = 0; i < (int)module->GetObjectTypeCount(); ++i)
    {
      asITypeInfo *info = module->GetObjectTypeByIndex(i);

      JFrameValue item(rapidjson::kObjectType);
      add_member("name", info->GetName(), item, docAlloc);
      add_member<int>("flags", info->GetFlags(), item, docAlloc);

      JFrameValue props(rapidjson::kArrayType);
      for (int j = 0; j < (int)info->GetPropertyCount(); ++j)
      {
        JFrameValue paramItem(rapidjson::kObjectType);
        const char *name = nullptr;
        int typeId = -1;
        info->GetProperty(j, &name, &typeId);
        add_member("name", name, paramItem, docAlloc);
        add_type("type", typeId, paramItem, docAlloc);
        props.PushBack(eastl::move(paramItem), docAlloc);
      }
      add_member("props", props, item, docAlloc);

      JFrameValue methods(rapidjson::kArrayType);
      for (int j = 0; j < (int)info->GetMethodCount(); ++j)
      {
        const asIScriptFunction *fn = info->GetMethodByIndex(j);

        JFrameValue methodItem(rapidjson::kObjectType);
        add_function(fn, methodItem, docAlloc);

        methods.PushBack(eastl::move(methodItem), docAlloc);
      }
      add_member("methods", methods, item, docAlloc);

      types.PushBack(eastl::move(item), docAlloc);
    }
    add_member("types", types, doc, docAlloc);
  }

  static void json_message(const asSMessageInfo *msg, void *param)
  {
    JFrameValue &messages = *(JFrameDocument*)param;

    const char *type = "error";
    if (msg->type == asMSGTYPE_WARNING)
      type = "warning";
    else if (msg->type == asMSGTYPE_INFORMATION)
      type = "info";

    JFrameValue m(rapidjson::kObjectType);

    add_member("type", type, m, json_frame_allocator);
    add_member("section", msg->section, m, json_frame_allocator);
    add_member("row", msg->row, m, json_frame_allocator);
    add_member("col", msg->col, m, json_frame_allocator);
    add_member("message", msg->message, m, json_frame_allocator);

    messages.PushBack(eastl::move(m), json_frame_allocator);
  }

  bool inspect_module(const char *name, const char *path, JFrameDocument &out_result)
  {
    ASSERT(engine != nullptr);
    ASSERT(name && name[0]);

    JFrameValue messages(rapidjson::kArrayType);
    engine->SetMessageCallback(asFUNCTION(json_message), &messages, asCALL_CDECL);
    out_result.AddMember("messages", eastl::move(messages), json_frame_allocator);

    // TODO: Error codes

    CScriptBuilder builder;
    int r = builder.StartNewModule(engine, name);
    if (r < 0)
      return false;
    builder.AddSectionFromFile(path);
    if (r < 0)
      return false;
    builder.BuildModule();
    if (r < 0)
      return false;

    if (asIScriptModule *module = engine->GetModule(name))
    {
      JFrameValue moduleObj(rapidjson::kObjectType);
      save_all_bindings_to_document(module, moduleObj);
      out_result.AddMember("script", eastl::move(moduleObj), json_frame_allocator);

      engine->DiscardModule(name);
    }

    return true;
  }

  asIScriptModule* get_module(const char *name)
  {
    asIScriptModule *module = engine->GetModule(name);
    ASSERT(module != nullptr);
    return module;
  }

  asIScriptFunction* find_function_by_decl(asIScriptModule *module, const char *decl)
  {
    ASSERT(module != nullptr);
    return module->GetFunctionByDecl(decl);
  }

  asIScriptContext* find_free_context()
  {
    ASSERT(engine != nullptr);
    asIScriptContext *ctx = engine->CreateContext();
    return ctx;
  }

  bool register_component_property(const char *type, const char *decl, size_t offset)
  {
    int r = engine->RegisterObjectProperty(type, decl, offset);
    ASSERT(r >= 0);
    return r >= 0;
  }

  bool register_component_method(const char *type, const char *decl, const asSFuncPtr &f)
  {
    int r = engine->RegisterObjectMethod(type, decl, f, asCALL_THISCALL);
    ASSERT(r >= 0);
    return r >= 0;
  }

  bool register_component_function(const char *type, const char *decl, const asSFuncPtr &f)
  {
    int r = engine->RegisterObjectMethod(type, decl, f, asCALL_CDECL_OBJFIRST);
    ASSERT(r >= 0);
    return r >= 0;
  }

  bool register_function(const char *decl, const asSFuncPtr &f)
  {
    int r = engine->RegisterGlobalFunction(decl, f, asCALL_CDECL);
    ASSERT(r >= 0);
    return r >= 0;
  }

  ParamDesc get_param_desc(asIScriptFunction *fn, int i)
  {
    ParamDesc desc;

    int typeId = 0;
    const char *name = nullptr;
    fn->GetParam(i, &typeId, (asDWORD*)&desc.flags, &name);

    desc.typeId = typeId;
    desc.name = name;
    if (typeId == asTYPEID_FLOAT) desc.type = "float";
    else if (typeId == asTYPEID_INT32) desc.type = "int";
    else if (typeId == asTYPEID_BOOL) desc.type = "bool";
    else
    {
      asITypeInfo *type = engine->GetTypeInfoById(typeId);
      desc.type = type->GetName();
    }

    return eastl::move(desc);
  }

  ParamDescVector get_all_param_desc(asIScriptFunction *fn)
  {
    eastl::vector<ParamDesc> res;
    for (int i = 0; i < (int)fn->GetParamCount(); ++i)
      res.push_back(eastl::move(get_param_desc(fn, i)));
    return eastl::move(res);
  }

  namespace internal
  {
    asIScriptEngine *get_engine()
    {
      ASSERT(engine != nullptr);
      return engine;
    }

    void set_arg(asIScriptContext *ctx, size_t i, void *arg)
    {
      ctx->SetArgAddress(i, arg);
    }

    void prepare_context(asIScriptContext *ctx, asIScriptFunction *fn)
    {
      ctx->Prepare(fn);
    }

    void execute_context(asIScriptContext *ctx)
    {
      ctx->Execute();
    }

    void* get_return_address(asIScriptContext *ctx)
    {
      return ctx->GetReturnAddress();
    }

    asIScriptContext* create_context()
    {
      return engine->CreateContext();
    }

    void release_context(asIScriptContext *ctx)
    {
      ctx->Release();
    }

    void set_arg_wrapped(asIScriptContext *ctx, int i, void *data)
    {
      ASSERT(data != nullptr);
      ctx->SetArgAddress(i, data);
    }
  }
}
