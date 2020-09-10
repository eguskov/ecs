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
      walker.Int((int32_t&)((EntityId*)data)->handle);
  }
};

struct ComponentsMapAnnotation : das::ManagedStructureAnnotation<ComponentsMap, false>
{
  ComponentsMapAnnotation(das::ModuleLibrary &ml) : das::ManagedStructureAnnotation<ComponentsMap, false>("ComponentsMap", ml)
  {
    cppName = " ::ComponentsMap";
  }
};

struct QueryAnnotation : das::ManagedStructureAnnotation<Query, false>
{
  QueryAnnotation(das::ModuleLibrary &ml) : das::ManagedStructureAnnotation<Query, false>("Query", ml)
  {
    cppName = " ::Query";
  }
};

static bool get_underlying_ecs_type(das::Type type, das::string &str)
{
  switch (type)
  {
    case das::tString:       str = "string"; return true;
    case das::tBool:         str = "bool"; return true;
    case das::tInt64:        str = "int64_t"; return true;
    case das::tUInt64:       str = "uint64_t"; return true;
    case das::tInt:          str = "int"; return true;
    case das::tInt2:         str = "int2"; return true;
    case das::tInt3:         str = "int3"; return true;
    case das::tInt4:         str = "int4"; return true;
    case das::tUInt:         str = "uint32_t"; return true;
    case das::tFloat:        str = "float"; return true;
    case das::tFloat2:       str = "vec2"; return true;
    case das::tFloat3:       str = "vec3"; return true;
    case das::tFloat4:       str = "vec4"; return true;
    case das::tDouble:       str = "double"; return true;
    default:
      return false;
  }
}

static bool get_underlying_ecs_type(das::TypeDecl &info, das::string &str, das::Type &type, bool module_name = true)
{
  type = info.baseType;
  if (info.baseType == das::Type::tHandle)
  {
    if (!info.annotation)
      return false;
    if (!info.annotation->cppName.empty())
    {
      const char *cppName=info.annotation->cppName.c_str();
      while (*cppName == ' ' || *cppName == ':')
        cppName++;
      str = cppName;
    } else
    {
      str = (module_name && info.annotation->module && info.annotation->module->name!="$" ?
              info.annotation->module->name + "::" + info.annotation->name : info.annotation->name);
    }
    return true;
  }
  else if (info.baseType == das::Type::tStructure)
  {
    if (!info.structType)
      return false;
    str = module_name && info.structType->module ? info.structType->module->name + "::" +  info.structType->name : info.structType->name;
    return true;
  }
  else if (info.baseType == das::Type::tPointer)
  {
    if (!info.firstType)
      return false;
    return get_underlying_ecs_type(*info.firstType, str, type);
  }
  else if (info.baseType==das::Type::tEnumeration)
  {
    if (!info.enumType)
      return false;
    str = module_name && info.enumType->module ? info.enumType->module->name + "::" + info.enumType->name : info.enumType->name;
    return true;
  }
  else
    return get_underlying_ecs_type(info.baseType, str);
}

static void process_query_impl(Query &query, DasQueryData *query_data, const das::Block &block, das::Context *ctx, das::LineInfoArg *line)
{
  ASSERT(query_data != nullptr);

  vec4f * __restrict args = (vec4f *)(::alloca(query.componentsCount * sizeof(vec4f)));
  int32_t * __restrict stride = query_data->stride.data();

  for (auto q = query.begin(), e = query.end(); q != e; ++q)
  {
    for (int compNo = 0; compNo < query.componentsCount; ++compNo)
    {
      uint8_t * __restrict ptr = *(q.curChunk + compNo) + q.idx * stride[compNo];
      if (query_data->isComponentPointer[compNo])
        args[compNo] = das::cast<uint8_t *>::from(ptr);
      else
      {
        if (stride[compNo] == 1)
          args[compNo] = v_cast_vec4f(v_splatsi(*(uint8_t*)ptr));
        else if (stride[compNo] == 2)
          args[compNo] = v_cast_vec4f(v_splatsi(*(uint16_t*)ptr));
        else
          args[compNo] = v_ldu((float *)ptr);
      }
    }
    ctx->invoke(block, args, nullptr, line);
  }
}

static void process_query(Query &query, const das::Block &block, das::Context *ctx, das::LineInfoArg *line)
{
  process_query_impl(query, (DasQueryData *)query.userData.get(), block, ctx, line);
}

static void das_system(const RawArg &evt, Query &query)
{
  DasQueryData *qd = (DasQueryData *)query.userData.get();
  ASSERT(qd != nullptr);

  qd->ctx->tryRestartAndLock();
  auto lambda = [&evt, &query, qd]()
  {
    vec4f args[2];
    args[0] = das::cast<const uint8_t *>::from(evt.mem);
    args[1] = das::cast<const Query *>::from(&query);
    qd->ctx->call(qd->fn, args, 0);
  };
  const bool result = qd->ctx->runWithCatch(lambda);
  if (!result)
    DEBUG_LOG("unhandled exception <" << qd->ctx->getException() << "> during es <" << qd->fn->name << ">");
  qd->ctx->unlock();
}

static void das_system_empty(const RawArg &evt, Query &query)
{
  DasQueryData *qd = (DasQueryData *)query.userData.get();
  ASSERT(qd != nullptr);

  vec4f arg = das::cast<const uint8_t *>::from(evt.mem);
  qd->ctx->tryRestartAndLock();
  {
    auto lambda = [&arg, qd](){ qd->ctx->call(qd->fn, &arg, 0);};
    const bool result = qd->ctx->runWithCatch(lambda);
    if (!result)
      DEBUG_LOG("unhandled exception <" << qd->ctx->getException() << "> during es <" << qd->fn->name << ">");
  }
  qd->ctx->unlock();
}

void perform_query(const das::Block &block, das::Context *ctx, das::LineInfoArg *line)
{
  das::SimNode_ClosureBlock *closure = (das::SimNode_ClosureBlock *)block.body;
  DasQueryData *qd = (DasQueryData*)closure->annotationData;
  process_query_impl(ecs::get_query(qd->queryId), qd, block, ctx, line);
}

static inline void tags_from_list(const das::AnnotationArgumentList &args, const char *name, eastl::string &str)
{
  for (const auto &arg : args)
    if (arg.type == das::Type::tString && arg.name == name)
      str.append_sprintf("%s%s", str.length() > 0 ? "," : "", arg.sValue.c_str());
}

struct SystemRegistrator final : das::FunctionAnnotation
{
  SystemRegistrator() : das::FunctionAnnotation("es") {}

  virtual bool apply(das::ExprBlock *block, das::ModuleGroup &mg, const das::AnnotationArgumentList &args, das::string &err) override
  {
    return true;
  }

  virtual bool apply(const das::FunctionPtr &func, das::ModuleGroup &mg, const das::AnnotationArgumentList &args, das::string &err) override
  {
    ASSERT(!func->arguments.empty());

    func->exports = true;

    das::string eventName;
    das::Type eventType;
    get_underlying_ecs_type(*func->arguments[0]->type, eventName, eventType);

    eastl::string beforeStr;
    eastl::string afterStr;
    tags_from_list(args, "before", beforeStr);
    tags_from_list(args, "after",  afterStr);

    DasQueryData *d = new DasQueryData;
    d->isComponentPointer.resize((int)func->arguments.size() - 1);
    d->stride.resize((int)func->arguments.size() - 1);

    QueryDescription queryDesc;

    for (int i = 1; i < (int)func->arguments.size(); ++i)
    {
      const auto &arg = func->arguments[i];

      das::string argTypeName;
      das::Type argType;
      get_underlying_ecs_type(*arg->type, argTypeName, argType);

      Component comp;
      comp.name = hash_str(arg->name.c_str());
      comp.desc = find_component(argTypeName.c_str());
      ASSERT(comp.desc != nullptr);
      comp.size = comp.desc->size;

      d->isComponentPointer[i - 1] = arg->type->isRefOrPointer();
      d->stride[i - 1] = comp.size;
  
      queryDesc.components.push_back(comp);
    }

    for (const auto &arg : args)
      if (arg.type == das::Type::tString && (arg.name == "have" || arg.name == "notHave"))
      {
        const auto *compDesc = g_mgr->getComponentDescByName(arg.sValue.c_str());
        ASSERT(compDesc != nullptr);
  
        Component comp;
        comp.name = hash_str(arg.sValue.c_str());
        comp.desc = compDesc;
        ASSERT(comp.desc != nullptr);
        comp.size = comp.desc->size;
    
        (arg.name == "have" ? queryDesc.haveComponents : queryDesc.notHaveComponents).push_back(comp);
      }

    auto sysDesc = new SystemDescription(
      hash_str(func->name.c_str()),
      func->arguments.size() > 1 ? &das_system : &das_system_empty,
      hash_str(eventName.c_str()),
      beforeStr.empty() ? "*" : beforeStr.c_str(),
      afterStr.empty()  ? "*" : afterStr.c_str());
    // TODO: isDynamic as template argument
    sysDesc->isDynamic = true;

    auto *moduleData = ((EcsModuleGroupData*)mg.getUserData("ecs"));

    auto res = moduleData->dasSystems.insert(sysDesc->name.str);
    SystemId sid = g_mgr->createSystem(sysDesc->name, sysDesc, &queryDesc);
    res.first->second = sid;

    g_mgr->queries[g_mgr->systems[sid.index].queryId.index].userData.reset(d);

    if (func->arguments.size() > 1)
    {
      // def funName(info:ecs::UpdateStageInfoAct; var qv:QueryView) // note, no more arguments
      //    ecs::process_query(qv) <| $( clone of function arguments, but first )
      //        clone of function body

      auto cle = das::make_smart<das::ExprCall>(func->at, "ecs::process_query");
      auto blk = das::make_smart<das::ExprBlock>();
      blk->at = func->at;
      blk->isClosure = true;
      bool first = true;
      for (auto &arg : func->arguments)
      {
        if (!first)
        {
          auto carg = arg->clone();
          blk->arguments.push_back(carg);
        }
        else
        {
          first = false;
        }
      }
      blk->returnType = das::make_smart<das::TypeDecl>(das::Type::tVoid);
      auto ann = das::make_smart<das::AnnotationDeclaration>();
      ann->annotation = this;
      ann->arguments = args;
      blk->annotations.push_back(ann);
      blk->list.push_back(func->body->clone());

      auto mkb = das::make_smart<das::ExprMakeBlock>(func->at, blk);
      auto vinfo = das::make_smart<das::ExprVar>(func->at, "query");

      cle->arguments.push_back(vinfo);
      cle->arguments.push_back(mkb);
      auto cleb = das::make_smart<das::ExprBlock>();
      cleb->at = func->at;
      cleb->list.push_back(cle);
      func->body = cleb;
      func->arguments.resize(1);

      auto pVar = das::make_smart<das::Variable>();
      pVar->at = func->at;
      pVar->name = "query";
      pVar->type = mg.makeHandleType("ecs::Query");
      pVar->type->constant = false;
      pVar->type->removeConstant = true;

      func->arguments.push_back(pVar);
    }

    return true;
  }
  virtual bool finalize(const das::FunctionPtr &func, das::ModuleGroup &mg_, const das::AnnotationArgumentList &args, const das::AnnotationArgumentList &progArgs, das::string &err) override
  {
    return true;
  }
  virtual bool finalize(das::ExprBlock *block, das::ModuleGroup &mg, const das::AnnotationArgumentList &args, const das::AnnotationArgumentList &progArgs, das::string &err) override
  {
    ASSERT(!block->arguments.empty());

    DasQueryData *d = new DasQueryData;
    d->isComponentPointer.resize((int)block->arguments.size());
    d->stride.resize((int)block->arguments.size());

    QueryDescription queryDesc;

    for (int i = 0; i < (int)block->arguments.size(); ++i)
    {
      const auto &arg = block->arguments[i];

      das::string argTypeName;
      das::Type argType;
      get_underlying_ecs_type(*arg->type, argTypeName, argType);

      Component comp;
      comp.name = hash_str(arg->name.c_str());
      comp.desc = find_component(argTypeName.c_str());
      ASSERT(comp.desc != nullptr);
      comp.size = comp.desc->size;

      d->isComponentPointer[i] = arg->type->isRefOrPointer();
      d->stride[i] = comp.size;
  
      queryDesc.components.push_back(comp);
    }

    for (const auto &arg : args)
      if (arg.type == das::Type::tString && (arg.name == "have" || arg.name == "notHave"))
      {
        const auto *compDesc = g_mgr->getComponentDescByName(arg.sValue.c_str());
        ASSERT(compDesc != nullptr);
  
        Component comp;
        comp.name = hash_str(arg.sValue.c_str());
        comp.desc = compDesc;
        ASSERT(comp.desc != nullptr);
        comp.size = comp.desc->size;
    
        (arg.name == "have" ? queryDesc.haveComponents : queryDesc.notHaveComponents).push_back(comp);
      }

    auto mangledName = block->getMangledName(true, true);
    for (auto &ann : block->annotations)
      if (ann->annotation.get() == this)
        mangledName += " " + ann->getMangledName();

    // TODO: Mark as lazy query ?
    d->queryId = g_mgr->createQuery(hash_str(mangledName.c_str()), queryDesc);
    g_mgr->queries[d->queryId.index].userData.reset(d);

    block->annotationDataSid = das::hash_blockz32((uint8_t *)mangledName.c_str());
    block->annotationData = (uintptr_t)d;

    ((EcsModuleGroupData*)mg.getUserData("ecs"))->dasQueries.push_back(d->queryId);

    return true;
  }
  virtual bool simulate(das::Context *ctx, das::SimFunction *fn)
  {
    return true;
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
    addAnnotation(das::make_smart<QueryAnnotation>(lib));

    do_auto_bind_module(HASH("ecs"), *this, lib);

    das::addFunctionBasic<EntityId>(*this, lib);
    addFunction(das::make_smart<das::BuiltInFn<das::Sim_BoolNot<EntityId>, bool, EntityId>>("!", lib, "BoolNot"));

    das::addExtern<DAS_BIND_FUN(ecs_hash)>(*this, lib, "ecs_hash", das::SideEffects::none, "ecs_hash");
    das::addExtern<DAS_BIND_FUN(to_eastl_string)>(*this, lib, "eastl_string", das::SideEffects::none, "to_eastl_string");
    das::addExtern<DAS_BIND_FUN(create_entity)>(*this, lib, "create_entity", das::SideEffects::modifyExternal, "create_entity");
    das::addExtern<DAS_BIND_FUN(delete_entity)>(*this, lib, "delete_entity", das::SideEffects::modifyExternal, "delete_entity");

    das::addInterop<add_component_to_template, void, ComponentsMap&, const char*, vec4f>
      (*this, lib, "_builtin_add", das::SideEffects::modifyArgument, "add_component_to_template");

    addAnnotation(das::make_smart<TemplateRegistrator>());
    addAnnotation(das::make_smart<SystemRegistrator>());

    das::addExtern<DAS_BIND_FUN(process_query)>(*this, lib, "process_query", das::SideEffects::modifyExternal, "bind_dascript::process_query");
    das::addExtern<DAS_BIND_FUN(perform_query)>(*this, lib, "query", das::SideEffects::modifyExternal, "perform_query");

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