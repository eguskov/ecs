#pragma once

#include <ecs/debug.h>

#include <EASTL/vector.h>
#include <EASTL/array.h>
#include <EASTL/bitset.h>
#include <EASTL/fixed_vector.h>

#include <EASTL/string.h>

#include <angelscript.h>

struct RegComp;
class CScriptBuilder;

namespace script
{
  namespace internal
  {
    void set_arg(asIScriptContext *ctx, size_t i, void *arg);

    template <size_t D, size_t ... I>
    void set_args(asIScriptContext *ctx, eastl::index_sequence<I...>) {}

    template <size_t D, typename Head, typename... Tail, size_t ... I>
    void set_args(asIScriptContext *ctx, eastl::index_sequence<I...>, Head &&head, Tail&&... tail)
    {
      set_arg(ctx, D, (void*)head);
      set_args<D + 1>(ctx, eastl::make_index_sequence<sizeof...(I) - 1>{}, eastl::forward<Tail>(tail)...);
    }

    template <typename... Args>
    void set_args(asIScriptContext *ctx, Args&&... args)
    {
      set_args<0>(ctx, eastl::make_index_sequence<sizeof...(Args)>{}, eastl::forward<Args>(args)...);
    }

    asIScriptEngine *get_engine();
    asIScriptContext* create_context();

    void set_arg_wrapped(asIScriptContext *ctx, int i, void *data);
  }

  struct ScopeContext
  {
    asIScriptContext *ctx = nullptr;
    ScopeContext() : ctx(script::internal::create_context()) {}
    ~ScopeContext() { ctx->Release(); }
    operator asIScriptContext*() { return ctx; }
    asIScriptContext* operator->() { return ctx; }
  };

  struct TypeId
  {
    uint32_t id = 0;
    uint32_t baseId = 0;
    TypeId() = default;
    TypeId(uint32_t _id) : id(_id), baseId(_id & asTYPEID_MASK_SEQNBR) {}

    void operator=(uint32_t _id) { id = _id; baseId = _id & asTYPEID_MASK_SEQNBR; }
    bool operator==(const TypeId &rhs) const { return baseId == rhs.baseId; }
    operator uint32_t() const { return baseId; }
  };

  struct ParamDesc
  {
    eastl::string name;
    eastl::string type;
    TypeId typeId;
    uint32_t flags = 0;

    const char* getTypeName() const
    {
      if (type == "real")
        return "float";
      else if (type == "boolean")
        return "bool";
      return type.c_str();
    }
  };

  using ParamDescVector = eastl::vector<ParamDesc>;

  bool init();
  void release();

  bool build_module(const char *name, const char *path, const eastl::function<void (CScriptBuilder&, asIScriptModule&)> &callback);

  asIScriptModule* get_module(const char *name);
  asIScriptFunction* find_function_by_decl(asIScriptModule *module, const char *decl);

  template <typename... Args>
  void call(asIScriptContext *ctx, asIScriptFunction *fn, Args&&... args)
  {
    ctx->Prepare(fn);
    internal::set_args(ctx, eastl::forward<Args>(args)...);
    ctx->Execute();
  }

  template <typename... Args>
  void call(asIScriptContext *ctx, void *user_data, asIScriptFunction *fn, Args&&... args)
  {
    ctx->Prepare(fn);
    ctx->SetUserData(user_data, 1000);
    internal::set_args(ctx, eastl::forward<Args>(args)...);
    ctx->Execute();
  }

  template <typename... Args>
  void call(asIScriptFunction *fn, Args&&... args)
  {
    ScopeContext ctx;
    call(ctx, fn, eastl::forward<Args>(args)...);
  }

  template <typename... Args>
  void call(void *user_data, asIScriptFunction *fn, Args&&... args)
  {
    ScopeContext ctx;
    call(ctx, user_data, fn, eastl::forward<Args>(args)...);
  }

  template <typename R, typename C, typename... Args>
  void call(const C &callback, asIScriptFunction *fn, Args&&... args)
  {
    ScopeContext ctx;
    call(ctx, fn, eastl::forward<Args>(args)...);
    callback((R*)ctx->GetReturnAddress());
  }

  template <typename R, typename C, typename... Args>
  void callValue(const C &callback, asIScriptFunction *fn, Args&&... args)
  {
    ScopeContext ctx;
    call(ctx, fn, eastl::forward<Args>(args)...);
    callback((R*)ctx->GetAddressOfReturnValue());
  }

  template <typename T>
  bool register_component(const char *type)
  {
    eastl::string factoryDecl = type;
    factoryDecl += "@ f()";

    int r = internal::get_engine()->RegisterObjectType(type, 0, asOBJ_REF | asOBJ_NOCOUNT);
    ASSERT(r >= 0);
    r = internal::get_engine()->RegisterObjectBehaviour(type, asBEHAVE_FACTORY, factoryDecl.c_str(), asFUNCTION(RawAllocator<T>::alloc), asCALL_CDECL);
    ASSERT(r >= 0);

    return true;
  }

  bool register_component_property(const char *type, const char *decl, size_t offset);
  bool register_component_function(const char *type, const char *decl, const asSFuncPtr &f);
  bool register_component_method(const char *type, const char *decl, const asSFuncPtr &f);
  bool register_function(const char *decl, const asSFuncPtr &f);

  ParamDesc get_param_desc(asIScriptFunction *fn, int i);
  ParamDescVector get_all_param_desc(asIScriptFunction *fn);

  void clear_frame_mem_data();
}

namespace eastl
{
  template <> struct hash<script::TypeId>
  {
    size_t operator()(script::TypeId val) const { return static_cast<uint32_t>(val); }
  };
};