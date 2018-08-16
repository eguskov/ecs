#pragma once

#include <EASTL/vector.h>
#include <EASTL/string.h>

#include <angelscript.h>

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

    asIScriptContext* create_context();

    bool register_struct(const char *type, int size, uint32_t flags);
  }

  struct ScopeContext
  {
    asIScriptContext *ctx = nullptr;
    ScopeContext() : ctx(script::internal::create_context()) {}
    ~ScopeContext() { ctx->Release(); }
    operator asIScriptContext*() { return ctx; }
    asIScriptContext* operator->() { return ctx; }
  };

  struct ParamDesc
  {
    eastl::string name;
    eastl::string type;
    uint32_t flags = 0;
  };

  using ParamDescVector = eastl::vector<ParamDesc>;

  bool init();
  void release();

  asIScriptModule* build_module(const char *name, const char *path);
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
  void call(asIScriptFunction *fn, Args&&... args)
  {
    ScopeContext ctx;
    call(ctx, fn, eastl::forward<Args>(args)...);
  }

  template <typename R, typename C, typename... Args>
  void call(const C &callback, asIScriptFunction *fn, Args&&... args)
  {
    ScopeContext ctx;
    call(ctx, fn, eastl::forward<Args>(args)...);
    callback((R*)ctx->GetReturnAddress());
  }

  template <typename T>
  bool register_struct(const char *type)
  {
    return internal::register_struct(type, sizeof(T), asGetTypeTraits<T>());
  }

  bool register_struct_property(const char *type, const char *decl, size_t offset);

  ParamDesc get_param_desc(asIScriptFunction *fn, int i);
  ParamDescVector get_all_param_desc(asIScriptFunction *fn);
}