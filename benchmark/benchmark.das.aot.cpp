#include "daScript/misc/platform.h"

#include "daScript/simulate/simulate.h"
#include "daScript/simulate/aot.h"
#include "daScript/simulate/aot_library.h"

 // require builtin

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4100)   // unreferenced formal parameter
#pragma warning(disable:4189)   // local variable is initialized but not referenced
#pragma warning(disable:4244)   // conversion from 'int32_t' to 'float', possible loss of data
#pragma warning(disable:4114)   // same qualifier more than once
#pragma warning(disable:4623)   // default constructor was implicitly defined as deleted
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wwritable-strings"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunsequenced"
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace das {
namespace {




inline bool test_30169ea2890b661e ( Context * __context__, float3 & __pos_rename_at_4, float3 const  & __vel_rename_at_4 );

void __init_script ( Context * __context__, bool __init_shared )
{
}

inline bool test_30169ea2890b661e ( Context * __context__, float3 & __pos_rename_at_4, float3 const  & __vel_rename_at_4 )
{
	SimPolicy<float3>::SetAdd((char *)&(__pos_rename_at_4),(SimPolicy<float3>::MulVecScal(__vel_rename_at_4,cast<float>::from(0.0166666675f),*__context__)),*__context__);
	return true;
}
struct AotList_impl : AotListBase {
	virtual void registerAotFunctions ( AotLibrary & aotLib ) override {
		// test_30169ea2890b661e
		aotLib[0x5650d010aa847829] = [&](Context & ctx){
		return ctx.code->makeNode<SimNode_Aot<bool (*) ( Context * __context__, float3 & , float3 const  &  ),&test_30169ea2890b661e>>();
	};
	};
};
AotList_impl impl;
}
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
