// int and numeric

#define IMPLEMENT_OP1_INTEGER_FUSION_POINT(OPNAME) \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,Int,int32_t,int32_t); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,UInt,uint32_t,uint32_t); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(_msc_inline_bug,OPNAME,Int64,int64_t,int64_t); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(_msc_inline_bug,OPNAME,UInt64,uint64_t,uint64_t);

#define IMPLEMENT_OP1_NUMERIC_FUSION_POINT(OPNAME) \
    IMPLEMENT_OP1_INTEGER_FUSION_POINT(OPNAME); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,Float,float,float); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,Double,double,double);

#define IMPLEMENT_OP1_SCALAR_FUSION_POINT(OPNAME) \
    IMPLEMENT_OP1_NUMERIC_FUSION_POINT(OPNAME); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,Bool,bool,bool);

#define IMPLEMENT_OP1_WORKHORSE_FUSION_POINT(OPNAME) \
    IMPLEMENT_OP1_SCALAR_FUSION_POINT(OPNAME); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,Ptr,StringPtr,StringPtr); \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,Ptr,VoidPtr,StringPtr);

#define IMPLEMENT_OP1_VEC(OPNAME,CTYPE) \
    IMPLEMENT_ANY_OP1_FUSION_POINT(__forceinline,OPNAME,,CTYPE,CTYPE)

#define IMPLEMENT_OP1_INTEGER_VEC(OPNAME) \
    IMPLEMENT_OP1_VEC(OPNAME,int2 ); \
    IMPLEMENT_OP1_VEC(OPNAME,uint2); \
    IMPLEMENT_OP1_VEC(OPNAME,int3 ); \
    IMPLEMENT_OP1_VEC(OPNAME,uint3); \
    IMPLEMENT_OP1_VEC(OPNAME,int4 ); \
    IMPLEMENT_OP1_VEC(OPNAME,uint4);

#define IMPLEMENT_OP1_NUMERIC_VEC(OPNAME)  \
    IMPLEMENT_OP1_INTEGER_VEC(OPNAME); \
    IMPLEMENT_OP1_VEC(OPNAME,float2 ); \
    IMPLEMENT_OP1_VEC(OPNAME,float3 ); \
    IMPLEMENT_OP1_VEC(OPNAME,float4 );


