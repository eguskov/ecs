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

MAKE_TYPE_FACTORY(eastl_string, eastl::string);
MAKE_TYPE_FACTORY(ComponentsMap, ::ComponentsMap);

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
};