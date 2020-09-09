#pragma once

#include <ecs/ecs.h>
#include <ecs/autoBind.h>

#include <daScript/daScript.h>

struct EASTLStringAnnotation : das::TypeAnnotation
{
  EASTLStringAnnotation() : das::TypeAnnotation("eastl_string", "eastl::string")
  {
  }

  virtual bool rtti_isHandledTypeAnnotation() const override { return true; }
  virtual bool isRefType() const override { return true; }
  virtual bool isLocal() const override { return false; }
  virtual void walk (das::DataWalker & dw, void * p ) override
  {
    auto pstr = (eastl::string *)p;
    if (dw.reading) {
      char * pss = nullptr;
      dw.String(pss);
      *pstr = pss;
    } else {
      char * pss = (char *) pstr->c_str();
      dw.String(pss);
    }
  }
  virtual bool canCopy() const override { return false; }
  virtual bool canMove() const override { return false; }
  virtual bool canClone() const override { return true; }
  virtual size_t getAlignOf() const override { return alignof(eastl::string);}
  virtual size_t getSizeOf() const override { return sizeof(eastl::string);}
  virtual das::SimNode * simulateClone (das::Context & context, const das::LineInfo & at, das::SimNode * l, das::SimNode * r ) const override
  {
    return context.code->makeNode<das::SimNode_CloneRefValueT<eastl::string>>(at, l, r);
  }
};

struct EcsContext final : das::Context
{
  EcsContext(uint32_t stack_size) : das::Context(stack_size) {}

  void to_out(const char *message) override
  {
    if (message)
      std::cout << "[das]: " << message << std::endl;
  }

  void to_err(const char *message) override
  {
    if (message)
      std::cerr << "[dasError]: "  << message << std::endl;
  }
};

struct EcsModuleGroupData final : das::ModuleGroupUserData
{
  eastl::hash_map<eastl::string, SystemId> dasSystems;
  eastl::vector<QueryId> dasQueries;

  EcsModuleGroupData() : das::ModuleGroupUserData("ecs") {}
};

struct DasQueryData : QueryUserData
{
  das::Context *ctx = nullptr;
  das::SimFunction *fn = nullptr;
  QueryId queryId;
  eastl::vector<bool> isComponentPointer;
  eastl::vector<int> stride;
};

MAKE_TYPE_FACTORY(eastl_string, eastl::string);
MAKE_TYPE_FACTORY(ComponentsMap, ::ComponentsMap);
MAKE_TYPE_FACTORY(Query, ::Query);

MAKE_EXTERNAL_TYPE_FACTORY(EntityId, ::EntityId);

namespace das
{
  template <>
  struct cast<EntityId>
  {
    static __forceinline EntityId to(vec4f x)   { return EntityId(ecs_handle_t(v_extract_xi(v_cast_vec4i(x)))); }
    static __forceinline vec4f from(EntityId x) { return v_cast_vec4f(v_splatsi(ecs_handle_t(x))); }
  };

  template<>
  struct SimPolicy<EntityId>
  {
    static __forceinline auto to ( vec4f a ) { return das::cast<EntityId>::to(a); }
    static __forceinline bool Equ     ( vec4f a, vec4f b, Context & ) { return to(a) == to(b); }
    static __forceinline bool NotEqu  ( vec4f a, vec4f b, Context & ) { return to(a) != to(b); }
    static __forceinline bool BoolNot ( vec4f a, Context & ) { return !to(a); }
    // and for the AOT, since we don't have vec4f c-tor
    static __forceinline bool Equ     ( EntityId a, EntityId b, Context & ) { return a == b; }
    static __forceinline bool NotEqu  ( EntityId a, EntityId b, Context & ) { return a != b; }
    static __forceinline bool BoolNot ( EntityId a, Context & ) { return !a; }
  };

  #define MAKE_TYPE_FACTORY_ALIAS(TYPE, DAS_DECL_TYPE, DAS_TYPE)\
    template <> struct typeFactory<TYPE>\
    {\
      static TypeDeclPtr make(const ModuleLibrary &)\
      {\
        auto t = make_smart<TypeDecl>(Type::DAS_DECL_TYPE);\
        t->alias = #TYPE;\
        t->aotAlias = true;\
        return t;\
      }\
    };\
    template <> struct typeName<TYPE> { static string name() { return #TYPE; } };\
    template <> struct das_alias<TYPE> : das::das_alias_vec<TYPE, DAS_TYPE> {};\

  MAKE_TYPE_FACTORY_ALIAS(glm::vec4, tFloat4, float4)
  MAKE_TYPE_FACTORY_ALIAS(glm::vec3, tFloat3, float3)
  MAKE_TYPE_FACTORY_ALIAS(glm::vec2, tFloat2, float2)

  #undef MAKE_TYPE_FACTORY_ALIAS

  template<> struct ToBasicType<glm::vec4>        { enum { type = Type::tFloat4 }; };
  template<> struct cast <glm::vec4>  : cast_fVec<glm::vec4> {};

  template<> struct ToBasicType<glm::vec3>        { enum { type = Type::tFloat3 }; };
  template<> struct cast <glm::vec3>  : cast_fVec<glm::vec3> {};

  template<> struct ToBasicType<glm::vec2>        { enum { type = Type::tFloat2 }; };
  template<> struct cast <glm::vec2>  : cast_fVec_half<glm::vec2> {};
};