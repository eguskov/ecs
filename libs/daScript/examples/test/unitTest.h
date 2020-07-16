#pragma once

#include "daScript/simulate/simulate.h"
#include "daScript/simulate/bind_enum.h"
#include "daScript/simulate/aot.h"

enum class SomeEnum {
    zero
,   one
,   two
};

DAS_BIND_ENUM_CAST(SomeEnum);

namespace Goo {
    enum class GooEnum {
        regular
    ,   hazardous
    };

    enum GooEnum98 {
        soft
    ,   hard
    };
}

DAS_BIND_ENUM_CAST(::Goo::GooEnum);
DAS_BIND_ENUM_CAST_98_IN_NAMESPACE(::Goo::GooEnum98,GooEnum98);

enum SomeEnum98 {
    SomeEnum98_zero = 0
,   SomeEnum98_one  = 1
,   SomeEnum98_two  = 2
};

DAS_BIND_ENUM_CAST_98(SomeEnum98);

enum SomeEnum_16 : int16_t {
    SomeEnum_16_zero = 0
,   SomeEnum_16_one  = 1
,   SomeEnum_16_two  = 2
};
DAS_BIND_ENUM_CAST_98(SomeEnum_16);

Goo::GooEnum efn_flip ( Goo::GooEnum goo );
SomeEnum efn_takeOne_giveTwo ( SomeEnum one );
SomeEnum98 efn_takeOne_giveTwo_98 ( SomeEnum98 one );
SomeEnum98_DasProxy efn_takeOne_giveTwo_98_DasProxy ( SomeEnum98_DasProxy two );

//sample of your-engine-float3-type to be aliased as float3 in daScript.
struct Point3 { float x, y, z; };

template <> struct das::das_alias<Point3> : das::das_alias_vec<Point3,float3> {};

typedef das::vector<Point3> Point3Array;

void testPoint3Array(const das::TBlock<void, const Point3Array> & blk, das::Context * context);

struct SomeDummyType {
    int32_t foo;
    float   bar;
};

__forceinline Point3 getSamplePoint3() {return Point3{0,1,2};}
__forceinline Point3 doubleSamplePoint3(const Point3 &a) { return Point3{ a.x + a.x, a.y + a.y, a.z + a.z }; }
__forceinline void project_to_nearest_navmesh_point(Point3 & a, float t) { a = Point3{ a.x + t, a.y + t, a.z + t }; }

struct TestObjectFoo {
    Point3 hit;
    int fooData;
    SomeEnum_16 e16;
    TestObjectFoo * foo_loop = nullptr;
    int propAdd13() {
        return fooData + 13;
    }
    __forceinline Point3 hitPos() const { return hit; }
    __forceinline bool operator == ( const TestObjectFoo & obj ) const {
        return fooData == obj.fooData;
    }
    __forceinline bool operator != ( const TestObjectFoo & obj ) const {
        return fooData != obj.fooData;
    }
};

struct TestObjectSmart : public das::ptr_ref_count {
     int fooData = 1234;
     das::smart_ptr<TestObjectSmart> first;
     TestObjectSmart() { total ++; }
     virtual ~TestObjectSmart() { total --; }
     static int32_t total;
};

__forceinline das::smart_ptr<TestObjectSmart> makeTestObjectSmart() { return das::make_smart<TestObjectSmart>(); }
__forceinline uint32_t countTestObjectSmart( const das::smart_ptr<TestObjectSmart> & p ) { return p->use_count(); }

__forceinline int32_t getTotalTestObjectSmart() { return TestObjectSmart::total; }

__forceinline TestObjectFoo makeDummy() { TestObjectFoo x; x.fooData = 1; return x; }
__forceinline int takeDummy ( const TestObjectFoo & x ) { return x.fooData; }

struct TestObjectBar {
    TestObjectFoo * fooPtr;
    float           barData;
    TestObjectFoo & getFoo() { return *fooPtr; }
    TestObjectFoo * getFooPtr() { return fooPtr; }
};

struct TestObjectNotLocal {
    int fooData;
};

int *getPtr();

void testFields ( das::Context * ctx );
void test_das_string(const das::Block & block, das::Context * context);
vec4f new_and_init ( das::Context & context, das::SimNode_CallBase * call, vec4f * );

struct CppS1 {
    virtual ~CppS1() {}
    // int64_t * a;
    int64_t b;
    int32_t c;
};

struct CppS2 : CppS1 {
    int32_t d;
};

__forceinline float testGetDiv ( float a, float b ) { return a / b; }
__forceinline float testGetNan() { return nanf("test"); }

__forceinline int CppS1Size() { return int(sizeof(CppS1)); }
__forceinline int CppS2Size() { return int(sizeof(CppS2)); }
__forceinline int CppS2DOffset() { return int(offsetof(CppS2, d)); }


uint32_t CheckEid ( TestObjectFoo & foo, char * const name, das::Context * context );
uint32_t CheckEidHint ( TestObjectFoo & foo, char * const name, uint32_t hashHint, das::Context * context );

__forceinline void complex_bind (const TestObjectFoo &,
                          const das::TBlock<void, das::TArray<TestObjectFoo>> &,
                          das::Context *) {
    // THIS DOES ABSOLUTELY NOTHING, ITS HERE TO TEST BIND OF ARRAY INSIDE BLOCK
}

__forceinline bool start_effect(const char *, const das::float3x4 &, float) {
    // THIS DOES ABSOLUTELY NOTHING, ITS HERE TO TEST DEFAULT ARGUMENTS
    return false;
}

void builtin_printw(char * utf8string);

bool tempArrayExample(const das::TArray<char *> & arr,
    const das::TBlock<void, das::TTemporary<const das::TArray<char *>>> & blk,
    das::Context * context);

__forceinline TestObjectFoo & fooPtr2Ref(TestObjectFoo * pMat) {
    return *pMat;
}

struct SampleVariant {
    int32_t _variant;
    union {
        int32_t     i_value;
        float       f_value;
        char *      s_value;
    };
};

template <> struct das::das_alias<SampleVariant>
    : das::das_alias_ref<SampleVariant,TVariant<sizeof(SampleVariant),int32_t,float,char *>> {};

__forceinline SampleVariant makeSampleI() {
    SampleVariant v;
    v._variant = 0;
    v.i_value = 1;
    return v;
}

__forceinline SampleVariant makeSampleF() {
    SampleVariant v;
    v._variant = 1;
    v.f_value = 2.0f;
    return v;
}

__forceinline SampleVariant makeSampleS() {
    SampleVariant v;
    v._variant = 2;
    v.s_value = (char *)("3");
    return v;
}

__forceinline int32_t testCallLine ( das::LineInfoArg * arg ) { return arg ? arg->line : 0; }

void tableMojo ( das::TTable<char *,int> & in, const das::TBlock<void,das::TTable<char *,int>> & block, das::Context * context );