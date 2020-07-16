#include "daScript/misc/platform.h"

#include "unitTest.h"
#include "module_unitTest.h"

#include "daScript/simulate/simulate_visit_op.h"

 int32_t TestObjectSmart::total = 0;

//sample of your-engine-float3-type to be aliased as float3 in daScript.
template<> struct das::cast <Point3>  : cast_fVec<Point3> {};

namespace das {
    template <>
    struct typeFactory<Point3> {
        static TypeDeclPtr make(const ModuleLibrary &) {
            auto t = make_smart<TypeDecl>(Type::tFloat3);
            t->alias = "Point3";
            t->aotAlias = true;
            return t;
        }
    };

    template <> struct typeName<Point3>   { static string name() { return "Point3"; } };
}

DAS_BASE_BIND_ENUM_98(SomeEnum_16, SomeEnum_16, SomeEnum_16_zero, SomeEnum_16_one, SomeEnum_16_two)

//sample of your engine annotated struct
MAKE_TYPE_FACTORY(TestObjectSmart,TestObjectSmart)
MAKE_TYPE_FACTORY(TestObjectFoo,TestObjectFoo)
MAKE_TYPE_FACTORY(TestObjectBar, TestObjectBar)
MAKE_TYPE_FACTORY(TestObjectNotLocal, TestObjectNotLocal)

MAKE_TYPE_FACTORY(SomeDummyType, SomeDummyType)
MAKE_TYPE_FACTORY(Point3Array, Point3Array)

namespace das {
  template <>
  struct typeFactory<SampleVariant> {
      static TypeDeclPtr make(const ModuleLibrary & library ) {
          auto vtype = make_smart<TypeDecl>(Type::tVariant);
          vtype->alias = "SampleVariant";
          vtype->aotAlias = true;
          vtype->addVariant("i_value", typeFactory<decltype(SampleVariant::i_value)>::make(library));
          vtype->addVariant("f_value", typeFactory<decltype(SampleVariant::f_value)>::make(library));
          vtype->addVariant("s_value", typeFactory<decltype(SampleVariant::s_value)>::make(library));
          // optional validation
          DAS_ASSERT(sizeof(SampleVariant) == vtype->getSizeOf());
          DAS_ASSERT(alignof(SampleVariant) == vtype->getAlignOf());
          DAS_ASSERT(offsetof(SampleVariant, i_value) == vtype->getVariantFieldOffset(0));
          DAS_ASSERT(offsetof(SampleVariant, f_value) == vtype->getVariantFieldOffset(1));
          DAS_ASSERT(offsetof(SampleVariant, s_value) == vtype->getVariantFieldOffset(2));
          return vtype;
      }
  };
};

struct TestObjectNotLocalAnnotation : ManagedStructureAnnotation <TestObjectNotLocal> {
    TestObjectNotLocalAnnotation(ModuleLibrary & ml) : ManagedStructureAnnotation ("TestObjectNotLocal", ml) {
        addField<DAS_BIND_MANAGED_FIELD(fooData)>("fooData");
    }
    virtual bool isLocal() const { return false; }
};

struct TestObjectSmartAnnotation : ManagedStructureAnnotation <TestObjectSmart> {
    TestObjectSmartAnnotation(ModuleLibrary & ml) : ManagedStructureAnnotation ("TestObjectSmart", ml) {
        addField<DAS_BIND_MANAGED_FIELD(fooData)>("fooData");
    }
    void init() {
        // this needs to be separate
        // reason: recursive type
        addField<DAS_BIND_MANAGED_FIELD(first)>("first");
    }
    virtual bool isLocal() const override { return false; }
    virtual bool canMove() const override { return false; }
    virtual bool canCopy() const override { return false; }
};

struct TestObjectFooAnnotation : ManagedStructureAnnotation <TestObjectFoo> {
    TestObjectFooAnnotation(ModuleLibrary & ml) : ManagedStructureAnnotation ("TestObjectFoo", ml) {
        addField<DAS_BIND_MANAGED_FIELD(hit)>("hit");
        addField<DAS_BIND_MANAGED_FIELD(fooData)>("fooData");
        addField<DAS_BIND_MANAGED_FIELD(e16)>("e16");
        addProperty<DAS_BIND_MANAGED_PROP(propAdd13)>("propAdd13");
        addProperty<DAS_BIND_MANAGED_PROP(hitPos)>("hitPos");
    }
    void init() {
        addField<DAS_BIND_MANAGED_FIELD(foo_loop)>("foo_loop");
    }
    virtual bool isLocal() const override { return true; }
    virtual bool canMove() const override { return true; }
    virtual bool canCopy() const override { return true; }
};

struct TestObjectBarAnnotation : ManagedStructureAnnotation <TestObjectBar> {
    TestObjectBarAnnotation(ModuleLibrary & ml) : ManagedStructureAnnotation ("TestObjectBar", ml) {
        addField<DAS_BIND_MANAGED_FIELD(fooPtr)>("fooPtr");
        addField<DAS_BIND_MANAGED_FIELD(barData)>("barData");
        addProperty<DAS_BIND_MANAGED_PROP(getFoo)>("getFoo");
        addProperty<DAS_BIND_MANAGED_PROP(getFooPtr)>("getFooPtr");
    }
    virtual bool isLocal() const { return true; }
};

void testFoo ( TestObjectFoo & foo ) {
    foo.fooData = 1234;
}

void testAdd ( int & a, int b ) {
    a += b;
}

struct IntFields {
    das_hash_map<string,int32_t> fields;
};

struct CheckRange : StructureAnnotation {
    CheckRange() : StructureAnnotation("checkRange") {}
    virtual bool touch(const StructurePtr & st, ModuleGroup &,
        const AnnotationArgumentList & args, string & ) override {
        // this is here for the 'example' purposes
        // lets add a sample 'dummy' field
        if (args.getBoolOption("dummy",false) && !st->findField("dummy")) {
            st->fields.emplace_back("dummy", make_smart<TypeDecl>(Type::tInt),
                nullptr /*init*/, AnnotationArgumentList(), false /*move_to_init*/, LineInfo());
            st->filedLookup.clear();
        }
        return true;
    }
    virtual bool look ( const StructurePtr & st, ModuleGroup &,
                        const AnnotationArgumentList & args, string & err ) override {
        bool ok = true;
        if (!args.getBoolOption("disable", false)) {
            for (auto & fd : st->fields) {
                if (fd.type->isSimpleType(Type::tInt) && fd.annotation.size()) {
                    int32_t val = 0;
                    int32_t minVal = INT32_MIN;
                    int32_t maxVal = INT32_MAX;
                    if (fd.init && fd.init->rtti_isConstant()) {
                        val = static_pointer_cast<ExprConstInt>(fd.init)->getValue();
                    }
                    if (auto minA = fd.annotation.find("min", Type::tInt)) {
                        minVal = minA->iValue;
                    }
                    if (auto maxA = fd.annotation.find("max", Type::tInt)) {
                        maxVal = maxA->iValue;
                    }
                    if (val<minVal || val>maxVal) {
                        err += fd.name + " out of annotated range [" + to_string(minVal) + ".." + to_string(maxVal) + "]\n";
                        ok = false;
                    }
                }
            }
        }
        return ok;
    }
};

struct IntFieldsAnnotation : StructureTypeAnnotation {

    // NOTE - SafeGetFieldPtr is not necessary, since its Int always

    // FIELD .
    struct SimNode_IntFieldDeref : SimNode {
        DAS_PTR_NODE;
        SimNode_IntFieldDeref ( const LineInfo & at, SimNode * rv, char * n ) : SimNode(at), value(rv), name(n) {}
        virtual SimNode * visit ( SimVisitor & vis ) override {
            V_BEGIN();
            V_OP(IntFieldDeref);
            V_SUB(value);
            V_ARG(name);
            V_END();
        }
        virtual SimNode * copyNode ( Context & context, NodeAllocator * code ) override {
            SimNode_IntFieldDeref * that = (SimNode_IntFieldDeref *) SimNode::copyNode(context, code);
            that->name = code->allocateName(name);
            return that;
        }
        char * compute ( Context & context ) {
            DAS_PROFILE_NODE
            vec4f rv = value->eval(context);
            if ( IntFields * prv = cast<IntFields *>::to(rv) ) {
                auto it = prv->fields.find(name);
                if ( it != prv->fields.end() ) {
                    return (char *) (&it->second);
                } else {
                    context.throw_error_at(debugInfo, "field %s not found", name);
                    return nullptr;
                }
            } else {
                context.throw_error_at(debugInfo,"dereferencing null pointer");
                return nullptr;
            }
        }
        SimNode *   value;
        char *      name;
    };

    struct SimNode_IntFieldDerefR2V : SimNode_IntFieldDeref {
        DAS_INT_NODE;
        SimNode_IntFieldDerefR2V ( const LineInfo & at, SimNode * rv, char * n )
            : SimNode_IntFieldDeref(at,rv,n) {}
        virtual SimNode * visit ( SimVisitor & vis ) override {
            V_BEGIN();
            V_OP(IntFieldDerefR2V);
            V_SUB(value);
            V_ARG(name);
            V_END();
        }
        __forceinline int32_t compute ( Context & context ) {
            DAS_PROFILE_NODE
            vec4f rv = value->eval(context);
            if ( IntFields * prv = cast<IntFields *>::to(rv) ) {
                auto it = prv->fields.find(name);
                if ( it != prv->fields.end() ) {
                    return it->second;
                } else {
                    context.throw_error_at(debugInfo,"field %s not found",name);
                    return 0;
                }
            } else {
                context.throw_error_at(debugInfo,"dereferencing null pointer");
                return 0;
            }
        }
    };

    // FIELD ?.
    struct SimNode_SafeIntFieldDeref : SimNode_IntFieldDeref {
        DAS_PTR_NODE;
        SimNode_SafeIntFieldDeref ( const LineInfo & at, SimNode * rv, char * n ) : SimNode_IntFieldDeref(at,rv,n) {}
        virtual SimNode * visit ( SimVisitor & vis ) override {
            V_BEGIN();
            V_OP(SafeIntFieldDeref);
            V_SUB(value);
            V_ARG(name);
            V_END();
        }
        __forceinline char * compute ( Context & context ) {
            DAS_PROFILE_NODE
            vec4f rv = value->eval(context);
            if ( IntFields * prv = cast<IntFields *>::to(rv) ) {
                auto it = prv->fields.find(name);
                if ( it != prv->fields.end() ) {
                    return (char *) &it->second;
                } else {
                    return nullptr;
                }
            } else {
                return nullptr;
            }
        }
    };

    IntFieldsAnnotation() : StructureTypeAnnotation("IntFields") {}
    virtual TypeAnnotationPtr clone ( const TypeAnnotationPtr & p = nullptr ) const override {
        smart_ptr<IntFieldsAnnotation> cp =  p ? static_pointer_cast<IntFieldsAnnotation>(p) : make_smart<IntFieldsAnnotation>();
        return StructureTypeAnnotation::clone(cp);
    }
    virtual bool create ( const smart_ptr<Structure> & st, const AnnotationArgumentList & args, string & err ) override {
        if( !StructureTypeAnnotation::create(st,args,err) )
            return false;
        bool fail = false;
        for ( auto & f : st->fields ) {
            if ( !f.type->isSimpleType(Type::tInt) ) {
                err += "field " + f.name + " must be int\n";
                fail = true;
            }
        }
        return !fail;
    }
    virtual TypeDeclPtr makeFieldType ( const string & na ) const override {
        if ( auto pF = makeSafeFieldType(na) ) {
            pF->ref = true;
            return pF;
        } else {
            return nullptr;
        }
    }
    virtual TypeDeclPtr makeSafeFieldType ( const string & na ) const override {
        auto pF = structureType->findField(na);
        return pF ? make_smart<TypeDecl>(*pF->type) : nullptr;
    }
    virtual SimNode * simulateGetField ( const string & na, Context & context,
                                        const LineInfo & at, const ExpressionPtr & rv ) const  override {
        return context.code->makeNode<SimNode_IntFieldDeref>(at,rv->simulate(context),context.code->allocateName(na));
    }
    virtual SimNode * simulateGetFieldR2V ( const string & na, Context & context,
                                           const LineInfo & at, const ExpressionPtr & rv ) const  override {
        return context.code->makeNode<SimNode_IntFieldDerefR2V>(at,rv->simulate(context),context.code->allocateName(na));
    }    virtual SimNode * simulateSafeGetField ( const string & na, Context & context,
                                            const LineInfo & at, const ExpressionPtr & rv ) const  override {
        return context.code->makeNode<SimNode_SafeIntFieldDeref>(at,rv->simulate(context),context.code->allocateName(na));
    }
    virtual size_t getSizeOf() const override { return sizeof(IntFields); }
    virtual size_t getAlignOf() const override { return alignof(IntFields); }
};

void testFields ( Context * ctx ) {
    int32_t t = 0;
    IntFields x;
    auto fx = ctx->findFunction("testFields");
    if (!fx) {
        ctx->throw_error("function testFields not found");
        return;
    }
    vec4f args[1] = { cast<IntFields *>::from(&x) };
    x.fields["a"] = 1;
    t = cast<int32_t>::to ( ctx->eval(fx, args) );
    assert(!ctx->getException());
    assert(t==1);
    x.fields["b"] = 2;
    t = cast<int32_t>::to ( ctx->eval(fx, args) );
    assert(!ctx->getException());
    assert(t==3);
    x.fields["c"] = 3;
    t = cast<int32_t>::to ( ctx->eval(fx, args) );
    assert(!ctx->getException());
    assert(t==6);
    x.fields["d"] = 4;
    t = cast<int32_t>::to ( ctx->eval(fx, args) );
    assert(!ctx->getException());
    assert(t==10);
    x.fields.erase("b");
    t = cast<int32_t>::to ( ctx->eval(fx, args) );
    assert(!ctx->getException());
    assert(t==8);
}

void test_das_string(const Block & block, Context * context) {
    string str = "test_das_string";
    string str2;
    vec4f args[2];
    args[0] = cast<void *>::from(&str);
    args[1] = cast<void *>::from(&str2);
    context->invoke(block, args, nullptr);
    if (str != "out_of_it") context->throw_error("test string mismatch");
    if (str2 != "test_das_string") context->throw_error("test string clone mismatch");
}

vec4f new_and_init ( Context & context, SimNode_CallBase * call, vec4f * ) {
    TypeInfo * typeInfo = call->types[0];
    if ( typeInfo->dim || typeInfo->type!=Type::tStructure ) {
        context.throw_error("invalid type");
        return v_zero();
    }
    auto size = getTypeSize(typeInfo);
    auto data = context.heap->allocate(size);
    if ( typeInfo->structType && typeInfo->structType->initializer!=-1 ) {
        auto fn = context.getFunction(typeInfo->structType->initializer);
        context.callWithCopyOnReturn(fn, nullptr, data, 0);
    } else {
        memset(data, 0, size);
    }
    return cast<char *>::from(data);
}
int g_st = 0;
int *getPtr() {return &g_st;}

uint2 get_screen_dimensions() {return uint2{1280, 720};}

uint32_t CheckEid ( TestObjectFoo & foo, char * const name, Context * context ) {
    if (!name) context->throw_error("invalid id");
    return hash_function(*context, name) + foo.fooData;
}

uint32_t CheckEidHint ( TestObjectFoo & foo, char * const name, uint32_t hashHint, Context * context ) {
    if (!name) context->throw_error("invalid id");
    uint32_t hv = hash_function(*context, name) + foo.fooData;
    if ( hv != hashHint ) context->throw_error("invalid hash value");
    return hashHint;
}

struct CheckEidFunctionAnnotation : TransformFunctionAnnotation {
    CheckEidFunctionAnnotation() : TransformFunctionAnnotation("check_eid") { }
    virtual ExpressionPtr transformCall ( ExprCallFunc * call, string & err ) override {
        auto arg = call->arguments[1];
        if ( arg->type && arg->type->isString() && arg->type->isConst() && arg->rtti_isConstant() ) {
            auto starg = static_pointer_cast<ExprConstString>(arg);
            if (!starg->getValue().empty()) {
                uint32_t hv = hash_blockz32((uint8_t *)starg->text.c_str());
                auto hconst = make_smart<ExprConstUInt>(arg->at, hv);
                hconst->type = make_smart<TypeDecl>(Type::tUInt);
                hconst->type->constant = true;
                auto newCall = static_pointer_cast<ExprCallFunc>(call->clone());
                newCall->arguments.insert(newCall->arguments.begin() + 2, hconst);
                return newCall;
            } else {
                err = "EID can't be an empty string";
            }
        }
        return nullptr;
    }
};

struct TestFunctionAnnotation : FunctionAnnotation {
    TestFunctionAnnotation() : FunctionAnnotation("test_function") { }
    virtual bool apply ( const FunctionPtr & fn, ModuleGroup &, const AnnotationArgumentList &, string & ) override {
        TextPrinter tp;
        tp << "test function: apply " << fn->describe() << "\n";
        return true;
    }
    virtual bool finalize ( const FunctionPtr & fn, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string & ) override {
        TextPrinter tp;
        tp << "test function: finalize " << fn->describe() << "\n";
        return true;
    }
    virtual bool apply ( ExprBlock * blk, ModuleGroup &, const AnnotationArgumentList &, string & ) override {
        TextPrinter tp;
        tp << "test function: apply to block at " << blk->at.describe() << "\n";
        return true;
    }
    virtual bool finalize ( ExprBlock * blk, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string & ) override {
        TextPrinter tp;
        tp << "test function: finalize block at " << blk->at.describe() << "\n";
        return true;
    }
};

struct EventRegistrator : StructureAnnotation {
    EventRegistrator() : StructureAnnotation("event") {}
    bool touch ( const StructurePtr & st, ModuleGroup & /*libGroup*/,
        const AnnotationArgumentList & /*args*/, string & /*err*/ ) override {
        st->fields.emplace(st->fields.begin(), "eventFlags", make_smart<TypeDecl>(Type::tUInt16),
            ExpressionPtr(), AnnotationArgumentList(), false, st->at);
        st->fields.emplace(st->fields.begin(), "eventSize", make_smart<TypeDecl>(Type::tUInt16),
            ExpressionPtr(), AnnotationArgumentList(), false, st->at);
        st->fields.emplace(st->fields.begin(), "eventType", make_smart<TypeDecl>(Type::tUInt),
            ExpressionPtr(), AnnotationArgumentList(), false, st->at);
        return true;
    }
    bool look (const StructurePtr & /*st*/, ModuleGroup & /*libGroup*/,
        const AnnotationArgumentList & /*args*/, string & /* err */ ) override {
        return true;
    }
};

#if defined(__APPLE__)

void builtin_printw(char * utf8string) {
    fprintf(stdout, "%s", utf8string);
    fflush(stdout);
}

#else

#include <iostream>
#include <codecvt>
#include <locale>

#if defined(_MSC_VER)
#include <fcntl.h>
#include <io.h>
#endif

void builtin_printw(char * utf8string) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
    auto outs = utf8_conv.from_bytes(utf8string);
#if defined(_MSC_VER)
    _setmode(_fileno(stdout), _O_U8TEXT);
#else
    std::wcout.sync_with_stdio(false);
    std::wcout.imbue(std::locale("en_US.utf8"));
#endif
    std::wcout << outs;
#if defined(_MSC_VER)
    _setmode(_fileno(stdout), _O_TEXT);
#endif
}

#endif

bool tempArrayExample( const TArray<char *> & arr,
    const TBlock<void, TTemporary<const TArray<char *>>> & blk, Context * context ) {
    vec4f args[1];
    args[0] = cast<void *>::from(&arr);
    context->invoke(blk, args, nullptr);
    return (arr.size == 1) && (strcmp(arr[0], "one") == 0);
}

void testPoint3Array(const TBlock<void,const Point3Array> & blk, Context * context) {
    Point3Array arr;
    for (int32_t x = 0; x != 10; ++x) {
        Point3 p;
        p.x = 1.0f;
        p.y = 2.0f;
        p.z = 3.0f;
        arr.push_back(p);
    }
    vec4f args[1];
    args[0] = cast<Point3Array *>::from(&arr);
    context->invoke(blk, args, nullptr);
}

void tableMojo ( TTable<char *,int> & in, const TBlock<void,TTable<char *,int>> & block, Context * context ) {
    vec4f args[1];
    args[0] = cast<Table *>::from(&in);
    context->invoke(block, args, nullptr);
}

Module_UnitTest::Module_UnitTest() : Module("UnitTest") {
    ModuleLibrary lib;
    lib.addModule(this);
    lib.addModule(Module::require("math"));
    lib.addBuiltInModule();
    addEnumTest(lib);
    // structure annotations
    addAnnotation(make_smart<CheckRange>());
    addAnnotation(make_smart<IntFieldsAnnotation>());
    // dummy type example
    addAnnotation(make_smart<DummyTypeAnnotation>("SomeDummyType", "SomeDummyType", sizeof(SomeDummyType), alignof(SomeDummyType)));
    // register types
    addAnnotation(make_smart<TestObjectNotLocalAnnotation>(lib));
    auto fooann = make_smart<TestObjectFooAnnotation>(lib);
    addAnnotation(fooann);
    initRecAnnotation(fooann,lib);
    addAnnotation(make_smart<TestObjectBarAnnotation>(lib));
    // smart object recursive type
    auto tosa = make_smart<TestObjectSmartAnnotation>(lib);
    addAnnotation(tosa);
    initRecAnnotation(tosa, lib);
    // events
    addAnnotation(make_smart<EventRegistrator>());
    // test
    addAnnotation(make_smart<TestFunctionAnnotation>());
    // point3 array
    addAnnotation(make_smart<ManagedVectorAnnotation<Point3Array>>("Point3Array",lib));
    addExtern<DAS_BIND_FUN(testPoint3Array)>(*this, lib, "testPoint3Array",
        SideEffects::modifyExternal, "testPoint3Array");
    // utf8 print
    addExtern<DAS_BIND_FUN(builtin_printw)>(*this, lib, "printw",
        SideEffects::modifyExternal, "builtin_printw");
    // register function
    addEquNeq<TestObjectFoo>(*this, lib);
    addExtern<DAS_BIND_FUN(complex_bind)>(*this, lib, "complex_bind",
        SideEffects::modifyExternal, "complex_bind");
    addInterop<new_and_init,void *,vec4f>(*this, lib, "new_and_init",
        SideEffects::none, "new_and_init");
    addExtern<DAS_BIND_FUN(get_screen_dimensions)>(*this, lib, "get_screen_dimensions",
        SideEffects::none, "get_screen_dimensions");
    addExtern<DAS_BIND_FUN(test_das_string)>(*this, lib, "test_das_string",
        SideEffects::modifyExternal, "test_das_string");
    addExtern<DAS_BIND_FUN(testFoo)>(*this, lib, "testFoo",
        SideEffects::modifyArgument, "testFoo");
    addExtern<DAS_BIND_FUN(testAdd)>(*this, lib, "testAdd",
        SideEffects::modifyArgument, "testAdd");
    addExtern<DAS_BIND_FUN(testFields)>(*this, lib, "testFields",
        SideEffects::modifyExternal, "testFields");
    addExtern<DAS_BIND_FUN(getSamplePoint3)>(*this, lib, "getSamplePoint3",
        SideEffects::modifyExternal, "getSamplePoint3");
    addExtern<DAS_BIND_FUN(doubleSamplePoint3)>(*this, lib, "doubleSamplePoint3",
        SideEffects::none, "doubleSamplePoint3");
    addExtern<DAS_BIND_FUN(project_to_nearest_navmesh_point)>(*this, lib, "project_to_nearest_navmesh_point",
        SideEffects::modifyArgument, "project_to_nearest_navmesh_point");
    addExtern<DAS_BIND_FUN(getPtr)>(*this, lib, "getPtr",
        SideEffects::modifyExternal, "getPtr");
    /*
     addExtern<DAS_BIND_FUN(makeDummy)>(*this, lib, "makeDummy", SideEffects::none, "makeDummy");
     */
    addExtern<DAS_BIND_FUN(makeDummy), SimNode_ExtFuncCallAndCopyOrMove>(*this, lib, "makeDummy",
        SideEffects::none, "makeDummy");
    addExtern<DAS_BIND_FUN(takeDummy)>(*this, lib, "takeDummy",
        SideEffects::none, "takeDummy");
    // register Cpp alignment functions
    addExtern<DAS_BIND_FUN(CppS1Size)>(*this, lib, "CppS1Size",
        SideEffects::modifyExternal, "CppS1Size");
    addExtern<DAS_BIND_FUN(CppS2Size)>(*this, lib, "CppS2Size",
        SideEffects::modifyExternal, "CppS2Size");
    addExtern<DAS_BIND_FUN(CppS2DOffset)>(*this, lib, "CppS2DOffset",
        SideEffects::modifyExternal, "CppS2DOffset");
    // register CheckEid functions
    addExtern<DAS_BIND_FUN(CheckEidHint)>(*this, lib, "CheckEid",
        SideEffects::none, "CheckEidHint");
    auto ceid = addExtern<DAS_BIND_FUN(CheckEid)>(*this, lib,
        "CheckEid", SideEffects::none, "CheckEid");
    auto ceid_decl = make_smart<AnnotationDeclaration>();
    ceid_decl->annotation = make_smart<CheckEidFunctionAnnotation>();
    ceid->annotations.push_back(ceid_decl);
    // register CheckEid2 functoins and macro
    addExtern<DAS_BIND_FUN(CheckEidHint)>(*this, lib, "CheckEid2",
        SideEffects::modifyExternal, "CheckEidHint");
    addExtern<DAS_BIND_FUN(CheckEid)>(*this, lib, "CheckEid2",
        SideEffects::modifyExternal, "CheckEid");
    addExtern<DAS_BIND_FUN(CheckEid)>(*this, lib, "CheckEid3",
        SideEffects::modifyExternal, "CheckEid");
    // extra tests
    addExtern<DAS_BIND_FUN(start_effect)>(*this, lib, "start_effect",
        SideEffects::modifyExternal, "start_effect");
    addExtern<DAS_BIND_FUN(tempArrayExample)>(*this, lib, "temp_array_example",
        SideEffects::modifyExternal, "tempArrayExample");
    // ptr2ref
    addExtern<DAS_BIND_FUN(fooPtr2Ref),SimNode_ExtFuncCallRef>(*this, lib, "fooPtr2Ref",
        SideEffects::none, "fooPtr2Ref");
    // compiled functions
    appendCompiledFunctions();
    // add sample variant type
    addAlias(typeFactory<SampleVariant>::make(lib));
    addExtern<DAS_BIND_FUN(makeSampleI), SimNode_ExtFuncCallAndCopyOrMove>(*this, lib, "makeSampleI",
        SideEffects::none, "makeSampleI");
    addExtern<DAS_BIND_FUN(makeSampleF), SimNode_ExtFuncCallAndCopyOrMove>(*this, lib, "makeSampleF",
        SideEffects::none, "makeSampleF");
    addExtern<DAS_BIND_FUN(makeSampleS), SimNode_ExtFuncCallAndCopyOrMove>(*this, lib, "makeSampleS",
        SideEffects::none, "makeSampleS");
    // smart ptr
    addExtern<DAS_BIND_FUN(getTotalTestObjectSmart)>(*this, lib, "getTotalTestObjectSmart",
        SideEffects::modifyExternal, "getTotalTestObjectSmart");
    addExtern<DAS_BIND_FUN(makeTestObjectSmart)>(*this, lib, "makeTestObjectSmart",
        SideEffects::modifyExternal, "makeTestObjectSmart");
    addExtern<DAS_BIND_FUN(countTestObjectSmart)>(*this, lib, "countTestObjectSmart",
        SideEffects::none, "countTestObjectSmart");
    // div
    addExtern<DAS_BIND_FUN(testGetDiv)>(*this, lib, "testGetDiv",
        SideEffects::none, "testGetDiv");
    addExtern<DAS_BIND_FUN(testGetNan)>(*this, lib, "testGetNan",
        SideEffects::none, "testGetNan");
    // call line
    addExtern<DAS_BIND_FUN(testCallLine)>(*this, lib, "testCallLine",
        SideEffects::modifyExternal, "testCallLine");
    // table mojo
    addExtern<DAS_BIND_FUN(tableMojo)>(*this, lib, "tableMojo",
        SideEffects::modifyExternal, "tableMojo");

    // and verify
    verifyAotReady();
}

Type Module_UnitTest::getOptionType ( const string & optName ) const {
    if ( optName=="unit_test" ) return Type::tFloat;
    return Type::none;
}

ModuleAotType Module_UnitTest::aotRequire ( TextWriter & tw ) const {
    tw << "#include \"unitTest.h\"\n";
    return ModuleAotType::cpp;
}

#include "unit_test.das.inc"
bool Module_UnitTest::appendCompiledFunctions() {
    return compileBuiltinModule("unit_test.das",unit_test_das, sizeof(unit_test_das));
}

REGISTER_MODULE(Module_UnitTest);
