#include "script.h"

#include <assert.h>

#include <iostream>
#include <regex>

#include <ecs/ecs.h>
#include <ecs/component.h>
#include <ecs/system.h>
#include <ecs/map.h>

#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/intrusive_list.h>

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
    std::cout << "[Script]: " << str << std::endl;
  }

  // static void print(const FrameMemString &str)
  // {
  //   std::cout << "[Script]: " << str.c_str() << std::endl;
  // }

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
    static const T& exec(const T& v1)
    {
      return v1;
    }
    using ExecType = const T& (*) (const T&);
    static constexpr ExecType execPtr = &opImplConv<T>::exec;
  };

  struct ScriptQuery
  {
    struct Iterator
    {
      ScriptQuery *query = nullptr;
      int pos = 0;

      Iterator& operator=(const Iterator &assign)
      {
        pos = assign.pos;
        query = assign.query;
        assert(query != nullptr);
        query->addRef();
        return *this;
      }

      bool hasNext() const
      {
        return pos < (int)query->eids.size();
      }

      void operator++()
      {
        ++pos;
      }

      void* get()
      {
        asIScriptFunction *fn = query->subType->GetFactoryByIndex(0);
        asIScriptObject *object = nullptr;
        callValue<asIScriptObject*>([&](asIScriptObject **obj)
        {
          object = *obj;
          object->AddRef();
        }, fn);

        EntityId eid = query->eids[pos];
        const auto &entity = g_mgr->entities[eid.index];
        const auto &templ = g_mgr->templates[entity.templateId];

        for (const auto &c : query->components)
        {
          for (const auto &tc : templ.components)
          {
            if (c.nameId == tc.nameId)
            {
              void *prop = object->GetAddressOfProperty(c.id);
              *(uint8_t**)prop = g_mgr->storages[tc.nameId]->getRaw(entity.componentOffsets[tc.id]);
              break;
            }
          }
        }

        return object;
      }
    };

    static void initIterator(asITypeInfo *type, void *data)
    {
    }

    static void releaseIterator(Iterator *it)
    {
      it->query->release();
    }

    eastl::vector<EntityId, FrameMemAllocator> eids;
    eastl::vector<CompDescWithAllocator<FrameMemAllocator>, FrameMemAllocator> components;

    asITypeInfo *subType = nullptr;

    int refCount = 1;

    ~ScriptQuery()
    {
      eids.reset_lose_memory();
      components.reset_lose_memory();
    }

    void addRef()
    {
      asAtomicInc(refCount);
    }

    void release()
    {
    }

    Iterator perform()
    {
      eids.clear();

      for (auto &e : g_mgr->entities)
      {
        if (!e.ready)
          continue;

        const auto &templ = g_mgr->templates[e.templateId];

        bool ok = true;
        for (const auto &c : components)
          if (c.desc->id != g_mgr->eidCompId && !templ.hasCompontent(c.desc->id, c.name.c_str()))
          {
            ok = false;
            break;
          }

        if (ok)
          eids.push_back(e.eid);
      }

      Iterator it;
      it.query = this;
      it.query->addRef();
      return it;
    }
  };

  static eastl::array<ScriptQuery, 1024> g_query_buffer;
  static size_t g_query_offset = 0;

  ScriptQuery* createScriptQuery(asITypeInfo *type, void *data)
  {
    ScriptQuery *query = new (&g_query_buffer[g_query_offset++]) ScriptQuery;

    query->subType = type->GetEngine()->GetTypeInfoById(type->GetSubTypeId());
    assert(query->subType->GetPropertyCount() != 0);
    assert(query->subType->GetFactoryCount() != 0);

    for (int i = 0; i < (int)query->subType->GetPropertyCount(); ++i)
    {
      const char *name = nullptr;
      int typeId = -1;
      query->subType->GetProperty(i, &name, &typeId);

      const char *typeName = nullptr;

      if (typeId == asTYPEID_FLOAT) typeName = "float";
      else if (typeId == asTYPEID_INT32) typeName = "int";
      else if (typeId == asTYPEID_BOOL) typeName = "bool";
      else typeName = engine->GetTypeInfoById(typeId)->GetName();

      const RegComp *desc = find_comp(typeName);
      if (!desc)
        desc = find_comp(name);
      assert(desc != nullptr);

      query->components.push_back({ i, g_mgr->getComponentNameId(name), name, desc });
    }

    // TODO: Save query for "subType" and invalidate
    return query;
  }

  // struct StringFactory : public asIStringFactory
  // {
  //   struct Node : eastl::intrusive_list_node
  //   {
  //     size_t index;
  //     eastl::string value;
  //   };

  //   size_t index = 0;
  //   eastl::queue<size_t> freeIndices;
  //   eastl::array<Node, 1024> stringsData;
  //   eastl::intrusive_list<Node> strings;

  //   virtual const void *GetStringConstant(const char *data, asUINT length)
  //   {
  //     size_t i = 0;
  //     if (freeIndices.empty())
  //       i = index++;
  //     else
  //     {
  //       i = freeIndices.front();
  //       freeIndices.pop();
  //     }

  //     Node *n = &stringsData[i];
  //     strings.push_back(*n);

  //     n->index = index - 1;
  //     n->value.assign(data, length);

  //     return n;

  //     //FrameMemString *str = new (alloc_frame_mem(sizeof(FrameMemString))) FrameMemString(data, length);
  //     //return str;
  //   }

  //   virtual int ReleaseStringConstant(const void *str)
  //   {
  //     if (str == nullptr)
  //       return asERROR;

  //     freeIndices.push(((Node*)str)->index);
  //     strings.remove(*(Node*)str);

  //     return asSUCCESS;
  //   }

  //   virtual int GetRawStringData(const void *str, char *data, asUINT *length) const
  //   {
  //     if (str == nullptr)
  //       return asERROR;

  //     if (length)
  //       *length = ((Node*)str)->value.length();

  //     if (data)
  //       ::memcpy(data, ((Node*)str)->value.data(), ((Node*)str)->value.length());

  //     return asSUCCESS;
  //   }
  // }
  // string_factory;

  // static StringFactory::Node* alloc_string()
  // {
  //   return new (alloc_frame_mem(sizeof(StringFactory::Node))) StringFactory::Node;
  // }

  // static void construct_string(StringFactory::Node *thisPointer)
  // {
  //   new (thisPointer) StringFactory::Node();
  // }

  // static void copy_construct_string(const StringFactory::Node &other, StringFactory::Node *thisPointer)
  // {
  //   new (thisPointer) StringFactory::Node(other);
  // }

  // static void destruct_string(StringFactory::Node *thisPointer)
  // {
  //   // thisPointer->~string();
  // }

  // static FrameMemString add_string_float(const FrameMemString &str, float f)
  // {
  //   FrameMemString* res = alloc_string();
  //   res->append_sprintf("%s%f", str.c_str(), f);
  //   return *res;
  // }

  // static FrameMemString add_float_string(float f, const FrameMemString &str)
  // {
  //   FrameMemString* res = alloc_string();
  //   res->append_sprintf("%f%s", f, str.c_str());
  //   return *res;
  // }

  // static FrameMemString add_string_string(const FrameMemString &a, const FrameMemString &b)
  // {
  //   FrameMemString* res = alloc_string();
  //   res->append_sprintf("%s%s", a.c_str(), b.c_str());
  //   return *res;
  // }

  bool init()
  {
    engine = asCreateScriptEngine();

    engine->SetMessageCallback(asFUNCTION(message), 0, asCALL_CDECL);

    RegisterScriptHandle(engine);
    RegisterScriptArray(engine, true);
    RegisterStdString(engine);
    RegisterStdStringUtils(engine);
    RegisterScriptMath(engine);
    RegisterScriptDictionary(engine);

    // engine->RegisterObjectType("string", sizeof(FrameMemString), asOBJ_VALUE | asGetTypeTraits<FrameMemString>());
    // engine->RegisterStringFactory("string", &string_factory);

    // engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f()", asFUNCTION(construct_string), asCALL_CDECL_OBJLAST);
    // engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f(const string &in)", asFUNCTION(copy_construct_string), asCALL_CDECL_OBJLAST);
    // engine->RegisterObjectBehaviour("string", asBEHAVE_DESTRUCT,   "void f()", asFUNCTION(destruct_string),  asCALL_CDECL_OBJLAST);

    // engine->RegisterObjectMethod("string", "string opAdd(const string &in) const", asFUNCTIONPR(add_string_string, (const FrameMemString &, const FrameMemString &), FrameMemString), asCALL_CDECL_OBJFIRST);

    // engine->RegisterObjectMethod("string", "string opAdd(float) const", asFUNCTION(add_string_float), asCALL_CDECL_OBJFIRST);
    // engine->RegisterObjectMethod("string", "string opAdd_r(float) const", asFUNCTION(add_float_string), asCALL_CDECL_OBJLAST);

    engine->RegisterObjectType("QueryIterator<class T>", sizeof(ScriptQuery::Iterator), asOBJ_VALUE | asOBJ_TEMPLATE | asGetTypeTraits<ScriptQuery::Iterator>());
    engine->RegisterObjectBehaviour("QueryIterator<T>", asBEHAVE_CONSTRUCT, "void f(int&in)", asFUNCTIONPR(ScriptQuery::initIterator, (asITypeInfo*, void*), void), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("QueryIterator<T>", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR(ScriptQuery::releaseIterator, (ScriptQuery::Iterator*), void), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("QueryIterator<T>", "QueryIterator<T>& opAssign(const QueryIterator<T>&in)", asMETHODPR(ScriptQuery::Iterator, operator=, (const ScriptQuery::Iterator&), ScriptQuery::Iterator&), asCALL_THISCALL);
    engine->RegisterObjectMethod("QueryIterator<T>", "bool hasNext() const", asMETHODPR(ScriptQuery::Iterator, hasNext, () const, bool), asCALL_THISCALL);
    engine->RegisterObjectMethod("QueryIterator<T>", "void opPreInc()", asMETHODPR(ScriptQuery::Iterator, operator++, (), void), asCALL_THISCALL);
    engine->RegisterObjectMethod("QueryIterator<T>", "T@ get()", asMETHODPR(ScriptQuery::Iterator, get, (), void*), asCALL_THISCALL);

    engine->RegisterObjectType("Query<class T>", sizeof(ScriptQuery), asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT);
    engine->RegisterObjectBehaviour("Query<T>", asBEHAVE_FACTORY, "Query<T>@ f(int&in)", asFUNCTIONPR(createScriptQuery, (asITypeInfo*, void*), ScriptQuery*), asCALL_CDECL);
    engine->RegisterObjectMethod("Query<T>", "QueryIterator<T> perform()", asMETHODPR(ScriptQuery, perform, (), ScriptQuery::Iterator), asCALL_THISCALL);

    // engine->RegisterObjectType("Map", sizeof(FrameMemMap), asOBJ_REF | asOBJ_NOCOUNT);
    // engine->RegisterObjectMethod("Map", "void set(string&in, int&in) const", asMETHODPR(Map::Iterator, hasNext, () const, bool), asCALL_THISCALL);

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
    assert(r >= 0);

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
    assert(engine != nullptr);
    assert(name && name[0]);

    if (engine->GetModule(name) != nullptr)
    {
      engine->DiscardModule(name);
    }

    CScriptBuilder builder;
    int r = builder.StartNewModule(engine, name);
    assert(r >= 0);
    r = builder.AddSectionFromFile(path);
    assert(r >= 0);
    r = builder.BuildModule();
    assert(r >= 0);

    asIScriptModule *module = engine->GetModule(name);
    assert(module != nullptr);

    if (module)
      callback(builder, *module);

    return module != nullptr;
  }

  asIScriptModule* get_module(const char *name)
  {
    asIScriptModule *module = engine->GetModule(name);
    assert(module != nullptr);
    return module;
  }

  asIScriptFunction* find_function_by_decl(asIScriptModule *module, const char *decl)
  {
    assert(module != nullptr);
    asIScriptFunction *fn = module->GetFunctionByDecl(decl);
    assert(fn != nullptr);
    return fn;
  }

  asIScriptContext* find_free_context()
  {
    assert(engine != nullptr);
    asIScriptContext *ctx = engine->CreateContext();
    return ctx;
  }

  bool register_component_property(const char *type, const char *decl, size_t offset)
  {
    int r = engine->RegisterObjectProperty(type, decl, offset);
    assert(r >= 0);
    return r >= 0;
  }

  bool register_component_method(const char *type, const char *decl, const asSFuncPtr &f)
  {
    int r = engine->RegisterObjectMethod(type, decl, f, asCALL_THISCALL);
    assert(r >= 0);
    return r >= 0;
  }

  bool register_component_function(const char *type, const char *decl, const asSFuncPtr &f)
  {
    int r = engine->RegisterObjectMethod(type, decl, f, asCALL_CDECL_OBJFIRST);
    assert(r >= 0);
    return r >= 0;
  }

  bool register_function(const char *decl, const asSFuncPtr &f)
  {
    int r = engine->RegisterGlobalFunction(decl, f, asCALL_CDECL);
    assert(r >= 0);
    return r >= 0;
  }

  ParamDesc get_param_desc(asIScriptFunction *fn, int i)
  {
    ParamDesc desc;

    int typeId = 0;
    const char *name = nullptr;
    fn->GetParam(i, &typeId, (asDWORD*)&desc.flags, &name);

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

  void clear_frame_mem_data()
  {
    for (size_t i = 0; i < g_query_offset; ++i)
      g_query_buffer[i].~ScriptQuery();
    g_query_offset = 0;
  }

  namespace internal
  {
    asIScriptEngine *get_engine()
    {
      assert(engine != nullptr);
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
      assert(data != nullptr);
      ctx->SetArgAddress(i, data);
    }
  }
}
