#pragma once

#include <assert.h>

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
  struct IScriptHelper;

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

    void register_struct_helper(IScriptHelper *helper);
    void set_arg_wrapped(asIScriptContext *ctx, int i, int comp_id, void *data);
    void set_arg_wrapped(asIScriptContext *ctx, int i, const RegComp *desc, void *data);
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

  asIScriptModule* build_module(const char *name, const char *path, const eastl::function<void (CScriptBuilder&, asIScriptModule*)> &callback);
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

  template <typename R, typename C, typename... Args>
  void callValue(const C &callback, asIScriptFunction *fn, Args&&... args)
  {
    ScopeContext ctx;
    call(ctx, fn, eastl::forward<Args>(args)...);
    callback((R*)ctx->GetAddressOfReturnValue());
  }

  struct IScriptHelper
  {
    const RegComp *desc = nullptr;
    virtual void* wrapObject(void *object) = 0;
  };

  template <typename Desc, size_t MaxInstanceCount> struct ScriptHelper;

  template <typename T, size_t _MaxInstanceCount>
  struct ScriptHelperDesc
  {
    using Self = ScriptHelperDesc<T, _MaxInstanceCount>;
    using Object = T;
    using Helper = ScriptHelper<Self, _MaxInstanceCount>;
    using Wrapper = typename eastl::add_pointer<typename Helper::Wrapper>::type;

    constexpr static size_t MaxInstanceCount = _MaxInstanceCount;
  };

  template <typename Desc, size_t MaxInstanceCount>
  struct ScriptHelper : IScriptHelper
  {
    using Helper = ScriptHelper<Desc, MaxInstanceCount>;

    struct Wrapper
    {
      // Must be the first, because it is written from script
      typename Desc::Object object;
      typename Desc::Object *refObject = nullptr;

      int id = 0;
      int refCount = 1;

      Helper* helper = nullptr;

      void addRef()
      {
        asAtomicInc(refCount);
      }

      void release()
      {
#ifdef _DEBUG
        assert(helper->magic == 0xdeadbeaf);
#endif
        if (asAtomicDec(refCount) == 0)
        {
          assert(refCount >= 0);
          if (refObject)
            *refObject = object;
          refObject = nullptr;
          helper->release(this);
        }
      }
    };

#ifdef _DEBUG
    uint32_t magic = 0xdeadbeaf;
#endif
    eastl::bitset<MaxInstanceCount> freeMask;
    eastl::array<Wrapper, MaxInstanceCount> storage;

    ScriptHelper()
    {
      freeMask.set();
    }

    void release(Wrapper *w)
    {
      freeMask.set(w->id);
    }

    void* wrapObject(void *object) override final
    {
      return Helper::createRef(*(typename Desc::Object*)object);
    }

    static Helper& instance()
    {
      static Helper helper;
      return helper;
    }

    static Wrapper* wrapperInstance()
    {
      Helper &helper = instance();

      int id = -1;
      Wrapper *w = nullptr;
      for (int i = 0; i < MaxInstanceCount; ++i)
        if (helper.freeMask[i])
        {
          id = i;
          w = &helper.storage[i];
          break;
        }

      assert(w != nullptr && id >= 0);
      assert(w->refCount <= 1);

      helper.freeMask.reset(id);

      w->id = id;
      w->refCount = 1;
      w->helper = &helper;
      new (&w->object) typename Desc::Object();
      return w;
    }

    static Wrapper* create(const typename Desc::Object &v)
    {
      Wrapper *w = wrapperInstance();
      w->object = v;
      return w;
    }

    static Wrapper* createRef(typename Desc::Object &v)
    {
      Wrapper *w = wrapperInstance();
      w->object = v;
      w->refObject = &v;
      return w;
    }
  };

  template <typename Desc>
  bool register_component(const char *type, const RegComp *desc = nullptr)
  {
    using ScriptHelperT = ScriptHelper<Desc, Desc::MaxInstanceCount>;

    auto &helper = ScriptHelperT::instance();
    helper.desc = desc;
    assert(desc != nullptr);

    internal::register_struct_helper(&helper);

    eastl::string factoryDecl = type;
    factoryDecl += "@ f()";

    int r = internal::get_engine()->RegisterObjectType(type, 0, asOBJ_REF);
    assert(r >= 0);
    r = internal::get_engine()->RegisterObjectBehaviour(type, asBEHAVE_FACTORY, factoryDecl.c_str(), asFUNCTION(ScriptHelperT::wrapperInstance), asCALL_CDECL);
    assert(r >= 0);
    r = internal::get_engine()->RegisterObjectBehaviour(type, asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptHelperT::Wrapper, addRef), asCALL_THISCALL); assert(r >= 0);
    assert(r >= 0);
    r = internal::get_engine()->RegisterObjectBehaviour(type, asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptHelperT::Wrapper, release), asCALL_THISCALL); assert(r >= 0);
    assert(r >= 0);

    return true;
  }

  bool register_component_property(const char *type, const char *decl, size_t offset);
  bool register_component_function(const char *type, const char *decl, const asSFuncPtr &f);
  bool register_component_method(const char *type, const char *decl, const asSFuncPtr &f);
  bool register_function(const char *decl, const asSFuncPtr &f);

  ParamDesc get_param_desc(asIScriptFunction *fn, int i);
  ParamDescVector get_all_param_desc(asIScriptFunction *fn);
}