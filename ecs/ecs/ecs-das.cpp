#include <ecs/ecs-das.h>

#include <daScript/ast/ast_policy_types.h>
#include <daScript/simulate/sim_policy.h>
#include <daScript/simulate/simulate_visit_op.h>

namespace das
{
  IMPLEMENT_OP1_EVAL_POLICY(BoolNot, EntityId);
  IMPLEMENT_OP2_EVAL_BOOL_POLICY(Equ, EntityId);
  IMPLEMENT_OP2_EVAL_BOOL_POLICY(NotEqu, EntityId);
}

IMPLEMENT_EXTERNAL_TYPE_FACTORY(EntityId, EntityId);

// TODO: Bina as Value type
struct TagAnnotation : das::ManagedStructureAnnotation<Tag, false>
{
  TagAnnotation(das::ModuleLibrary &ml) : das::ManagedStructureAnnotation<Tag, false>("Tag", ml)
  {
    cppName = " ::Tag";
  }
  bool isLocal() const override { return true; }
};

struct EntityIdAnnotation final : das::ManagedValueAnnotation<EntityId>
{
  EntityIdAnnotation() : ManagedValueAnnotation("EntityId", " ::EntityId") {}

  void walk(das::DataWalker & walker, void * data) override
  {
    if (!walker.reading)
      walker.UInt(((EntityId*)data)->handle);
  }
};

struct ComponentsMapAnnotation : das::ManagedStructureAnnotation<ComponentsMap, false>
{
  ComponentsMapAnnotation(das::ModuleLibrary &ml) : das::ManagedStructureAnnotation<ComponentsMap, false>("ComponentsMap", ml)
  {
    cppName = " ::ComponentsMap";
  }
};

struct TemplateRegistrator final : das::FunctionAnnotation
{
  TemplateRegistrator() : das::FunctionAnnotation("ecsTemplate") {}

  eastl::hash_map<eastl::string, eastl::string> templateNameMap;

  virtual bool apply(das::ExprBlock * block, das::ModuleGroup &, const das::AnnotationArgumentList &args, das::string &err) override
  {
    return true;
  }
  virtual bool apply(const das::FunctionPtr &func, das::ModuleGroup &mg_, const das::AnnotationArgumentList &args, das::string &err) override
  {
    func->exports = true;
    return true;
  }
  virtual bool finalize(const das::FunctionPtr &func, das::ModuleGroup &mg_, const das::AnnotationArgumentList &args, const das::AnnotationArgumentList &progArgs, das::string &err) override
  {
    const das::AnnotationArgument *nameAnnotation = args.find("name", das::Type::tString);
    templateNameMap[func->name.c_str()] = (nameAnnotation ? nameAnnotation->sValue : func->name).c_str();
    return true;
  }
  virtual bool finalize(das::ExprBlock *block, das::ModuleGroup &mg_, const das::AnnotationArgumentList &args, const das::AnnotationArgumentList &progArgs, das::string &err) override
  {
    return true;
  }
  virtual bool simulate(das::Context *ctx, das::SimFunction *fn)
  {
    ComponentsMap cmap;
    vec4f args[] = { das::cast<ComponentsMap&>::from(cmap) };
    ctx->evalWithCatch(fn, args);

    auto res = templateNameMap.find_as(fn->name);
    ASSERT(res != templateNameMap.end());

    DEBUG_LOG("[templates] add: " << res->second.c_str());
    g_mgr->addTemplate(res->second.c_str(), eastl::move(cmap));

    return true;
  }
};

struct ComponentsMapDataWalker : das::DataWalker
{
  const char *name;
  ComponentsMap &cmap;

  ComponentsMapDataWalker(das::Context &_ctx, const char *_name, ComponentsMap &_cmap) : name(_name), cmap(_cmap)
  {
    context = &_ctx;
  }

  template<typename RetT, typename CompT>
  RetT& createComponent()
  {
    const auto *desc = find_component(ComponentType<CompT>::type);
    ASSERT(desc->size == sizeof(RetT));

    return *(RetT*)cmap.createComponent(name, desc);
  }

  void createComponent(const das::TypeAnnotation *ann, char *pa)
  {
    const auto desc = find_component(ann->name.c_str());
    ASSERT(desc->size == ann->getSizeOf());
    desc->move(cmap.createComponent(name, desc), (uint8_t*)pa);
  }

  void Bool(bool &value) override
  {
    createComponent<bool, bool>() = value;
  }

  void Int(int32_t &value) override
  {
    createComponent<int32_t, int>() = value;
  }

  void Float(float &value) override
  {
    createComponent<float, float>() = value;
  }

  void Float2(das::float2 &value) override
  {
    createComponent<das::float2, glm::vec2>() = value;
  }

  void Float3(das::float3 &value) override
  {
    createComponent<das::float3, glm::vec3>() = value;
  }

  void Float4(das::float4 &value) override
  {
    createComponent<das::float4, glm::vec4>() = value;
  }

  void String(char * &value) override
  {
    createComponent<eastl::string, eastl::string>() = (value ? value : "");
  }

  void beforeHandle(char *pa, das::TypeInfo *ti) override
  {
    createComponent(ti->getAnnotation(), pa);
    cancel = true;
  }

  void beforePtr(char *pa, das::TypeInfo *ti) override
  {
    createComponent(ti->firstType->getAnnotation(), pa);
    cancel = true;
  }
};

static vec4f add_component_to_template(das::Context &ctx, das::SimNode_CallBase *call, vec4f *args)
{
  ComponentsMap &cmap = *das::cast<ComponentsMap*>::to(args[0]);
  const char* name = das::cast<const char*>::to(args[1]);

  das::TypeInfo *info = call->types[2];

  ComponentsMapDataWalker walker(ctx, name, cmap);
  char *ptr = (info->type == das::tPointer || (info->flags & das::TypeInfo::flag_refType)) ?
    das::cast<char *>::to(args[2]) : (char *)&args[2];

  walker.walk(ptr, info);

  return v_zero();
}

static void create_entity(const char *template_name, const das::TBlock<void, ComponentsMap> &block, das::Context *context)
{
  ComponentsMap cmap;
  vec4f arg = das::cast<ComponentsMap*>::from(&cmap);
  context->invoke(block, &arg, nullptr);

  return ecs::create_entity(template_name, eastl::move(cmap));
}

static void delete_entity(EntityId eid)
{
  return ecs::delete_entity(eid);
}

static eastl::string* to_eastl_string(const char *str, das::Context *ctx)
{
  return new (ctx->heap->allocate(sizeof(eastl::string))) eastl::string(str);
}

static int ecs_hash(const char *s)
{
  return hash::str(s ? s : "");
}

struct ECSModule final : public das::Module
{
  ECSModule() : das::Module("ecs")
  {
    das::ModuleLibrary lib;
    lib.addBuiltInModule();
    lib.addModule(this);

    addAnnotation(das::make_smart<EntityIdAnnotation>());
    addAnnotation(das::make_smart<EASTLStringAnnotation>());
    addAnnotation(das::make_smart<ComponentsMapAnnotation>(lib));
    addAnnotation(das::make_smart<TagAnnotation>(lib));

    das::addFunctionBasic<EntityId>(*this, lib);
    addFunction(das::make_smart<das::BuiltInFn<das::Sim_BoolNot<EntityId>, bool, EntityId>>("!", lib, "BoolNot"));

    das::addExtern<DAS_BIND_FUN(ecs_hash)>(*this, lib, "ecs_hash", das::SideEffects::none, "ecs_hash");
    das::addExtern<DAS_BIND_FUN(to_eastl_string)>(*this, lib, "eastl_string", das::SideEffects::none, "to_eastl_string");
    das::addExtern<DAS_BIND_FUN(create_entity)>(*this, lib, "create_entity", das::SideEffects::modifyExternal, "create_entity");
    das::addExtern<DAS_BIND_FUN(delete_entity)>(*this, lib, "delete_entity", das::SideEffects::modifyExternal, "delete_entity");

    das::addInterop<add_component_to_template, void, ComponentsMap&, const char*, vec4f>
      (*this, lib, "_builtin_add", das::SideEffects::modifyArgument, "add_component_to_template");

    addAnnotation(das::make_smart<TemplateRegistrator>());

    #include "ecs.das.gen"
    compileBuiltinModule("ecs.das", ecs_builtin, sizeof(ecs_builtin));

    verifyAotReady();
  }

  virtual das::ModuleAotType aotRequire(das::TextWriter& tw) const override
  {
    tw << "#include \"dasModules/ecs.h\"\n";
    return das::ModuleAotType::cpp;
  }
};

REGISTER_MODULE(ECSModule);