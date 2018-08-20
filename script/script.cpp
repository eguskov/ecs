#include "script.h"

#include <assert.h>

#include <iostream>

#include <ecs/component.h>

#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scripthandle/scripthandle.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptmath/scriptmath.h>

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

  static asIScriptEngine *engine = nullptr;

  static eastl::hash_map<int, IScriptHelper*> helperMap;

  using SBool = ScriptHelperDesc<bool, 1024>;
  using SFloat = ScriptHelperDesc<float, 1024>;
  using SVec2 = ScriptHelperDesc<glm::vec2, 1024>;
  using SVec3 = ScriptHelperDesc<glm::vec3, 1024>;

  template<typename D, typename T>
  static typename D::Wrapper opAdd(const T& v1, const T& v2)
  {
    return D::Helper::create(v1 + v2);
  }

  template<typename D, typename T>
  static typename D::Wrapper opSub(const T& v1, const T& v2)
  {
    return D::Helper::create(v1 - v2);
  }

  template<typename T>
  static float opMul(const T& v1, const T& v2)
  {
    return glm::dot(v1, v2);
  }

  template <typename T>
  static float opLength(const T& v1)
  {
    return glm::length(v1);
  }

  template<typename D, typename T>
  static typename D::Wrapper opMulScalar(const T &v1, float s)
  {
    return D::Helper::create(v1 * s);
  }

  template <typename T>
  static T& opAssign(T &v1, const T &v2)
  {
    v1 = v2;
    return v1;
  }

  template <typename T>
  static const T& opImplConv(const T &v1)
  {
    return v1;
  }

  template <typename D, typename T>
  static typename D::Wrapper opNeg(const T &v1)
  {
    return D::Helper::create(-v1);
  }

  template <typename D, typename T>
  static typename D::Wrapper opNormalize(const T &v1)
  {
    return D::Helper::create(glm::normalize(v1));
  }

  bool init()
  {
    engine = asCreateScriptEngine();

    engine->SetMessageCallback(asFUNCTION(message), 0, asCALL_CDECL);

    RegisterScriptHandle(engine);
    RegisterScriptArray(engine, true);
    RegisterStdString(engine);
    RegisterStdStringUtils(engine);
    RegisterScriptMath(engine);

    register_component<SBool>("boolean", find_comp("bool"));
    register_component_property("boolean", "bool v", 0);
    register_component_function("boolean", "boolean& opAssign(const boolean&in)", asFUNCTIONPR((opAssign<bool>), (bool&, const bool&), bool&));
    register_component_function("boolean", "boolean& opAssign(const bool&in)", asFUNCTIONPR((opAssign<bool>), (bool&, const bool&), bool&));
    register_component_function("boolean", "bool opImplConv() const", asFUNCTIONPR((opImplConv<bool>), (const bool&), const bool&));

    register_component<SFloat>("real", find_comp("float"));
    register_component_property("real", "float v", 0);
    register_component_function("real", "real& opAssign(const real&in)", asFUNCTIONPR((opAssign<float>), (float&, const float&), float&));
    register_component_function("real", "real& opAssign(const float&in)", asFUNCTIONPR((opAssign<float>), (float&, const float&), float&));
    register_component_function("real", "real@ opNeg()", asFUNCTIONPR((opNeg<SFloat, float>), (const float&), SFloat::Wrapper));

    register_component<SVec2>("vec2", find_comp("vec2"));
    register_component_property("vec2", "float x", offsetof(glm::vec2, x));
    register_component_property("vec2", "float y", offsetof(glm::vec2, y));
    register_component_function("vec2", "vec2& opAssign(const vec2&in)", asFUNCTIONPR((opAssign<glm::vec2>), (glm::vec2&, const glm::vec2&), glm::vec2&));
    register_component_function("vec2", "vec2@ opAdd(const vec2&in) const", asFUNCTIONPR((opAdd<SVec2, glm::vec2>), (const glm::vec2&, const glm::vec2&), SVec2::Wrapper));
    register_component_function("vec2", "vec2@ opSub(const vec2&in) const", asFUNCTIONPR((opSub<SVec2, glm::vec2>), (const glm::vec2&, const glm::vec2&), SVec2::Wrapper));
    register_component_function("vec2", "float opMul(const vec2&in) const", asFUNCTIONPR((opMul<glm::vec2>), (const glm::vec2&, const glm::vec2&), float));
    register_component_function("vec2", "vec2@ opMul(float) const", asFUNCTIONPR((opMulScalar<SVec2, glm::vec2>), (const glm::vec2&, float), SVec2::Wrapper));
    register_component_function("vec2", "vec2@ opMul_r(float) const", asFUNCTIONPR((opMulScalar<SVec2, glm::vec2>), (const glm::vec2&, float), SVec2::Wrapper));
    register_component_function("vec2", "vec2@ opNeg() const", asFUNCTIONPR((opNeg<SVec2, glm::vec2>), (const glm::vec2&), SVec2::Wrapper));
    register_function("float dot(const vec2&in, const vec2&in)", asFUNCTIONPR((opMul<glm::vec2>), (const glm::vec2&, const glm::vec2&), float));
    register_function("float length(const vec2&in)", asFUNCTIONPR((opLength<glm::vec2>), (const glm::vec2&), float));
    register_function("vec2@ normalize(const vec2&in)", asFUNCTIONPR((opNormalize<SVec2, glm::vec2>), (const glm::vec2&), SVec2::Wrapper));

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

  asIScriptModule* build_module(const char *name, const char *path)
  {
    assert(engine != nullptr);

    char *buffer = nullptr;

    FILE *file = nullptr;
    ::fopen_s(&file, path, "rb");
    if (file)
    {
      size_t sz = ::ftell(file);
      ::fseek(file, 0, SEEK_END);
      sz = ::ftell(file) - sz;
      ::fseek(file, 0, SEEK_SET);

      buffer = new char[sz + 1];
      buffer[sz] = '\0';
      ::fread(buffer, 1, sz, file);
      ::fclose(file);
    }
    assert(buffer != nullptr);
    if (!buffer)
      return nullptr;

    asIScriptModule *module = engine->GetModule(name, asGM_ALWAYS_CREATE);
    module->AddScriptSection(name, buffer, ::strlen(buffer));
    module->Build();

    delete[] buffer;

    // For compile test
    // std::cin.get();

    return module;
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

    void register_struct_helper(IScriptHelper *helper)
    {
      if (!helper->desc)
        return;

      auto res = helperMap.insert(helper->desc->id);
      assert(res.second);

      res.first->second = helper;
    }

    void set_arg_wrapped(asIScriptContext *ctx, int i, int comp_id, void *data)
    {
      assert(data != nullptr);

      auto res = helperMap.find(comp_id);
      ctx->SetArgAddress(i, res == helperMap.end() ? data : res->second->wrapObject(data));
    }

    void set_arg_wrapped(asIScriptContext *ctx, int i, const RegComp *desc, void *data)
    {
      assert(desc != nullptr);
      set_arg_wrapped(ctx, i, desc->id, data);
    }
  }
}
