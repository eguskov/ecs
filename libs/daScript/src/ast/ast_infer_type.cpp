#include "daScript/misc/platform.h"

#include "daScript/ast/ast.h"
#include "daScript/ast/ast_visitor.h"
#include "daScript/ast/ast_generate.h"

namespace das {

    class CaptureLambda : public Visitor {
    public:
        virtual void preVisit ( ExprVar * expr ) override {
            auto var = expr->variable;
            if ( expr->local || expr->argument || expr->block ) {
                if ( expr->argument || (scope.find(var) != scope.end()) ) {
                    auto varT = var->type;
                    if ( !varT || varT->isAutoOrAlias() ) {
                        fail = true;
                        return;
                    }
                    capt.insert(var);
                }
            }
        }
        das_hash_set<VariablePtr>   scope;
        safe_var_set                capt;
        bool                        fail = false;
    };

// type inference

    class InferTypes : public FoldingVisitor {
    public:
        InferTypes( const ProgramPtr & prog ) : FoldingVisitor(prog ) {
            enableInferTimeFolding = prog->options.getBoolOption("infer_time_folding",true);
        }
        bool finished() const { return !needRestart; }
    protected:
        FunctionPtr             func;
        vector<VariablePtr>     local;
        vector<ExpressionPtr>   loop;
        vector<ExprBlock *>     blocks;
        vector<ExprBlock *>     scopes;
        vector<ExprWith *>      with;
        vector<size_t>          varStack;
        das_map<int32_t,int32_t>    labels;
        size_t                  fieldOffset = 0;
        int32_t                 fieldIndex = 0;
        bool                    cppLayout = false;
        bool                    cppLayoutPod = false;
        const Structure *       cppLayoutParent = nullptr;
        bool                    needRestart = false;
        bool                    enableInferTimeFolding;
        Expression *            lastEnuValue = nullptr;
        int32_t                 unsafeDepth = 0;
    public:
        vector<FunctionPtr>     extraFunctions;
    protected:
        string generateNewLambdaName(const LineInfo & at) {
            string mod = ctx.thisProgram->thisModule->name;
            if ( mod.empty() ) mod = "thismodule";
            return "_lambda_" + mod + "_" + to_string(at.line) + "_" + to_string(at.column)
                + "_" + to_string(program->newLambdaIndex++);
        }
        string generateNewLocalFunctionName(const LineInfo & at) {
            string mod = ctx.thisProgram->thisModule->name;
            if ( mod.empty() ) mod = "thismodule";
            return "_localfunction_" + mod + "_" + to_string(at.line) + "_" + to_string(at.column)
                + "_" + to_string(program->newLambdaIndex++);
        }
        void pushVarStack() {
            varStack.push_back(local.size());
        }
        void popVarStack()  {
            local.resize(varStack.back());
            varStack.pop_back();
        }
        void error ( const string & err, const string & extra, const string & fixme, const LineInfo & at, CompilationError cerr = CompilationError::unspecified ) const {
            if ( func ) {
                string ex = extra;
                if (!extra.empty()) ex += "\n";
                ex += func->getLocationExtra();
                program->error(err,ex,fixme,at,cerr);
            } else {
                program->error(err,extra,fixme,at,cerr);
            }
        }
        void reportAstChanged() {
            needRestart = true;
        }
        virtual void reportFolding() override {
            FoldingVisitor::reportFolding();
            needRestart = true;
        }
    protected:
        void verifyType ( const TypeDeclPtr & decl, bool allowExplicit = false ) const {
            // TODO: enable and cleanup
            if ( decl->isExplicit && !allowExplicit ) {
                /*
                error("expression can't be explicit here " + decl->describe(), "", "",
                      decl->at,CompilationError::invalid_type);
                */
            }
            if ( decl->dim.size() && decl->ref ) {
                error("can't declare an array of references, " + decl->describe(), "", "",
                      decl->at,CompilationError::invalid_type);
            }
            for ( auto di : decl->dim ) {
                if ( di<=0 ) {
                    error("array dimension can't be 0 or less, " + decl->describe(), "", "",
                          decl->at,CompilationError::invalid_array_dimension);
                }
            }
            if ( decl->baseType==Type::tFunction || decl->baseType==Type::tLambda
                || decl->baseType==Type::tBlock || decl->baseType==Type::tVariant ||
                    decl->baseType==Type::tTuple ) {
                if ( decl->argNames.size() && decl->argNames.size()!=decl->argTypes.size() ) {
                    string allNames = "\t";
                    for ( const auto & na : decl->argNames ) {
                        allNames += na + " ";
                    }
                    error("malformed type, " + decl->describe(), allNames, "",
                        decl->at,CompilationError::invalid_type);
                }
            }
            if ( decl->baseType==Type::tVoid ) {
                if ( decl->dim.size() ) {
                    error("can't declare an array of void, " + decl->describe(), "", "",
                          decl->at,CompilationError::invalid_type);
                }
                if ( decl->ref ) {
                    error("can't declare a void reference, " + decl->describe(), "", "",
                          decl->at,CompilationError::invalid_type);
                }
            } else if ( decl->baseType==Type::tPointer ) {
                if ( auto ptrType = decl->firstType ) {
                    if ( ptrType->ref ) {
                        error("can't declare a pointer to a reference, " + decl->describe(), "", "",
                              ptrType->at,CompilationError::invalid_type);
                    }
                    if ( decl->smartPtr ) {
                        if ( ptrType->baseType != Type::tHandle ) {
                            error("can't declare a smart pointer to anything other than annotation, " + decl->describe(), "", "",
                                ptrType->at,CompilationError::invalid_type);
                        } else if ( !ptrType->annotation->isSmart() ) {
                            error("this annotation does not support smart pointers, " + decl->describe(), "", "",
                                ptrType->at,CompilationError::invalid_type);
                        }
                    }
                    verifyType(ptrType);
                } else {
                    if ( decl->smartPtr ) {
                        error("can't declare a void smart pointer", "", "",
                              decl->at,CompilationError::invalid_type);
                    }
                }
            } else if ( decl->baseType==Type::tIterator ) {
                if ( auto ptrType = decl->firstType ) {
                    verifyType(ptrType);
                }
            } else if ( decl->baseType==Type::tArray ) {
                if ( auto arrayType = decl->firstType ) {
                    if ( arrayType->isAutoOrAlias() ) {
                        error("array type is not fully resolved, " + arrayType->describe(), "", "",
                              arrayType->at,CompilationError::invalid_array_type);
                    }
                    if ( arrayType->ref ) {
                        error("can't declare an array of references, " + arrayType->describe(), "", "",
                              arrayType->at,CompilationError::invalid_array_type);
                    }
                    if ( arrayType->baseType==Type::tVoid) {
                        error("can't declare a void array, " + arrayType->describe(), "", "",
                              arrayType->at,CompilationError::invalid_array_type);
                    }
                    if ( !arrayType->isLocal() ) {
                        error("array type has to be 'local', " + arrayType->describe(), "", "",
                              arrayType->at,CompilationError::invalid_type);
                    }
                    verifyType(arrayType);
                }
            } else if ( decl->baseType==Type::tTable ) {
                if ( auto keyType = decl->firstType ) {
                    if ( keyType->isAutoOrAlias() ) {
                        error("table key is not fully resolved, " + keyType->describe(), "", "",
                              keyType->at,CompilationError::invalid_array_type);
                    }
                    if ( keyType->ref ) {
                        error("table key can't be declared as a reference, " + keyType->describe(), "", "",
                              keyType->at,CompilationError::invalid_table_type);
                    }
                    if ( !keyType->isWorkhorseType() ) {
                        error("table key has to be declare as a basic 'hashable' type, " + keyType->describe(), "", "",
                              keyType->at,CompilationError::invalid_table_type);
                    }
                    verifyType(keyType);
                }
                if ( auto valueType = decl->secondType ) {
                    if ( valueType->isAutoOrAlias() ) {
                        error("table value is not fully resolved, " + valueType->describe(), "", "",
                              valueType->at,CompilationError::invalid_array_type);
                    }
                    if ( valueType->ref ) {
                        error("table value can't be declared as a reference, " + valueType->describe(), "", "",
                              valueType->at,CompilationError::invalid_table_type);
                    }
                    if ( !valueType->isLocal() ) {
                        error("table value has to be 'local', " + valueType->describe(), "", "",
                              valueType->at,CompilationError::invalid_type);
                    }
                    verifyType(valueType);
                }
            } else if ( decl->baseType==Type::tBlock || decl->baseType==Type::tFunction || decl->baseType==Type::tLambda ) {
                if ( auto resultType = decl->firstType ) {
                    if ( !resultType->isReturnType() ) {
                        error("not a valid return type, " + resultType->describe(), "", "",
                              resultType->at,CompilationError::invalid_return_type);
                    }
                    verifyType(resultType);
                }
                for ( auto & argType : decl->argTypes ) {
                    if ( argType->ref && argType->isRefType() ) {
                        error("can't pass a boxed type by a reference, " + argType->describe(), "", "",
                              argType->at,CompilationError::invalid_argument_type);
                    }
                    verifyType(argType,true);
                }
            } else if ( decl->baseType==Type::tTuple ) {
                for ( auto & argType : decl->argTypes ) {
                    if ( argType->ref ) {
                        error("tuple element can't be ref, " + argType->describe(), "", "",
                              argType->at,CompilationError::invalid_type);
                    }
                    verifyType(argType);
                }
            } else if ( decl->baseType==Type::tVariant ) {
                for ( auto & argType : decl->argTypes ) {
                    if ( argType->ref ) {
                        error("variant element can't be ref, " + argType->describe(), "", "",
                              argType->at,CompilationError::invalid_type);
                    }
                    verifyType(argType);
                }
            }
        }

        void propagateTempType ( const TypeDeclPtr & parentType, TypeDeclPtr & subexprType ) {
            if ( subexprType->isTempType() ) {
                if ( parentType->temporary ) subexprType->temporary = true;   // array<int?># -> int?#
            } else {
                subexprType->temporary = false; // array<int#> -> int
            }
        }

        // find type alias name, and resolve it to type
        // without one generic function
        TypeDeclPtr findFuncAlias ( const FunctionPtr & fptr, const string & name ) const {
            for ( auto & arg : fptr->arguments ) {
                if ( auto aT = arg->type->findAlias(name,true) ) {
                    return aT;
                }
            }
            if ( auto rT = fptr->result->findAlias(name,true) ) {
                return rT;
            }
            for ( auto & gvar : program->thisModule->globalsInOrder ) {
                if ( auto vT = gvar->type->findAlias(name,false) ) {
                    return vT;
                }
            }
            auto mtd = program->makeTypeDeclaration(LineInfo(),name);
            return mtd->isAlias() ? nullptr : mtd;
        }

        // find type alias name, and resolve it to type
        // within current context
        TypeDeclPtr findAlias ( const string & name ) const {
            if ( func ) {
                for ( auto it = local.rbegin(); it!=local.rend(); ++it ) {
                    auto & var = *it;
                    if ( auto vT = var->type->findAlias(name) ) {
                        return vT;
                    }
                }
                for ( auto & arg : func->arguments ) {
                    if ( auto aT = arg->type->findAlias(name) ) {
                        return aT;
                    }
                }
                if ( auto rT = func->result->findAlias(name,true) ) {
                    return rT;
                }
            }
            for ( auto & gvar : program->thisModule->globalsInOrder ) {
                if ( auto vT = gvar->type->findAlias(name) ) {
                    return vT;
                }
            }
            TypeDeclPtr mtd = program->makeTypeDeclaration(LineInfo(),name);
            return mtd->isAlias() ? nullptr : mtd;
        }

        // WARNING: this is really really slow, use faster tests when u can isAutoOrAlias for one
        // type chain is fully resolved, and not aliased \ auto
        bool isFullySealedType(const TypeDeclPtr & ptr, das_set<const TypeDecl *> & all ) {
            if (!ptr) return false;
            if ( ptr->baseType==Type::tStructure || ptr->baseType==Type::tTuple || ptr->baseType==Type::tVariant ) {
                auto thisType = ptr.get();
                if ( all.find(thisType)!=all.end() ) return true;
                all.insert(thisType);
            }
            if (ptr->baseType==Type::autoinfer || ptr->baseType==Type::alias) return false;
            if (ptr->firstType && !isFullySealedType(ptr->firstType,all)) return false;
            if (ptr->secondType && !isFullySealedType(ptr->secondType,all)) return false;
            for (auto & argT : ptr->argTypes) {
                if (argT && !isFullySealedType(argT,all)) return false;
            }
            if ( ptr->structType ) {
                for ( auto & fd : ptr->structType->fields )
                    if ( !isFullySealedType(fd.type,all) ) return false;
            }
            return true;
        }
        bool isFullySealedType(const TypeDeclPtr & ptr) {
            das_set<const TypeDecl *> all;
            return isFullySealedType(ptr, all);
        }

        // infer alias type
        TypeDeclPtr inferAlias ( const TypeDeclPtr & decl, const FunctionPtr & fptr = nullptr, AliasMap * aliases = nullptr ) const {
            if ( decl->baseType==Type::autoinfer ) {    // until alias is fully resolved, can't infer
                return nullptr;
            }
            if ( decl->baseType==Type::alias ) {
                auto aT = fptr ? findFuncAlias(fptr, decl->alias) : findAlias(decl->alias);
                if ( aliases ) {
                    if ( aT && aT->baseType==Type::autoinfer ) {
                        auto it = aliases->find(decl->alias);
                        if ( it != aliases->end() ) {
                            aT = it->second.get();
                        }
                    }
                }
                if ( aT ) {
                    auto resT = make_smart<TypeDecl>(*aT);
                    resT->at = decl->at;
                    resT->ref = (resT->ref | decl->ref) & !decl->removeRef;
                    resT->constant = (resT->constant | decl->constant) & !decl->removeConstant;
                    resT->temporary = (resT->temporary | decl->temporary) & !decl->removeTemporary;
                    resT->dim = decl->dim;
                    resT->alias.clear();
                    return resT;
                } else {
                    return nullptr;
                }
            }
            auto resT = make_smart<TypeDecl>(*decl);
            if ( decl->baseType==Type::tPointer ) {
                if ( decl->firstType ) {
                    resT->firstType = inferAlias(decl->firstType,fptr,aliases);
                    if ( !resT->firstType ) return nullptr;
                }
            } else if ( decl->baseType==Type::tIterator ) {
                if ( decl->firstType ) {
                    resT->firstType = inferAlias(decl->firstType,fptr,aliases);
                    if ( !resT->firstType ) return nullptr;
                }
            } else if ( decl->baseType==Type::tArray ) {
                if ( decl->firstType ) {
                    resT->firstType = inferAlias(decl->firstType,fptr,aliases);
                    if ( !resT->firstType ) return nullptr;
                }
            } else if ( decl->baseType==Type::tTable ) {
                if ( decl->firstType ) {
                    resT->firstType = inferAlias(decl->firstType,fptr,aliases);
                    if ( !resT->firstType ) return nullptr;
                }
                if ( decl->secondType ) {
                    resT->secondType = inferAlias(decl->secondType,fptr,aliases);
                    if ( !resT->secondType ) return nullptr;
                }
            } else if ( decl->baseType==Type::tFunction || decl->baseType==Type::tLambda || decl->baseType==Type::tBlock  ) {
                for ( size_t iA=0; iA!=decl->argTypes.size(); ++iA ) {
                    auto & declAT = decl->argTypes[iA];
                    if ( auto infAT = inferAlias(declAT,fptr,aliases) ) {
                        resT->argTypes[iA] = infAT;
                    } else {
                        return nullptr;
                    }
                }
                resT->firstType = inferAlias(decl->firstType,fptr,aliases);
                if ( !resT->firstType ) return nullptr;
            } else if ( decl->baseType==Type::tVariant || decl->baseType==Type::tTuple ) {
                for ( size_t iA=0; iA!=decl->argTypes.size(); ++iA ) {
                    auto & declAT = decl->argTypes[iA];
                    if ( auto infAT = inferAlias(declAT,fptr,aliases) ) {
                        resT->argTypes[iA] = infAT;
                    } else {
                        return nullptr;
                    }
                }
            }
            return resT;
        }

        // infer alias type
        TypeDeclPtr inferPartialAliases ( const TypeDeclPtr & decl, const FunctionPtr & fptr = nullptr, AliasMap * aliases = nullptr ) const {
            if ( decl->baseType==Type::autoinfer ) {
                return decl;
            }
            if ( decl->baseType==Type::alias ) {
                auto aT = fptr ? findFuncAlias(fptr, decl->alias) : findAlias(decl->alias);
                if ( aliases ) {
                    if ( aT && aT->baseType==Type::autoinfer ) {
                        auto it = aliases->find(decl->alias);
                        if ( it != aliases->end() ) {
                            aT = it->second.get();
                        }
                    }
                }
                if ( aT ) {
                    auto resT = make_smart<TypeDecl>(*aT);
                    resT->at = decl->at;
                    resT->ref = (resT->ref | decl->ref) & !decl->removeRef;
                    resT->constant = (resT->constant | decl->constant) & !decl->removeConstant;
                    resT->temporary = (resT->temporary | decl->temporary) & !decl->removeTemporary;
                    resT->dim = decl->dim;
                    resT->alias.clear();
                    return resT;
                } else {
                    return decl;
                }
            }
            auto resT = make_smart<TypeDecl>(*decl);
            if ( decl->baseType==Type::tPointer ) {
                if ( decl->firstType ) {
                    resT->firstType = inferPartialAliases(decl->firstType,fptr,aliases);
                }
            } else if ( decl->baseType==Type::tIterator ) {
                if ( decl->firstType ) {
                    resT->firstType = inferPartialAliases(decl->firstType,fptr,aliases);
                }
            } else if ( decl->baseType==Type::tArray ) {
                if ( decl->firstType ) {
                    resT->firstType = inferPartialAliases(decl->firstType,fptr,aliases);
                }
            } else if ( decl->baseType==Type::tTable ) {
                if ( decl->firstType ) {
                    resT->firstType = inferPartialAliases(decl->firstType,fptr,aliases);
                }
                if ( decl->secondType ) {
                    resT->secondType = inferPartialAliases(decl->secondType,fptr,aliases);
                }
            } else if ( decl->baseType==Type::tFunction || decl->baseType==Type::tLambda
                || decl->baseType==Type::tBlock || decl->baseType==Type::tVariant ||
                    decl->baseType==Type::tTuple ) {
                for ( size_t iA=0; iA!=decl->argTypes.size(); ++iA ) {
                    auto & declAT = decl->argTypes[iA];
                    resT->argTypes[iA] = inferPartialAliases(declAT,fptr,aliases);
                }
                resT->firstType = inferPartialAliases(decl->firstType,fptr,aliases);
            }
            return resT;
        }


        Module * getSearchModule(string & moduleName) const {
            if ( moduleName=="_" ) {
                moduleName = "*";
                return program->thisModule.get();
            } else if ( moduleName=="__" ) {
                moduleName = program->thisModule->name;
                return program->thisModule.get();
            } else if ( func ) {
                if ( func->fromGeneric ) {
                    if ( moduleName.empty() ) {     // ::foo in generic means generic::goo, not this::foo
                        moduleName = func->fromGeneric->module->name;
                    }
                    return func->fromGeneric->module;
                } else {
                    return func->module;
                }
            } else {
                return program->thisModule.get();
            }
        }

        Module * getFunctionVisModule( Function * fn ) const {
            return fn->fromGeneric ? fn->fromGeneric->module : fn->module;
        }

        bool canCallPrivate ( const FunctionPtr & pFn, Module * mod, Module * thisMod ) const {
            if ( !pFn->privateFunction ) {
                return true;
            } else if ( pFn->module==mod || pFn->module==thisMod ) {
                return true;
            } else if ( pFn->fromGeneric && (pFn->fromGeneric->module==mod || pFn->fromGeneric->module==thisMod) ) {
                return true;
            } else {
                return false;
            }
        }

        // b <- a <- this
        //  a(x)    b
        //  this`a(x)   __::b
        //      inWhichModule = this
        //      objModule = _b
        bool isVisibleFunc ( Module * inWhichModule, Module * objModule ) const {
            if ( inWhichModule->isVisibleDirectly(objModule) ) return true;
            // can always call within same module from instanced generic
            if ( func && func->fromGeneric ) {
                auto inWhichOtherModule = func->fromGeneric->module;
                if ( inWhichOtherModule->isVisibleDirectly(objModule) ) return true;
            }
            return false;
        }

        vector<FunctionPtr> findFuncAddr ( const string & name ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            auto inWhichModule = getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->functionsByName.find(funcName);
                if ( itFnList != mod->functionsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    for ( auto & pFn : goodFunctions ) {
                        if ( isVisibleFunc(inWhichModule,getFunctionVisModule(pFn.get())) ) {
                            if ( canCallPrivate(pFn,inWhichModule,program->thisModule.get()) ) {
                                result.push_back(pFn);
                            }
                        }
                    }
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findTypedFuncAddr ( const string & name, const vector<TypeDeclPtr> & arguments, const TypeDeclPtr & resType ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            auto inWhichModule = getSearchModule(moduleName);
            TextWriter ss;
            for ( auto & arg : arguments ) {
                ss << " " << arg->getMangledName();
            }
            string mangledNamePostfix = ss.str();
            program->library.foreach([&](Module * mod) -> bool {
                string mangledName;
                if ( !mod->name.empty() ) {
                    mangledName = "@" + mod->name + "::";
                }
                mangledName = mangledName + funcName + mangledNamePostfix;
                auto itFn = mod->functions.find(mangledName);
                if ( itFn != mod->functions.end() ) {
                    const auto & pFn = itFn->second;
                    if ( isVisibleFunc(inWhichModule,getFunctionVisModule(pFn.get())) ) {
                        if ( canCallPrivate(pFn,inWhichModule,program->thisModule.get()) ) {
                            if ( pFn->result->isSameType(*resType, RefMatters::yes, ConstMatters::yes, TemporaryMatters::yes) ) {
                                result.push_back(pFn);
                            }
                        }
                    }
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findCandidates ( const string & name, const vector<TypeDeclPtr> & ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->functionsByName.find(funcName);
                if ( itFnList != mod->functionsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    result.insert(result.end(), goodFunctions.begin(), goodFunctions.end());
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findCandidates ( const string & name, const vector<MakeFieldDeclPtr> & ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->functionsByName.find(funcName);
                if ( itFnList != mod->functionsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    result.insert(result.end(), goodFunctions.begin(), goodFunctions.end());
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findGenericCandidates ( const string & name, const vector<MakeFieldDeclPtr> & ) const {
            // TODO: better error reporting
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->genericsByName.find(funcName);
                if ( itFnList != mod->genericsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    result.insert(result.end(), goodFunctions.begin(), goodFunctions.end());
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findGenericCandidates ( const string & name, const vector<TypeDeclPtr> & ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->genericsByName.find(funcName);
                if ( itFnList != mod->genericsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    result.insert(result.end(), goodFunctions.begin(), goodFunctions.end());
                }
                return true;
            },moduleName);
            return result;
        }

        bool isMatchingArgument(const FunctionPtr & pFn, TypeDeclPtr argType, TypeDeclPtr passType, bool inferAuto, bool inferBlock, AliasMap * aliases = nullptr ) const {
            if (!passType) {
                return false;
            }
            // explicit const mast match
            if ( argType->explicitConst && (argType->constant != passType->constant) ) {
                return false;
            }
            if ( argType->baseType==Type::anyArgument ) {
                return true;
            }
            if (inferAuto) {
                // if it's an alias, we de'alias it, and see if it matches at all
                if (argType->isAlias()) {
                    argType = inferAlias(argType, pFn, aliases);
                    if ( !argType ) {
                        return false;
                    }
                }
                // match auto argument
                if (argType->isAuto()) {
                    return TypeDecl::inferGenericType(argType, passType, aliases) != nullptr;
                }
            }
            // match inferable block
            if (inferBlock && passType->isAuto() && (passType->isGoodBlockType() || passType->isGoodLambdaType() || passType->isGoodFunctionType())) {
                return TypeDecl::inferGenericType(passType, argType) != nullptr;
            }
            // compare types which don't need inference
            auto tempMatters = argType->implicit ? TemporaryMatters::no : TemporaryMatters::yes;
            if ( !argType->isSameType(*passType, RefMatters::no, ConstMatters::no, tempMatters, AllowSubstitute::yes) ) {
                return false;
            }
            // can't pass non-ref to ref
            if ( argType->isRef() && !passType->isRef() ) {
                return false;
            }
            // ref types can only add constness
            if (argType->isRef() && !argType->constant && passType->constant) {
                return false;
            }
            // pointer types can only add constant
            if (argType->isPointer() && !argType->constant && passType->constant) {
                return false;
            }
            // all good
            return true;
        }

        bool isFunctionCompatible ( const FunctionPtr & pFn, const vector<TypeDeclPtr> & types, bool inferAuto, bool inferBlock ) const {
            if ( pFn->arguments.size() < types.size() ) {
                return false;
            }
            if ( inferAuto && inferBlock ) {
                AliasMap aliases;
                for ( ;; ) {
                    bool anyFailed = false;
                    auto totalAliases = aliases.size();
                    for ( size_t ai = 0; ai != types.size(); ++ai ) {
                        auto argType = pFn->arguments[ai]->type;
                        if ( argType->isAlias() ) {
                            argType = inferPartialAliases(argType, pFn, &aliases);
                        }
                        auto passType = types[ai];
                        if (!isMatchingArgument(pFn,argType,passType,inferAuto,inferBlock,&aliases)) {
                            anyFailed = true;
                            continue;
                        }
                        TypeDecl::updateAliasMap(argType, passType, aliases);
                    }
                    if ( !anyFailed ) {
                        break;
                    }
                    if ( totalAliases == aliases.size() ) {
                        return false;
                    }
                }
            } else {
                for ( size_t ai = 0; ai != types.size(); ++ai ) {
                    if (!isMatchingArgument(pFn, pFn->arguments[ai]->type, types[ai],inferAuto,inferBlock)) {
                        return false;
                    }
                }
            }
            for ( auto ti = types.size(); ti != pFn->arguments.size(); ++ti ) {
                if ( !pFn->arguments[ti]->init ) {
                    return false;
                }
            }
            return true;
        }

        bool isFunctionCompatible ( const FunctionPtr & pFn, const vector<MakeFieldDeclPtr> & arguments, bool inferAuto, bool inferBlock ) const {
            if ( pFn->arguments.size() < arguments.size() ) {
                return false;
            }
            size_t fnArgIndex = 0;
            for ( size_t ai = 0; ai != arguments.size(); ++ai ) {
                auto & arg = arguments[ai];
                for ( ;; ) {
                    if ( fnArgIndex >= pFn->arguments.size() ) {    // out of source arguments. done
                        return false;
                    }
                    auto & fnArg = pFn->arguments[fnArgIndex];
                    if ( fnArg->name == arg->name ) {               // found it, name matches
                        break;
                    }
                    if ( !fnArg->init ) {                           // can't skip - no default value
                        return false;
                    }
                    fnArgIndex ++;
                }
                if (!isMatchingArgument(pFn, pFn->arguments[fnArgIndex]->type, arg->value->type,inferAuto,inferBlock)) {
                    return false;
                }
                fnArgIndex ++;
            }
            while ( fnArgIndex < pFn->arguments.size() ) {
                if ( !pFn->arguments[fnArgIndex]->init ) {
                    return false;                                   // tail must be defaults
                }
                fnArgIndex ++;
            }
            return true;
        }

        string reportAliasError( const TypeDeclPtr & type ) const {
            vector<string> aliases;
            type->collectAliasList(aliases);
            TextWriter ss;
            ss << "don't know what " << type->describe() << " is";
            for (auto & aa : aliases) {
                ss << "; unknown type " << aa;
            }
            return ss.str();
        }

        string describeMismatchingFunction(const FunctionPtr & pFn, const vector<MakeFieldDeclPtr> & arguments, bool inferAuto, bool inferBlock) const {
            if ( pFn->arguments.size() < arguments.size() ) {
                return "\t\ttoo many arguments\n";
            }
            TextWriter ss;
            size_t fnArgIndex = 0;
            for ( size_t ai = 0; ai != arguments.size(); ++ai ) {
                auto & arg = arguments[ai];
                for ( ;; ) {
                    if ( fnArgIndex >= pFn->arguments.size() ) {
                        auto it = find_if ( pFn->arguments.begin(), pFn->arguments.end(), [&]( const VariablePtr & varg){
                            return varg->name == arg->name;
                        });
                        if ( it != pFn->arguments.end() ) {
                            ss << "\t\tcan't match argument " << arg->name << ", its submitted out of order\n";
                        } else {
                            ss << "\t\tcan't match argument " << arg->name << ", out of function arguments\n";
                        }
                        return ss.str();
                    }
                    auto & fnArg = pFn->arguments[fnArgIndex];
                    if ( fnArg->name == arg->name ) {
                        break;
                    }
                    if ( !fnArg->init ) {
                        ss << "\t\twhile looking for argument " << arg->name
                            << ", can't skip function argument " << fnArg->name << " because it has no default value\n";
                        return ss.str();
                    }
                    fnArgIndex ++;
                }
                if (!isMatchingArgument(pFn, pFn->arguments[fnArgIndex]->type, arg->value->type,inferAuto,inferBlock)) {
                    ss << "\t\tinvalid argument " << arg->name << ". expecting "
                        << pFn->arguments[fnArgIndex]->type->describe() << ", passing " << arg->value->type->describe() << "\n";
                    if (arg->value->type->isAlias()) {
                        ss << "\t\t" << reportAliasError(arg->value->type) << "\n";
                    }
                    return ss.str();
                }
                fnArgIndex ++;
            }
            while ( fnArgIndex < pFn->arguments.size() ) {
                if ( !pFn->arguments[fnArgIndex]->init ) {
                    ss << "\t\tmissing default value for function argument " << pFn->arguments[fnArgIndex]->name << "\n";
                    return ss.str();
                }
                fnArgIndex ++;
            }
            return ss.str();
        }

        string describeMismatchingFunction(const FunctionPtr & pFn, const vector<TypeDeclPtr> & types, bool inferAuto, bool inferBlock) const {
            TextWriter ss;
            size_t tot = das::min ( types.size(), pFn->arguments.size() );
            size_t ai;
            for (ai = 0; ai != tot; ++ai) {
                auto & arg = pFn->arguments[ai];
                auto & passType = types[ai];
                if (!isMatchingArgument(pFn, arg->type, passType, inferAuto, inferBlock)) {
                    ss << "\t\tinvalid argument " << arg->name << ". expecting "
                        << arg->type->describe() << ", passing " << passType->describe() << "\n";
                    if ( passType->isAlias() ) {
                        ss << "\t\t" << reportAliasError(passType) << "\n";
                    }
                }
            }
            for ( ; ai!= pFn->arguments.size(); ++ai ) {
                auto & arg = pFn->arguments[ai];
                if ( !arg->init ) {
                    ss << "\t\tmissing argument " << arg->name << "\n";
                }
            }
            return ss.str();
        }

        vector<FunctionPtr> findMatchingFunctions ( const string & name, const vector<MakeFieldDeclPtr> & arguments, bool inferBlock = false ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            auto inWhichModule = getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->functionsByName.find(funcName);
                if ( itFnList != mod->functionsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    for ( auto & pFn : goodFunctions ) {
                        if ( isVisibleFunc(inWhichModule,getFunctionVisModule(pFn.get())) ) {
                            if ( canCallPrivate(pFn,inWhichModule,program->thisModule.get()) ) {
                                if ( isFunctionCompatible(pFn, arguments, false, inferBlock) ) {
                                    result.push_back(pFn);
                                }
                            }
                        }
                    }
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findMatchingFunctions ( const string & name, const vector<TypeDeclPtr> & types, bool inferBlock = false ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            auto inWhichModule = getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->functionsByName.find(funcName);
                if ( itFnList != mod->functionsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    for ( auto & pFn : goodFunctions ) {
                        if ( isVisibleFunc(inWhichModule,getFunctionVisModule(pFn.get()) ) ) {
                            if ( canCallPrivate(pFn,inWhichModule,program->thisModule.get()) ) {
                                if ( isFunctionCompatible(pFn, types, false, inferBlock) ) {
                                    result.push_back(pFn);
                                }
                            }
                        }
                    }
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findMatchingGenerics ( const string & name, const vector<MakeFieldDeclPtr> & arguments ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            auto inWhichModule = getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->genericsByName.find(funcName);
                if ( itFnList != mod->genericsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    for ( auto & pFn : goodFunctions ) {
                        if ( isVisibleFunc(inWhichModule,getFunctionVisModule(pFn.get())) ) {
                            if ( canCallPrivate(pFn,inWhichModule,program->thisModule.get()) ) {
                                if ( isFunctionCompatible(pFn, arguments, true, true) ) {   // infer block here?
                                    result.push_back(pFn);
                                }
                            }
                        }
                    }
                }
                return true;
            },moduleName);
            return result;
        }

        vector<FunctionPtr> findMatchingGenerics ( const string & name, const vector<TypeDeclPtr> & types ) const {
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            vector<FunctionPtr> result;
            auto inWhichModule = getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                auto itFnList = mod->genericsByName.find(funcName);
                if ( itFnList != mod->genericsByName.end() ) {
                    auto & goodFunctions = itFnList->second;
                    for ( auto & pFn : goodFunctions ) {
                        if ( isVisibleFunc(inWhichModule,getFunctionVisModule(pFn.get())) ) {
                            if ( canCallPrivate(pFn,inWhichModule,program->thisModule.get()) ) {
                                if ( isFunctionCompatible(pFn, types, true, true) ) {   // infer block here?
                                    result.push_back(pFn);
                                }
                            }
                        }
                    }
                }
                return true;
            },moduleName);
            return result;
        }

        void reportFunctionNotFound( const string & name, const string & extra,
                                    const LineInfo & at, const vector<FunctionPtr> & candidateFunctions,
            const vector<TypeDeclPtr> & types, bool inferAuto, bool inferBlocks, bool reportDetails,
                                    CompilationError cerror = CompilationError::function_not_found) {
            TextWriter ss;
            if ( candidateFunctions.size() > 1 ) {
                ss << "candidates:\n";
            } else if ( candidateFunctions.size()==1 ) {
                ss << "\ncandidate function:\n";
            }
            string moduleName, funcName;
            splitTypeName(name, moduleName, funcName);
            auto inWhichModule = getSearchModule(moduleName);
            for ( auto & missFn : candidateFunctions ) {
                ss << "\t";
                if ( missFn->module && !missFn->module->name.empty() && !(missFn->module->name=="$") )
                    ss << missFn->module->name << "::";
                ss << missFn->describe() << "\n";
                if ( reportDetails ) {
                    ss << describeMismatchingFunction(missFn, types, inferAuto, inferBlocks);
                }
                auto visM = getFunctionVisModule(missFn.get());
                if ( !isVisibleFunc(inWhichModule,visM) ) {
                    ss << "\t\tmodule " << visM->name << " is not visible directly from ";
                    if ( inWhichModule->name.empty()) {
                        ss << "the current module\n";
                    } else {
                        ss << inWhichModule->name << "\n";
                    }
                }
                if ( missFn->privateFunction && canCallPrivate(missFn,inWhichModule,program->thisModule.get()) ) {
                    ss << "\t\tfunction is private";
                    if ( missFn->module && !missFn->module->name.empty() ) {
                        ss << " to module " << missFn->module->name;
                    }
                    ss << "\n";
                }
            }
            error(extra, ss.str(), "", at, cerror);
        }

        void reportFunctionNotFound( const string & , const string & extra,
                                    const LineInfo & at, const vector<FunctionPtr> & candidateFunctions,
                                    const vector<MakeFieldDeclPtr> & arguments, bool inferAuto, bool inferBlocks, bool reportDetails ,
                                    CompilationError cerror = CompilationError::function_not_found) {
            TextWriter ss;
            if ( candidateFunctions.size() > 1 ) {
                ss << "candidates:\n";
            } else if ( candidateFunctions.size()==1 ) {
                ss << "\ncandidate function:\n";
            }
            for ( auto & missFn : candidateFunctions ) {
                ss << "\t";
                if ( missFn->module && !missFn->module->name.empty() && !(missFn->module->name=="$") )
                    ss << missFn->module->name << "::";
                ss << missFn->describe() << "\n";
                if ( reportDetails ) {
                    ss << describeMismatchingFunction(missFn, arguments, inferAuto, inferBlocks);
                }
            }
            error(extra, ss.str(), "", at, cerror);
        }

        bool hasUserConstructor ( const string & sna ) const {
            vector<TypeDeclPtr> argDummy;
            auto fnlist = findMatchingFunctions(sna, argDummy);
            return fnlist.size() != 0;  // at least one. if more its an error, but it has one for sure
        }

        bool verifyAnyFunc ( const vector<FunctionPtr> & fnList, const LineInfo & at ) const {
            int genCount = 0;
            int customCount = 0;
            for ( auto & fn : fnList ) {
                if ( !fn->generated ) {
                    customCount ++;
                } else {
                    genCount ++;
                }
            }
            if ( customCount && genCount ) {
                string candidates = program->describeCandidates(fnList);
                error("both generated and custom " + fnList[0]->name + " functions exist for " + fnList[0]->describe(), candidates, "",
                    at, CompilationError::function_not_found);
                return false;
            } else if ( customCount>1 ) {
                string candidates = program->describeCandidates(fnList);
                error("too many custom  " + fnList[0]->name + " functions exist for " + fnList[0]->describe(), candidates, "",
                    at,CompilationError::function_not_found);
                return false;
            } else {
                return true;
            }
        }

        vector<FunctionPtr> getCloneFunc ( const TypeDeclPtr & left, const TypeDeclPtr & right ) const {
            vector<TypeDeclPtr> argDummy;
            argDummy.push_back(make_smart<TypeDecl>(*left));
            argDummy.push_back(make_smart<TypeDecl>(*right));
            return findMatchingFunctions("_::clone", argDummy);
        }

        bool verifyCloneFunc ( const vector<FunctionPtr> & fnList, const LineInfo & at ) const {
            return verifyAnyFunc(fnList, at);
        }

        vector<FunctionPtr> getFinalizeFunc ( const TypeDeclPtr & subexpr ) const {
            vector<TypeDeclPtr> argDummy;
            argDummy.push_back(make_smart<TypeDecl>(*subexpr));
            return findMatchingFunctions("_::finalize", argDummy);
        }

        bool verifyFinalizeFunc ( const vector<FunctionPtr> & fnList, const LineInfo & at ) const {
            return verifyAnyFunc(fnList, at);
        }

        ExprWith * hasMatchingWith ( const string & fieldName ) const {
            for ( auto it=with.rbegin(); it!=with.rend(); ++it ) {
                auto eW = *it;
                if ( auto eWT = eW->with->type ) {
                    StructurePtr pSt;
                    if ( eWT->isStructure() ) {
                        pSt = eWT->structType;
                    } else if ( eWT->isPointer() && eWT->firstType && eWT->firstType->isStructure() ) {
                        pSt = eWT->firstType->structType;
                    }
                    if ( pSt ) {
                        if ( pSt->fieldLookup.find(fieldName) != pSt->fieldLookup.end() ) {
                            return eW;
                        }
                    }
                }
            }
            return nullptr;
        }

        bool inferTypeExpr ( TypeDecl * type ) {
            bool any = false;
            for ( size_t i=0; i!=type->dim.size(); ++i ) {
                if ( type->dim[i]==TypeDecl::dimConst ) {
                    if ( type->dimExpr[i] ) {
                        if ( auto constExpr = getConstExpr(type->dimExpr[i].get()) ) {
                            if ( constExpr->type->isIndex() ) {
                                auto cI = static_pointer_cast<ExprConstInt>(constExpr);
                                auto dI = cI->getValue();
                                if ( dI>0) {
                                    type->dim[i] = dI;
                                    any = true;
                                } else {
                                    error("array dimension can't be 0 or less",  "", "",
                                        type->at, CompilationError::invalid_array_dimension);
                                }
                            } else {
                                error("array dimension must be int32 or uint32",  "", "",
                                    type->at, CompilationError::invalid_array_dimension);
                            }
                        } else {
                            error("array dimension must be constant",  "", "",
                                type->at, CompilationError::invalid_array_dimension);
                        }
                    } else {
                        error("can't deduce array dimension",  "", "",
                            type->at, CompilationError::invalid_array_dimension);
                    }
                }
            }
            if ( type->firstType ) {
                any |= inferTypeExpr(type->firstType.get());
            }
            if ( type->secondType ) {
                any |= inferTypeExpr(type->secondType.get());
            }
            for ( auto & argType : type->argTypes ) {
                any |= inferTypeExpr(argType.get());
            }
            return any;
        }

    protected:

    // type
        virtual void preVisit ( TypeDecl * type ) override {
            if ( inferTypeExpr(type) ) {
                reportAstChanged();
            }
        }

        virtual  TypeDeclPtr visitAlias ( TypeDecl * td, const string & ) override {
            if ( td->isAlias() ) {
                if ( auto ta = inferAlias(td) ) {
                    if ( ta->isAutoOrAlias() ) {
                        ta = inferAlias(td);
                        error("kaboom. can't be infer " + td->describe(),  "", "",
                            td->at, CompilationError::invalid_type);
                        return td;
                    }
                    return ta;
                } else {
                    ta = inferAlias(td);
                    error("can't be inferred " + td->describe(),  "", "",
                        td->at, CompilationError::invalid_type);
                }
            }
            return td;
        }

    // enumeration

        ExpressionPtr makeEnumConstValue(Enumeration * enu, int64_t nextInt) const {
            vec4f nextV = v_zero();
            switch (enu->baseType) {
            case Type::tInt8:       nextV = cast<int8_t>  ::from(int8_t  (nextInt)); break;
            case Type::tUInt8:      nextV = cast<uint8_t> ::from(uint8_t (nextInt)); break;
            case Type::tInt16:      nextV = cast<int16_t> ::from(int16_t (nextInt)); break;
            case Type::tUInt16:     nextV = cast<uint16_t>::from(uint16_t(nextInt)); break;
            case Type::tInt:        nextV = cast<int32_t> ::from(int32_t (nextInt)); break;
            case Type::tUInt:       nextV = cast<uint32_t>::from(uint32_t(nextInt)); break;
            case Type::tBitfield:   nextV = cast<uint32_t>::from(uint32_t(nextInt)); break;
            case Type::tInt64:      nextV = cast<int64_t> ::from(int64_t (nextInt)); break;
            case Type::tUInt64:     nextV = cast<uint64_t>::from(uint64_t(nextInt)); break;
            default: DAS_ASSERTF(0,"we should not be here. unsupported enum type");
            }
            auto nextValue = Program::makeConst(enu->at, enu->makeBaseType(), nextV);
            nextValue->type = enu->makeBaseType();
            return nextValue;
        }

        virtual ExpressionPtr visitEnumerationValue ( Enumeration * enu, const string & name, Expression * value, bool last ) override {
            if ( !value ) {
                if ( lastEnuValue ) {
                    if ( lastEnuValue->rtti_isConstant() && lastEnuValue->type && lastEnuValue->type->isInteger() ) {
                        reportAstChanged();
                        int64_t nextInt = getConstExprIntOrUInt(lastEnuValue) + 1;
                        auto nextValue = makeEnumConstValue(enu, nextInt);
                        lastEnuValue = nextValue.get();
                        return nextValue;
                    } else {
                        error("enumeration value " + name + " can't be infered yet",  "", "",
                            enu->at);
                    }
                } else {
                    reportAstChanged();
                    auto zeroValue = Program::makeConst(enu->at, enu->makeBaseType(), v_zero());
                    zeroValue->type = enu->makeBaseType();
                    lastEnuValue = zeroValue.get();
                    return zeroValue;
                }
            } else {
                if ( !value->type ) {
                    error("enumeration value " + name + " can't be infered yet",  "", "",
                        value->at);
                } else if ( !value->type || !value->type->isInteger() ) {
                    error("enumeration value " + name + " has to be signed or unsigned integer of any size", "", "",
                          value->at, CompilationError::invalid_enumeration);
                } else if ( !value->rtti_isConstant() ) {
                    error("enumeration value " + name + " must be constant", "", "",
                          value->at, CompilationError::invalid_enumeration);
                } else if (value->type->baseType != enu->baseType) {
                    reportAstChanged();
                    int64_t thisInt = getConstExprIntOrUInt(value);
                    auto thisValue = makeEnumConstValue(enu, thisInt);
                    lastEnuValue = thisValue.get();
                    return thisValue;
                }
            }
            lastEnuValue = value;
            return Visitor::visitEnumerationValue(enu, name, value, last);
        }

        virtual void preVisit ( Enumeration * enu ) override {
            Visitor::preVisit(enu);
            lastEnuValue = nullptr;
        }

        virtual EnumerationPtr visit ( Enumeration * enu ) override {
            lastEnuValue = nullptr;
            return Visitor::visit(enu);
        }

    // strcuture
        virtual void preVisit ( Structure * that ) override {
            Visitor::preVisit(that);
            fieldOffset = 0;
            cppLayout = that->cppLayout;
            cppLayoutPod = !that->cppLayoutNotPod;
            cppLayoutParent = nullptr;
            that->fieldLookup.clear();
            fieldIndex = 0;
        }
        virtual void preVisitStructureField ( Structure * that, Structure::FieldDeclaration & decl, bool last ) override {
            Visitor::preVisitStructureField(that, decl, last);
            that->fieldLookup[decl.name] = fieldIndex++;
            if ( decl.type->isAuto() && !decl.init) {
                error("structure field type can't be infered, it needs an initializer", "", "",
                      decl.at, CompilationError::cant_infer_missing_initializer );
            }
        }
        virtual void visitStructureField ( Structure * st, Structure::FieldDeclaration & decl, bool ) override {
            if ( decl.type && decl.type->isExprType() ) {
                return;
            }
            if ( decl.parentType ) {
                auto pf = st->parent->findField(decl.name);
                if ( !pf->type->isAutoOrAlias() ) {
                    decl.type = make_smart<TypeDecl>(*pf->type);
                    decl.parentType = false;
                    decl.type->sanitize();
                    reportAstChanged();
                } else {
                    error("not fully resolved yet",  "", "", decl.at);
                }
                return;
            }
            if ( decl.type->isAlias() ) {
                if ( auto aT = inferAlias(decl.type) ) {
                    decl.type = aT;
                    decl.type->sanitize();
                    reportAstChanged();
                } else {
                    error("undefined type " + decl.type->describe(),  "", "",
                        decl.at, CompilationError::invalid_structure_field_type );
                }
            }
            if ( decl.type->isAuto() && decl.init && decl.init->type ) {
                auto varT = TypeDecl::inferGenericType(decl.type, decl.init->type);
                if ( !varT ) {
                    error("structure field initialization type can't be infered, "
                          + decl.type->describe() + " = " + decl.init->type->describe(), "", "",
                          decl.at, CompilationError::invalid_structure_field_type );
                } else {
                    TypeDecl::applyAutoContracts(varT, decl.type);
                    decl.type = varT;
                    decl.type->ref = false;
                    decl.type->sanitize();
                    reportAstChanged();
                }
            }
            if ( isCircularType(decl.type) ) {
                return;
            }
            if ( decl.type->isVoid() ) {
                error("structure field type can't be declared void", "", "",
                    decl.at,CompilationError::invalid_structure_field_type);
            } else if ( decl.type->ref ) {
                error("structure field type can't be declared a reference", "", "",
                    decl.at,CompilationError::invalid_structure_field_type);
            }
            if ( decl.init ) {
                if ( decl.init->type ) {
                    if ( !canCopyOrMoveType(decl.type,decl.init->type,TemporaryMatters::yes) ) {
                        error("structure field initialization type mismatch, "
                              + decl.type->describe() + " = " + decl.init->type->describe(),  "", "",
                            decl.at,CompilationError::invalid_initialization_type);
                    } else if ( !decl.type->canCopy() && !decl.moveSemantics ) {
                        error("field " + decl.name + " can't be copied, use <- instead; " + decl.type->describe(), "", "",
                              decl.init->at, CompilationError::invalid_initialization_type );
                    } else if ( !decl.init->type->canCopy() && !decl.init->type->canMove() ) {
                        error("field " + decl.name + "can't be initialized at all; " + decl.init->type->describe(),  "", "",
                            decl.at,CompilationError::invalid_initialization_type);
                    } else if (decl.moveSemantics && decl.init->type->isConst()) {
                        error("can't move from a constant value " + decl.init->type->describe(), "", "",
                            decl.init->at, CompilationError::cant_move);
                    }
                } else if ( !decl.type->isAuto() ){
                    if ( decl.init->rtti_isCast() ) {
                        auto castExpr = static_pointer_cast<ExprCast>(decl.init);
                        if ( castExpr->castType->isAuto() ) {
                            reportAstChanged();
                            castExpr->castType = make_smart<TypeDecl>(*decl.type);
                        }
                    }
                }
            }
            // TODO: verify. correct test is in fact the one bellow
            //  if ( isFullySealedType(decl.type) ) {
            // but the auto \ alias test may be sufficient
            if ( !decl.type->isAutoOrAlias() ) {
                int fieldAlignemnt = decl.type->getAlignOf();
                int fa = fieldAlignemnt - 1;
                if ( cppLayout ) {
                    auto fp = st->findFieldParent(decl.name);
                    if ( fp!=cppLayoutParent ) {
                        if (DAS_NON_POD_PADDING || cppLayoutPod) {
                            fieldOffset = cppLayoutParent ? cppLayoutParent->getSizeOf() : 0;
                        }
                        cppLayoutParent = fp;
                    }
                }
                fieldOffset = (fieldOffset + fa) & ~fa;
                decl.offset = int(fieldOffset);
                fieldOffset += decl.type->getSizeOf();
            }
            verifyType(decl.type);
        }
        virtual StructurePtr visit ( Structure * var ) override {
            if ( !var->genCtor && var->hasAnyInitializers() ) {
                if ( !hasUserConstructor(var->name) ) {
                    auto ctor = makeConstructor(var);
                    ctor->exports = program->options.getBoolOption("always_export_initializer", false);
                    extraFunctions.push_back(ctor);
                    var->genCtor = true;
                    reportAstChanged();
                } else if ( !var->isClass ) {
                    error("structure already has user defined initializer", "", "",
                          var->at, CompilationError::structure_already_has_initializer);
                }
            }
            auto tt = make_smart<TypeDecl>(Type::tStructure);
            tt->structType = var;
            if ( isCircularType(tt) ) {
                error("type creates circular dependency",  "", "",
                    var->at,CompilationError::invalid_type);
            }
            return Visitor::visit(var);
        }
    // globals
        virtual void preVisitGlobalLet ( const VariablePtr & var ) override {
            Visitor::preVisitGlobalLet(var);
            if ( var->type->isAuto() && !var->init) {
                error("global variable type can't be infered, it needs an initializer", "", "",
                      var->at, CompilationError::cant_infer_missing_initializer );
            }
            if ( var->type->isAlias() ) {
                if ( auto aT = inferAlias(var->type) ) {
                    var->type = aT;
                    reportAstChanged();
                } else {
                    error("undefined type " + var->type->describe(),  "", "",
                        var->at, CompilationError::invalid_type );
                }
            }
        }
        virtual ExpressionPtr visitGlobalLetInit ( const VariablePtr & var, Expression * init ) override {
            if ( !var->init->type ) return Visitor::visitGlobalLetInit(var, init);
            if ( var->type->isAuto() ) {
                auto varT = TypeDecl::inferGenericInitType(var->type, var->init->type);
                if ( !varT || varT->isAlias() ) {
                    error("global variable " + var->name + " initialization type can't be infered, "
                          + var->type->describe() + " = " + var->init->type->describe(), "", "",
                          var->at, CompilationError::cant_infer_mismatching_restrictions );
                } else {
                    varT->ref = false;
                    TypeDecl::applyAutoContracts(varT, var->type);
                    var->type = varT;
                    reportAstChanged();
                }
            } else if ( !canCopyOrMoveType(var->type,var->init->type,TemporaryMatters::no) ) {
                error("global variable " + var->name + " initialization type mismatch, "
                      + var->type->describe() + " = " + var->init->type->describe(),  "", "",
                    var->at, CompilationError::invalid_initialization_type);
            } else if ( var->type->ref && !var->type->isConst() && var->init->type->isConst() ) {
                error("global variable " + var->name + " initialization type mismatch, const matters "
                      + var->type->describe() + " = " + var->init->type->describe(),  "", "",
                    var->at, CompilationError::invalid_initialization_type);
            } else if ( !var->init_via_clone && (!var->init->type->canCopy() && !var->init->type->canMove()) ) {
                error("global variable " + var->name + " can't be initialized at all. " + var->type->describe(), "", "",
                      var->at, CompilationError::invalid_initialization_type);
            } else if ( var->init_via_move && var->init->type->isConst() ) {
                error("global variable " + var->name + " can't init (move) from a constant value",  "", "",
                    var->at, CompilationError::cant_move);
            } else if ( !(var->init_via_move || var->init_via_clone) && !var->init->type->canCopy() ) {
                error("global variable " + var->name + " can't be copied",  "", "",
                    var->at, CompilationError::cant_copy);
            } else if ( var->init_via_move && !var->init->type->canMove() ) {
                error("global variable " + var->name + " can't be moved",  "", "",
                    var->at, CompilationError::cant_move);
            } else if ( var->init_via_clone && !var->init->type->canClone() ) {
                auto varType = make_smart<TypeDecl>(*var->type);
                varType->ref = true;
                auto fnList = getCloneFunc(varType, var->init->type);
                if ( fnList.size() && verifyCloneFunc(fnList, var->at) ) {
                    return promoteToCloneToMove(var);
                } else {
                    error("global variable " + var->name + " can't be cloned",  "", "",
                        var->at, CompilationError::cant_copy);
                }
            } else {
                if ( var->init_via_clone ) {
                    return promoteToCloneToMove(var);
                }
            }
            return Visitor::visitGlobalLetInit(var, init);
        }
        virtual VariablePtr visitGlobalLet ( const VariablePtr & var ) override {
            if ( var->type && var->type->isExprType() ) {
                return Visitor::visitGlobalLet(var);
            }
            if ( isCircularType(var->type) ) {
                return Visitor::visitGlobalLet(var);
            }
            if ( var->type->ref )
                error("global variable can't be declared a reference",  "", "",
                    var->at, CompilationError::invalid_variable_type);
            if ( var->type->isVoid() )
                error("global variable can't be declared void",  "", "",
                    var->at, CompilationError::invalid_variable_type);
            if ( var->type->isHandle() && !var->type->annotation->isLocal() )
                error("can't have a global variable of handled type " + var->type->annotation->name, "", "",
                      var->at, CompilationError::invalid_variable_type);
            if ( !var->type->constant && var->global_shared )
                error("shared global variable must be constant", "", "",
                      var->at, CompilationError::invalid_variable_type);
            if ( var->global_shared && !var->init )
                error("shared global variable must be initialized", "", "",
                      var->at, CompilationError::invalid_variable_type);
            if ( var->global_shared && !var->type->isShareable() ) {
                if ( !(var->type->isSimpleType(Type::tLambda) && program->policies.allow_shared_lambda) ) {
                    error("this variable type can't be shared, " + var->type->describe(), "", "",
                        var->at, CompilationError::invalid_variable_type);
                }
            }
            verifyType(var->type);
            return Visitor::visitGlobalLet(var);
        }
    // function
        bool safeExpression ( Expression * expr ) const {
            if ( unsafeDepth ) return true;
            if ( expr->alwaysSafe ) return true;
            return false;
        }
        virtual void preVisit ( Function * f ) override {
            Visitor::preVisit(f);
            unsafeDepth = 0;
            func = f;
            func->hasReturn = false;
        }
        virtual void preVisitArgument ( Function * fn, const VariablePtr & var, bool lastArg ) override {
            Visitor::preVisitArgument(fn, var, lastArg);
            if ( var->type->isAlias() ) {
                if ( auto aT = inferAlias(var->type) ) {
                    var->type = aT;
                    reportAstChanged();
                } else {
                    error("undefined type " + var->type->describe(),  "", "",
                        var->at, CompilationError::type_not_found );
                }
            }
            if ( var->type->ref && var->type->isRefType() ) {   // silently fix a : Foo& into a : Foo
                var->type->ref = false;
            }
        }
        virtual ExpressionPtr visitArgumentInit ( Function * f, const VariablePtr & arg, Expression * that ) override {
            if (arg->type->isAuto() && arg->init->type) {
                auto varT = TypeDecl::inferGenericType(arg->type, arg->init->type);
                if ( !varT ) {
                    error("generic argument type can't be infered, "
                        + arg->type->describe() + " = " + arg->init->type->describe(), "", "",
                        arg->at, CompilationError::cant_infer_generic );
                } else {
                    TypeDecl::applyAutoContracts(varT, arg->type);
                    arg->type = varT;
                    reportAstChanged();
                    return Visitor::visitArgumentInit(f, arg, that);
                }
            }
            if ( !arg->init->type || !arg->type->isSameType(*arg->init->type, RefMatters::no, ConstMatters::no, TemporaryMatters::no) ) {
                error("function argument default value type mismatch " + arg->type->describe()
                    + " vs " + (arg->init->type ? arg->init->type->describe() : "???"),  "", "",
                    arg->init->at, CompilationError::invalid_type);
            }
            if (arg->init->type && arg->type->ref && !arg->init->type->ref ) {
                error("function argument default value type mismatch " + arg->type->describe()
                    + " vs " + arg->init->type->describe() + ", reference matters", "", "",
                    arg->init->at, CompilationError::invalid_type);
            }
            return Visitor::visitArgumentInit(f, arg, that);
        }
        virtual VariablePtr visitArgument ( Function * fn, const VariablePtr & var, bool lastArg ) override {
            if ( var->type->isAuto() ) {
                error("unresolved generics are not supported",  "", "",
                    var->at, CompilationError::cant_infer_generic );
            }
            verifyType(var->type,true);
            return Visitor::visitArgument(fn, var, lastArg);
        }
        virtual FunctionPtr visit ( Function * that ) override {
            // if function got no 'result', function is a void function
            if ( !func->hasReturn ) {
                if ( func->result->isAuto() ) {
                    func->result = make_smart<TypeDecl>(Type::tVoid);
                    reportAstChanged();
                } else if ( !func->result->isVoid() ){
                    error("function does not return a value", "", "",
                        func->at, CompilationError::expecting_return_value);
                }
            }
            if  ( func->result->isAlias() ) {
                if ( auto aT = inferAlias(func->result) ) {
                    func->result = aT;
                    func->result->sanitize();
                    reportAstChanged();
                } else {
                    error("undefined type " + func->result->describe(),  "", "",
                        func->at, CompilationError::type_not_found );
                }
            }
            verifyType(func->result);
            if ( !func->result->isReturnType() ) {
                error("not a valid function return type " + func->result->describe(), "", "",
                      func->result->at,CompilationError::invalid_return_type);
            }
            if ( func->result->isRefType() ) {
                if ( func->result->canCopy() ) {
                    func->copyOnReturn = true;
                    func->moveOnReturn = false;
                } else if ( func->result->canMove() ) {
                    func->copyOnReturn = false;
                    func->moveOnReturn = true;
                } else {
                    // the error will be reported in the inferReturnType
                    /*
                    error("this type can't be copied or moved. not a valid function return type "
                          + func->result->describe(),func->result->at,CompilationError::invalid_return_type);
                    */
                }
            } else {
                func->copyOnReturn = false;
                func->moveOnReturn = false;
            }
            // if any of this asserts failed, there is logic error in how we pop
            DAS_ASSERT(scopes.size()==0);
            DAS_ASSERT(blocks.size()==0);
            DAS_ASSERT(local.size()==0);
            DAS_ASSERT(with.size()==0);
            labels.clear();
            func.reset();
            return Visitor::visit(that);
        }
    // any expression
        virtual void preVisitExpression ( Expression * expr ) override {
            Visitor::preVisitExpression(expr);
            expr->type.reset();
        }
    // const
        vec4f getEnumerationValue( ExprConstEnumeration * expr, bool & infered ) const {
            infered = false;
            auto cfa = expr->enumType->find(expr->text);
            if ( !cfa.second ) {
                return v_zero();
            }
            if ( !cfa.first || !cfa.first->rtti_isConstant() ) {
                return v_zero();
            }
            vec4f envalue = v_zero();
            int64_t iou = getConstExprIntOrUInt(cfa.first);
            switch ( expr->enumType->baseType) {
            case Type::tInt8:
            case Type::tUInt8:      { int8_t tv = int8_t(iou); memcpy(&envalue, &tv, sizeof(int8_t)); break; }
            case Type::tInt16:
            case Type::tUInt16:     { int16_t tv = int16_t(iou); memcpy(&envalue, &tv, sizeof(int16_t)); break; }
            case Type::tInt:
            case Type::tUInt:
            case Type::tBitfield:   { int32_t tv = int32_t(iou); memcpy(&envalue, &tv, sizeof(int32_t)); break; }
            case Type::tInt64:
            case Type::tUInt64:     { memcpy(&envalue, &iou, sizeof(int64_t)); break; }
            default:
                DAS_ASSERTF( 0, "we should not even be here. unsupported enumeration type." );
            }
            infered = true;
            return envalue;
        }
        virtual ExpressionPtr visit ( ExprConst * c ) override {
            if ( c->baseType==Type::tEnumeration || c->baseType==Type::tEnumeration8 ||
                c->baseType==Type::tEnumeration16 ) {
                auto cE = static_cast<ExprConstEnumeration *>(c);
                bool infE = false;
                c->value = getEnumerationValue(cE, infE);
                if ( infE ) {
                    c->type = cE->enumType->makeEnumType();
                    c->type->constant = true;
                } else {
                    error("enumeration value not infered yet",  "", "",
                        c->at, CompilationError::invalid_enumeration);
                    c->type.reset();
                }
            } else if ( c->baseType==Type::tBitfield ) {
                auto cB = static_cast<ExprConstBitfield *>(c);
                if ( cB->bitfieldType ) {
                    c->type = make_smart<TypeDecl>(*cB->bitfieldType);
                    c->type->ref = false;
                } else {
                    c->type = make_smart<TypeDecl>(Type::tBitfield);
                }
                c->type->constant = true;
            } else if ( c->baseType==Type::tPointer ) {
                c->type = make_smart<TypeDecl>(c->baseType);
                c->type->constant = true;
                c->type->smartPtr = (static_cast<ExprConstPtr *>(c))->isSmartPtr;
            } else {
                c->type = make_smart<TypeDecl>(c->baseType);
                c->type->constant = true;
            }
            return Visitor::visit(c);
        }
    // ExprUnsafe
        virtual void preVisit ( ExprUnsafe * expr ) override {
            Visitor::preVisit(expr);
            unsafeDepth ++;
        }
        virtual ExpressionPtr visit ( ExprUnsafe * expr ) override {
            unsafeDepth --;
            return Visitor::visit(expr);
        }
    // ExprGoto
        Expression * findLabel ( int32_t label ) const {
            for ( auto it = scopes.rbegin(); it!=scopes.rend(); ++it ) {
                auto blk = *it;
                for ( const auto & ex : blk->list ) {
                    if ( ex->rtti_isLabel() ) {
                        auto lab = static_pointer_cast<ExprLabel>(ex);
                        if ( lab->label == label ) {
                            return lab.get();
                        }
                    }
                }
            }
            return nullptr;
        }
        virtual ExpressionPtr visit ( ExprGoto * expr ) override {
            if ( expr->subexpr ) {
                if ( !expr->subexpr->type ) return Visitor::visit(expr);
                expr->subexpr = Expression::autoDereference(expr->subexpr);
                if ( !expr->subexpr->type->isSimpleType(Type::tInt) ) {
                    error("label type has to be int, not " + expr->subexpr->type->describe(),  "", "",
                        expr->at, CompilationError::invalid_label);
                } else {
                    if ( enableInferTimeFolding ) {
                        if ( auto se = getConstExpr(expr->subexpr.get()) ) {
                            auto le = static_pointer_cast<ExprConstInt>(se);
                            expr->label = le->getValue();
                            expr->subexpr = nullptr;
                        }
                    }
                }
            }
            if ( !expr->subexpr && !findLabel(expr->label) ) {
                error("can't find label " + to_string(expr->label),  "", "",
                    expr->at, CompilationError::invalid_label);
            }
            return Visitor::visit(expr);
        }
    // ExprReader
        virtual ExpressionPtr visit ( ExprReader * expr ) override {
            // implement reader macros
            auto errc = ctx.thisProgram->errors.size();
            auto thisModule = ctx.thisProgram->thisModule.get();
            auto substitute = expr->macro->visit(ctx.thisProgram, thisModule, expr);
            if ( substitute ) {
                reportAstChanged();
                return substitute;
            }
            if ( errc==ctx.thisProgram->errors.size() ) {
                error("unsupported read macro " + expr->macro->name,  "", "",
                    expr->at, CompilationError::unsupported_read_macro);
            }
            return Visitor::visit(expr);
        }
    // ExprLabel
        virtual void preVisit ( ExprLabel * expr ) override {
            Visitor::preVisit(expr);
            auto total = ++labels[expr->label];
            if ( total != 1 ) {
                error("duplicate label " + to_string(expr->label),  "", "",
                    expr->at, CompilationError::invalid_label);
            }
        }
    // ExprRef2Value
        virtual ExpressionPtr visit ( ExprRef2Value * expr ) override {
            if ( !expr->subexpr->type ) return Visitor::visit(expr);
            // infer
            if ( !expr->subexpr->type->isRef() ) {
                TextWriter tw;
                tw << *expr->subexpr;
                error("can only dereference a reference", tw.str(), "",
                    expr->at, CompilationError::invalid_type);
            } else if ( !expr->subexpr->type->isSimpleType() && !expr->subexpr->type->isPointer() && !expr->subexpr->type->isEnum() ) {
                error("can only dereference a simple type, not a " + expr->subexpr->type->describe(),  "", "",
                    expr->at, CompilationError::invalid_type);
            } else {
                expr->type = make_smart<TypeDecl>(*expr->subexpr->type);
                expr->type->ref = false;
            }
            return Visitor::visit(expr);
        }
    // ExprAddr
        virtual ExpressionPtr visit ( ExprAddr * expr ) override {
            if (expr->funcType) {
                if (expr->funcType->isAlias()) {
                    auto aT = inferAlias(expr->funcType);
                    if (aT) {
                        expr->funcType = aT;
                        reportAstChanged();
                    } else {
                        error("undefined type " + expr->funcType->describe(),  "", "",
                            expr->at, CompilationError::type_not_found);
                        return Visitor::visit(expr);
                    }
                }
                if (expr->funcType->isAuto()) {
                    error("function of undefined type " + expr->funcType->describe(),  "", "",
                        expr->at, CompilationError::type_not_found);
                    return Visitor::visit(expr);
                }
            }
            expr->func = nullptr;
            vector<FunctionPtr> fns;
            if (expr->funcType) {
                fns = findTypedFuncAddr(expr->target, expr->funcType->argTypes, expr->funcType->firstType);
            } else {
                fns = findFuncAddr(expr->target);
            }
            if ( fns.size()==1 ) {
                expr->func = fns.back().get();
                expr->func->addr = true;
                expr->type = make_smart<TypeDecl>(Type::tFunction);
                expr->type->firstType = make_smart<TypeDecl>(*expr->func->result);
                expr->type->argTypes.reserve ( expr->func->arguments.size() );
                for ( auto & arg : expr->func->arguments ) {
                    auto at = make_smart<TypeDecl>(*arg->type);
                    expr->type->argTypes.push_back(at);
                    expr->type->argNames.push_back(arg->name);
                }
                verifyType(expr->type);
            } else if ( fns.size()==0 ) {
                error("function not found " + expr->target,  "", "",
                    expr->at, CompilationError::function_not_found);
            } else {
                string candidates = program->describeCandidates(fns);
                error("function not found " + expr->target, candidates, "",
                    expr->at, CompilationError::function_not_found);
            }
            if (expr->func && expr->func->builtIn) {
                error("can't get address of builtin function " + expr->func->describe(),  "", "",
                    expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            return Visitor::visit(expr);
        }
    // ExprPtr2Ref
        virtual ExpressionPtr visit ( ExprPtr2Ref * expr ) override {
            if ( !expr->subexpr->type ) return Visitor::visit(expr);
            // safe deref of non-pointer is it
            if ( expr->alwaysSafe && !expr->subexpr->type->isPointer() ) {
                reportAstChanged();
                return expr->subexpr;
            }
            expr->unsafeDeref = func ? func->unsafeDeref : false;
            // infer
            expr->subexpr = Expression::autoDereference(expr->subexpr);
            if ( !expr->subexpr->type->isPointer() ) {
                error("can only dereference a pointer",  "", "",
                    expr->at, CompilationError::cant_dereference);
            } else if ( !expr->subexpr->type->firstType || expr->subexpr->type->firstType->isVoid() ) {
                error("can only dereference a pointer to something",  "", "",
                    expr->at, CompilationError::cant_dereference);
            } else {
                expr->type = make_smart<TypeDecl>(*expr->subexpr->type->firstType);
                expr->type->ref = true;
                expr->type->constant |= expr->subexpr->type->constant;
                propagateTempType(expr->subexpr->type, expr->type); // deref(Foo#?) is Foo#
            }
            return Visitor::visit(expr);
        }
    // ExprRef2Ptr
        virtual ExpressionPtr visit ( ExprRef2Ptr * expr ) override {
            if ( !expr->subexpr->type ) return Visitor::visit(expr);
            // infer
            if ( !expr->subexpr->type->isRef() ) {
                error("can only make a pointer of of a reference",  "", "",
                    expr->at, CompilationError::cant_dereference);
            } else {
                if ( !safeExpression(expr) ) {
                    error("address of reference requires unsafe",  "", "",
                        expr->at, CompilationError::unsafe);
                }
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*expr->subexpr->type);
                expr->type->firstType->ref = false;
                expr->type->constant |= expr->subexpr->type->constant;
                propagateTempType(expr->subexpr->type, expr->type); // addr(Foo#) is Foo#?#
            }
            return Visitor::visit(expr);
        }
    // ExprNullCoalescing
        void propagateAlwaysSafe ( const ExpressionPtr & expr ) {
            if ( expr->alwaysSafe ) return;
            // make a ?as b ?? c always safe
            if (expr->rtti_isSafeAsVariant()) {
                reportAstChanged();
                auto sav = static_pointer_cast<ExprSafeAsVariant>(expr);
                sav->alwaysSafe = true;
                if ( sav->value->type->isPointer() ) {
                    propagateAlwaysSafe(sav->value);
                }
            }
            // make a ?[b] ?? c always safe
            else if ( expr->rtti_isSafeAt() ) {
                reportAstChanged();
                auto sat = static_pointer_cast<ExprSafeAt>(expr);
                sat->alwaysSafe = true;
                if ( sat->subexpr->type->isPointer() ) {
                    propagateAlwaysSafe(sat->subexpr);
                }
            }
            // make a ? b ?? c always safe (we need flag only)
            else if ( expr->rtti_isSafeField() ) {
                reportAstChanged();
                auto saf = static_pointer_cast<ExprSafeField>(expr);
                saf->alwaysSafe = true;
                if ( saf->value->type->isPointer() ) {
                    propagateAlwaysSafe(saf->value);
                }
            }
        }
        virtual ExpressionPtr visit ( ExprNullCoalescing * expr ) override {
            if ( !expr->subexpr->type | !expr->defaultValue->type ) return Visitor::visit(expr);
            // infer
            expr->subexpr = Expression::autoDereference(expr->subexpr);
            auto seT = expr->subexpr->type;
            auto dvT = expr->defaultValue->type;
            if ( !seT->isPointer() ) {
                error("can only dereference a pointer",  "", "",
                    expr->at, CompilationError::cant_dereference);
            } else if ( !seT->firstType || seT->firstType->isVoid() ) {
                error("can only dereference a pointer to something",  "", "",
                    expr->at, CompilationError::cant_dereference);
            } else if ( !seT->firstType->isSameType(*dvT,RefMatters::no, ConstMatters::no, TemporaryMatters::yes) ) {
                error("default value type mismatch in " + seT->firstType->describe() + " vs "
                      + dvT->describe(), "", "",
                    expr->at, CompilationError::cant_dereference);
            } else if ( seT->isRef() && !seT->isConst() && dvT->isConst() ) {
                error("default value type mismatch, constant matters in " + seT->describe() + " vs "
                      + dvT->describe(),  "", "",
                    expr->at, CompilationError::cant_dereference);
            } else {
                expr->type = make_smart<TypeDecl>(*dvT);
                expr->type->constant |= expr->subexpr->type->constant;
                propagateTempType(expr->subexpr->type, expr->type);
                propagateAlwaysSafe(expr->subexpr);
            }
            return Visitor::visit(expr);
        }
    // ExprStaticAssert
        virtual ExpressionPtr visit ( ExprStaticAssert * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            if ( expr->arguments.size()<1 || expr->arguments.size()>2  ) {
                error("static_assert(expr) or static_assert(expr,string)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            expr->autoDereference();
            if ( !expr->arguments[0]->type->isSimpleType(Type::tBool) )
                error("static assert condition must be boolean",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            if ( expr->arguments.size()==2 && !expr->arguments[1]->rtti_isStringConstant() ) {
                error("static assert comment must be string constant",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
                return nullptr;
            }
            expr->type = make_smart<TypeDecl>(Type::tVoid);
            return Visitor::visit(expr);
        }
    // ExprAssert
        virtual ExpressionPtr visit ( ExprAssert * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            if ( expr->arguments.size()<1 || expr->arguments.size()>2  ) {
                error("assert(expr) or assert(expr,string)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            expr->autoDereference();
            if ( !expr->arguments[0]->type->isSimpleType(Type::tBool) )
                error("assert condition must be boolean",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            if ( expr->arguments.size()==2 && !expr->arguments[1]->rtti_isStringConstant() ) {
                error("assert comment must be string constant",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            }
            expr->type = make_smart<TypeDecl>(Type::tVoid);
            return Visitor::visit(expr);
        }
    // ExprDebug
        virtual ExpressionPtr visit ( ExprDebug * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            if ( expr->arguments.size()<1 || expr->arguments.size()>2 ) {
                error("debug(expr) or debug(expr,string)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            if ( expr->arguments.size()==2 && !expr->arguments[1]->rtti_isStringConstant() ) {
                error("debug comment must be string constant",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            }
            expr->type = make_smart<TypeDecl>(*expr->arguments[0]->type);
            return Visitor::visit(expr);
        }
    // ExprMemZero
        virtual ExpressionPtr visit ( ExprMemZero * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            if ( expr->arguments.size()!=1 ) {
                error("memzero(ref expr)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            const auto & arg = expr->arguments[0];
            if ( !arg->type->isRef() ) {
                error("memzero argument must be reference",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            } else if ( arg->type->isConst() ) {
                error("memzero argument can't be constant",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            }
            expr->type = make_smart<TypeDecl>();
            return Visitor::visit(expr);
        }
    // ExprMakeGenerator
        virtual ExpressionPtr visit ( ExprMakeGenerator * expr ) override {
            if ( expr->iterType && expr->iterType->isExprType() ) {
                return Visitor::visit(expr);
            }
            if ( expr->iterType->isAlias() ) {
                auto aT = inferAlias(expr->iterType);
                if ( aT ) {
                    expr->iterType = aT;
                    reportAstChanged();
                } else {
                    error("undefined type " + expr->iterType->describe(),  "", "",
                        expr->at, CompilationError::type_not_found);
                    return Visitor::visit(expr);
                }
            }
            if ( expr->iterType->isAuto() ) {
                error("generator of undefined type " + expr->iterType->describe(),  "", "",
                    expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            } else if ( expr->iterType->isVoid() ) {
                error("generator can't be void (yet)",  "", "",
                    expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            if ( expr->arguments.size()!=1 ) {
                error("expecting generator(closure)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
            } else if ( !expr->arguments[0]->rtti_isMakeBlock() ) {
                error("expecting generator(closure)",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            } else {
                auto mkBlock = static_pointer_cast<ExprMakeBlock>(expr->arguments[0]);
                auto block = static_pointer_cast<ExprBlock>(mkBlock->block);
                if ( auto bT = block->makeBlockType() ) {
                    // TODO: verify
                    // if ( !isFullySealedType(bT) ) {
                    if ( bT->isAutoOrAlias() ) {
                        error("can't infer generator block type",  "", "",
                            expr->at, CompilationError::invalid_block);
                    } else if ( !bT->firstType->isSimpleType(Type::tBool) ) {
                        error("generator must return boolean",  "", "",
                            expr->at, CompilationError::invalid_argument_type);
                    } else if ( !bT->argTypes.empty() ) {
                        error("generator must have no arguments",  "", "",
                            expr->at, CompilationError::invalid_argument_type);
                    } else {
                        // TODO: check validity of the generator type
                        CaptureLambda cl;
                        // we can only capture in-scope variables
                        // i.e stuff BEFORE the scope
                        for ( auto & lv : local )
                            cl.scope.insert(lv);
                        for ( auto & bls : blocks ) {
                            for ( auto & blv : bls->arguments ) {
                                cl.scope.insert(blv);
                            }
                        }
                        block->visit(cl);
                        if ( !cl.fail ) {
                            for ( auto ba : block->arguments ) {
                                cl.capt.erase(ba);
                            }
                            // add "yield" argument
                            bool makeRef = false;
                            if ( !expr->iterType->isVoid() ) {
                                auto yva = make_smart<Variable>();
                                if ( expr->iterType->ref ) {
                                    yva->type = make_smart<TypeDecl>(Type::tPointer);
                                    yva->type->firstType = make_smart<TypeDecl>(*expr->iterType);
                                    yva->type->firstType->ref = false;
                                    yva->type->constant = false;
                                    yva->type->ref = true;
                                    makeRef = true;
                                } else {
                                    yva->type = make_smart<TypeDecl>(*expr->iterType);
                                    yva->type->constant = false;
                                    yva->type->ref = !expr->iterType->isRefType();
                                }
                                yva->name = (makeRef ? "_ryield" : "_yield_") + to_string(block->at.line);
                                yva->at = block->at;
                                yva->generated = true;
                                block->arguments.push_back(yva);
                            }
                            // make it all
                            bool isUnsafe = !safeExpression(expr);
                            if ( verifyCapture(expr->capture, cl, isUnsafe, expr->at) ) {
                                string lname = generateNewLambdaName(block->at);
                                auto ls = generateLambdaStruct(lname, block.get(), cl.capt, expr->capture, true);
                                if ( program->addStructure(ls) ) {
                                    auto pFn = generateLambdaFunction(lname, block.get(), ls, cl.capt, expr->capture, true);
                                    if ( program->addFunction(pFn) ) {
                                        auto pFnFin = generateLambdaFinalizer(lname, block.get(), ls);
                                        if ( program->addFunction(pFnFin) ) {
                                            reportAstChanged();
                                            auto ms = generateLambdaMakeStruct ( ls, pFn, pFnFin, cl.capt, expr->capture, expr->at );
                                            // each ( [[ ]]] )
                                            auto cEach = make_smart<ExprCall>(block->at, makeRef ? "each_ref" : "each");
                                            cEach->generated = true;
                                            cEach->arguments.push_back(ms);
                                            return cEach;
                                        } else {
                                            error("generator finalizer name mismatch",  "", "",
                                                expr->at, CompilationError::invalid_block);
                                        }
                                    } else {
                                        error("generator function name mismatch",  "", "",
                                            expr->at, CompilationError::invalid_block);
                                    }
                                } else {
                                    error("generator struct name mismatch " + ls->name,  "", "",
                                        expr->at, CompilationError::invalid_block);
                                }
                            }
                            // in case of error
                            if ( !expr->iterType->isVoid() ) {
                                block->arguments.pop_back();
                            }
                        }
                    }
                }
            }
            return Visitor::visit(expr);
        }
    // ExprMakeBlock
        bool verifyCapture ( const vector<CaptureEntry> & capture, const CaptureLambda & cl, bool isUnsafe, const LineInfo & at ) {
            for ( auto & cV : cl.capt ) {
                CaptureMode mode = CaptureMode::capture_any;
                auto it = find_if ( capture.begin(), capture.end(), [&] ( const auto & entry ){
                    return entry.name == cV->name;
                });
                if ( it != capture.end() ) {
                    mode = it->mode;
                }
                if ( mode == CaptureMode::capture_any ) {
                    if ( cV->capture_as_ref ) {
                        // this is ok by default
                    } else if ( !cV->type->canCopy() && !cV->type->canMove() ) {
                        error("can't captured variable " + cV->name,  "it can't be copied or moved", "",
                            at, CompilationError::invalid_capture);
                        return false;
                    } else if ( !cV->type->canCopy() && isUnsafe ) {
                        error("implicit capture by move requires unsafe, while capturing " + cV->name,  "", "",
                            at, CompilationError::invalid_capture);
                        return false;
                    }
                } else if ( mode == CaptureMode::capture_by_reference ) {
                    if ( !cV->capture_as_ref && isUnsafe ) {
                        error("capture by reference requires unsafe, while capturing " + cV->name,  "", "",
                            at, CompilationError::invalid_capture);
                        return false;
                    }
                } else if ( mode == CaptureMode::capture_by_move ) {
                    if ( !cV->type->canMove() ) {
                        error("can't move captured variable " + cV->name,  "", "",
                            at, CompilationError::invalid_capture);
                        return false;
                    }
                } else if ( mode == CaptureMode::capture_by_copy ) {
                    if ( !cV->type->canCopy() ) {
                        error("can't copy captured variable " + cV->name,  "", "",
                            at, CompilationError::invalid_capture);
                        return false;
                    }
                }
            }
            return true;
        }
        ExpressionPtr convertBlockToLambda ( ExprMakeBlock * expr ) {
            auto block = static_pointer_cast<ExprBlock>(expr->block);
            if ( auto bT = block->makeBlockType() ) {
                // TODO: verify
                // if ( !isFullySealedType(bT) ) {
                if ( bT->isAutoOrAlias() ) {
                    error("can't infer lambda block type",  "", "",
                        expr->at, CompilationError::invalid_block);
                } else {
                    CaptureLambda cl;
                    // we can only capture in-scope variables
                    // i.e stuff BEFORE the scope
                    for ( auto & lv : local )
                        cl.scope.insert(lv);
                    for ( auto & bls : blocks ) {
                        for ( auto & blv : bls->arguments ) {
                            cl.scope.insert(blv);
                        }
                    }
                    block->visit(cl);
                    if ( !cl.fail ) {
                        for ( auto ba : block->arguments ) {
                            cl.capt.erase(ba);
                        }
                        bool isUnsafe = !safeExpression(expr);
                        if ( verifyCapture(expr->capture, cl, isUnsafe, expr->at) ) {
                            string lname = generateNewLambdaName(block->at);
                            auto ls = generateLambdaStruct(lname, block.get(), cl.capt, expr->capture);
                            if ( program->addStructure(ls) ) {
                                auto pFn = generateLambdaFunction(lname, block.get(), ls, cl.capt, expr->capture, false);
                                if ( program->addFunction(pFn) ) {
                                    auto pFnFin = generateLambdaFinalizer(lname, block.get(), ls);
                                    if ( program->addFunction(pFnFin) ) {
                                        reportAstChanged();
                                        auto ms = generateLambdaMakeStruct ( ls, pFn, pFnFin, cl.capt, expr->capture, expr->at );
                                        return ms;
                                    } else {
                                        error("lambda finalizer name mismatch",  "", "",
                                            expr->at, CompilationError::invalid_block);
                                    }
                                } else {
                                    error("lambda function name mismatch",  "", "",
                                        expr->at, CompilationError::invalid_block);
                                }
                            } else {
                                error("lambda struct name mismatch",  "", "",
                                    expr->at, CompilationError::invalid_block);
                            }
                        }
                    }
                }
            }
            return nullptr;
        }
        ExpressionPtr convertBlockToLocalFunction ( ExprMakeBlock * expr ) {
            auto block = static_pointer_cast<ExprBlock>(expr->block);
            if ( auto bT = block->makeBlockType() ) {
                // TODO: verify
                // if ( !isFullySealedType(bT) ) {
                if ( bT->isAutoOrAlias() ) {
                    error("can't infer local function block type",  "", "",
                        expr->at, CompilationError::invalid_block);
                } else {
                    string lname = generateNewLocalFunctionName(block->at);
                    auto pFn = generateLocalFunction(lname, block.get());
                    if ( program->addFunction(pFn) ) {
                        reportAstChanged();
                        return make_smart<ExprAddr>(expr->at, lname + "`function");
                    } else {
                        error("local function name mismatch",  "", "",
                            expr->at, CompilationError::invalid_block);
                    }
                }
            }
            return nullptr;
        }
        virtual ExpressionPtr visit ( ExprMakeBlock * expr ) override {
            auto block = static_pointer_cast<ExprBlock>(expr->block);
            // can only infer block type, if all argument types are infered
            for ( auto & arg : block->arguments ) {
                if ( arg->type->isAlias() ) {
                    error(reportAliasError(arg->type),  "", "",
                        arg->at, CompilationError::invalid_argument_type);
                    return Visitor::visit(expr);
                }
            }
            expr->type = block->makeBlockType();
            if ( expr->isLambda ) {
                expr->type->baseType = Type::tLambda;
                // TODO: verify
                // if ( isFullySealedType(expr->type) ) {
                if ( !expr->type->isAutoOrAlias() ) {
                    if ( auto btl = convertBlockToLambda(expr) ) {
                        return btl;
                    }
                }
            } else if ( expr->isLocalFunction ) {
                expr->type->baseType = Type::tFunction;
                if ( !expr->type->isAutoOrAlias() ) {
                    if ( auto btl = convertBlockToLocalFunction(expr) ) {
                        return btl;
                    }
                }
            }
            return Visitor::visit(expr);
        }
    // ExprInvoke
        virtual ExpressionPtr visit ( ExprInvoke * expr ) override {
            if ( expr->argumentsFailedToInfer ) {
                auto blockT = expr->arguments[0]->type;
                if ( !blockT ) {
                    // no go, no block
                } else if ( !blockT->isGoodBlockType() && !blockT->isGoodFunctionType() && !blockT->isGoodLambdaType() ) {
                    // no go, not a good block
                } else if ( expr->arguments.size()-1 != blockT->argTypes.size() ) {
                    // no go, not a good argument
                } else {
                    for ( size_t i=0; i != blockT->argTypes.size(); ++i ) {
                        auto & arg = expr->arguments[i+1];
                        auto & passType = arg->type;
                        auto & argType = blockT->argTypes[i];
                        if ( arg->rtti_isCast() && !passType ) {
                            auto argCast = static_pointer_cast<ExprCast>(arg);
                            if ( argCast->castType->isAuto() ) {
                                reportAstChanged();
                                argCast->castType = make_smart<TypeDecl>(*argType);
                            }
                        }
                    }
                }
                return Visitor::visit(expr);
            }
            if ( expr->arguments.size()<1 ) {
                error("expecting invoke(block_or_function_or_lambda) or invoke(block_or_function_or_lambda,...)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            expr->arguments[0] = Expression::autoDereference(expr->arguments[0]);
            auto blockT = expr->arguments[0]->type;
            if ( blockT->isAutoOrAlias() ) {
                error("invoke argument not fully infered " + blockT->describe(),  "", "",
                    expr->at, CompilationError::invalid_argument_type);
                return Visitor::visit(expr);
            }
            if ( !blockT->isGoodBlockType() && !blockT->isGoodFunctionType() && !blockT->isGoodLambdaType() ) {
                error("expecting block, or function, or lambda, not a " + blockT->describe(),  "", "",
                    expr->at, CompilationError::invalid_argument_type);
                 return Visitor::visit(expr);
            }
            if ( expr->arguments.size()-1 != blockT->argTypes.size() ) {
                error("invalid number of arguments, expecting " + blockT->describe(), "", "",
                      expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            if ( blockT->isGoodLambdaType() ) {
                expr->arguments[0] = Expression::autoDereference(expr->arguments[0]);
            }
            for ( size_t i=0; i != blockT->argTypes.size(); ++i ) {
                auto & passType = expr->arguments[i+1]->type;
                auto & argType = blockT->argTypes[i];
                // same type only
                if ( !argType->isSameType(*passType, RefMatters::no, ConstMatters::no, TemporaryMatters::yes) ) {
                    error("incomaptible argument " + to_string(i+1) + " " + passType->describe() + " vs "
                          + argType->describe(),  "", "",
                        expr->at, CompilationError::invalid_argument_type);
                }
                // can't pass non-ref to reff
                if ( argType->isRef() && !passType->isRef() ) {
                    error("incomaptible argument. can't pass non-ref to ref " + to_string(i+1) + " "
                        + passType->describe() + " vs " + argType->describe(), "", "",
                        expr->at, CompilationError::invalid_argument_type);
                }
                // ref types can only add constness
                if ( argType->isRef() && !argType->constant && passType->constant ) {
                    error("incomaptible argument " + to_string(i+1) + " "
                        + passType->describe() + " vs " + argType->describe()
                        + ", passing const to non-const argument", "", "",
                            expr->at, CompilationError::invalid_argument_type);
                }
                // pointer types can only add constant
                if ( argType->isPointer() && !argType->constant && passType->constant ) {
                    error("incomaptible argument " + to_string(i+1)
                        + " " + passType->describe() + " vs " + argType->describe()
                        + ", passing const pointer to non-const pointer argument", "", "",
                          expr->at, CompilationError::invalid_argument_type);
                }
                // TODO: invoke with TEMP types
                if ( !argType->isRef() )
                    expr->arguments[i+1] = Expression::autoDereference(expr->arguments[i+1]);
            }
            if ( blockT->firstType ) {
                expr->type = make_smart<TypeDecl>(*blockT->firstType);
            } else {
                expr->type = make_smart<TypeDecl>();
            }
            return Visitor::visit(expr);
        }
    // ExprErase
        virtual ExpressionPtr visit ( ExprErase * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            if ( expr->arguments.size()!=2 ) {
                error("eraseKey(table,key)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            expr->arguments[1] = Expression::autoDereference(expr->arguments[1]);
            auto containerType = expr->arguments[0]->type;
            auto valueType = expr->arguments[1]->type;
            if ( containerType->isGoodTableType() ) {
                if ( !containerType->firstType->isSameType(*valueType,RefMatters::no, ConstMatters::no, TemporaryMatters::no) )
                    error("key must be of the same type as table<key,...>",  "", "",
                        expr->at, CompilationError::invalid_argument_type);
                expr->type = make_smart<TypeDecl>(Type::tBool);
            } else {
                error("first argument must be fully qualified table",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            }
            valueType->constant = true;
            return Visitor::visit(expr);
        }
    // ExprFind
        virtual ExpressionPtr visit ( ExprFind * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            if ( expr->arguments.size()!=2 ) {
                error("findKey(table,key)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            expr->arguments[1] = Expression::autoDereference(expr->arguments[1]);
            auto containerType = expr->arguments[0]->type;
            auto valueType = expr->arguments[1]->type;
            if ( containerType->isGoodTableType() ) {
                if ( !containerType->firstType->isSameType(*valueType,RefMatters::no, ConstMatters::no, TemporaryMatters::no) )
                    error("key must be of the same type as table<key,...>",  "", "",
                        expr->at, CompilationError::invalid_argument_type);
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*containerType->secondType);
            } else {
                error("first argument must be fully qualified table",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            }
            containerType->constant = true;
            valueType->constant = true;
            return Visitor::visit(expr);
        }
    // ExprKeyExists
        virtual ExpressionPtr visit ( ExprKeyExists * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            if ( expr->arguments.size()!=2 ) {
                error("keyExists(table,key)",  "", "",
                    expr->at, CompilationError::invalid_argument_count);
                return Visitor::visit(expr);
            }
            // infer
            expr->arguments[1] = Expression::autoDereference(expr->arguments[1]);
            auto containerType = expr->arguments[0]->type;
            auto valueType = expr->arguments[1]->type;
            if ( containerType->isGoodTableType() ) {
                if ( !containerType->firstType->isSameType(*valueType,RefMatters::no, ConstMatters::no, TemporaryMatters::no) )
                    error("key must be of the same type as table<key,...>",  "", "",
                        expr->at, CompilationError::invalid_argument_type);
                expr->type = make_smart<TypeDecl>(Type::tBool);
            } else {
                error("first argument must be fully qualified table",  "", "",
                    expr->at, CompilationError::invalid_argument_type);
            }
            containerType->constant = true;
            valueType->constant = true;
            return Visitor::visit(expr);
        }
    // ExprIs
        virtual ExpressionPtr visit ( ExprIs * expr ) override {
            if ( expr->typeexpr->isExprType() ) {
                return Visitor::visit(expr);
            }
            if ( !expr->subexpr->type ) {
                return Visitor::visit(expr);
            }
            // TODO: verify
            if ( expr->typeexpr->isAutoOrAlias() ) {
                error("is " + (expr->typeexpr ? expr->typeexpr->describe() : "...") + " can't be infered", "", "",
                      expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            auto nErrors = program->errors.size();
            if ( expr->typeexpr->isAlias() ) {
                if ( auto eT = inferAlias(expr->typeexpr) ) {
                    expr->typeexpr = eT;
                    reportAstChanged();
                    return Visitor::visit(expr);
                } else {
                    error("undefined type " + expr->typeexpr->describe(), "", "",
                          expr->at, CompilationError::type_not_found);
                    return Visitor::visit(expr);
                }
            }
            if ( expr->typeexpr->isAuto() ) {
                error("is ... auto is undefined, " + expr->typeexpr->describe(), "", "",
                      expr->at, CompilationError::typeinfo_auto);
                return Visitor::visit(expr);
            }
            // TODO: verify, is this even necessary now that we have tests above?
            // if ( !isFullySealedType(expr->typeexpr) ) {
            if ( expr->typeexpr->isAutoOrAlias() ) {
                error("is " + (expr->typeexpr ? expr->typeexpr->describe() : "...") + " can't be fully infered", "", "",
                      expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            verifyType(expr->typeexpr);
            if ( nErrors==program->errors.size() ) {
                reportAstChanged();
                bool isSame = expr->subexpr->type->isSameType(*expr->typeexpr, RefMatters::no, ConstMatters::no, TemporaryMatters::no);
                return make_smart<ExprConstBool>(expr->at, isSame);
            }
            // infer
            return Visitor::visit(expr);
        }

    // ExprTypeInfo
        virtual ExpressionPtr visit ( ExprTypeInfo * expr ) override {
            expr->macro = nullptr;
            if ( expr->typeexpr && expr->typeexpr->isExprType() ) {
                return Visitor::visit(expr);
            }
            if ( expr->subexpr && expr->subexpr->type ) {
                expr->typeexpr = make_smart<TypeDecl>(*expr->subexpr->type);
                expr->typeexpr->ref = false;
            }
            // verify
            if ( !expr->typeexpr  ) {
                error("typeinfo(" + (expr->typeexpr ? expr->typeexpr->describe() : "...") + ") can't be infered",  "", "",
                    expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            auto nErrors = program->errors.size();
            if ( expr->typeexpr->isAlias() ) {
                if ( auto eT = inferAlias(expr->typeexpr) ) {
                    expr->typeexpr = eT;
                    reportAstChanged();
                    return Visitor::visit(expr);
                } else {
                    error("undefined type " + expr->typeexpr->describe(), "", "",
                          expr->at, CompilationError::type_not_found);
                    return Visitor::visit(expr);
                }
            }
            if ( expr->typeexpr->isAuto() ) {
                error("typeinfo(... auto) is undefined, " + expr->typeexpr->describe(), "", "",
                      expr->at, CompilationError::typeinfo_auto);
                return Visitor::visit(expr);
            }
            // TODO: verify. is this even necessary with tests above?
            // if ( !isFullySealedType(expr->typeexpr) ) {
            if ( expr->typeexpr->isAutoOrAlias() ) {
                error("typeinfo(" + (expr->typeexpr ? expr->typeexpr->describe() : "...") + ") can't be fully infered",  "", "",
                    expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            verifyType(expr->typeexpr,true);
            if ( nErrors==program->errors.size() ) {
                if ( expr->trait=="sizeof" ) {
                    reportAstChanged();
                    return make_smart<ExprConstInt>(expr->at, expr->typeexpr->getSizeOf());
                } else if ( expr->trait=="alignof" ) {
                    reportAstChanged();
                    return make_smart<ExprConstInt>(expr->at, expr->typeexpr->getAlignOf());
                } else if ( expr->trait=="is_dim" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->dim.size()!=0);
                } else if ( expr->trait=="dim" ) {
                    if ( expr->typeexpr->dim.size() ) {
                        reportAstChanged();
                        return make_smart<ExprConstInt>(expr->at, expr->typeexpr->dim[0]);
                    } else {
                        error("typeinfo(dim non_array) is prohibited, " + expr->typeexpr->describe(), "", "",
                              expr->at,CompilationError::typeinfo_dim);
                    }
                } else if ( expr->trait=="variant_index" || expr->trait=="safe_variant_index" ) {
                    if ( !expr->typeexpr->isGoodVariantType() ) {
                        if (expr->trait == "variant_index") {
                            error("variant_index only valid for variant, not for " + expr->typeexpr->describe(), "", "",
                                expr->at, CompilationError::invalid_type);
                        } else {
                            reportAstChanged();
                            return make_smart<ExprConstInt>(expr->at, -1);
                        }
                    } else {
                        int32_t index = expr->typeexpr->findArgumentIndex(expr->subtrait);
                        if ( index!=-1 ||  expr->trait=="safe_variant_index" ) {
                            reportAstChanged();
                            return make_smart<ExprConstInt>(expr->at, index);
                        } else {
                            error("variant_index variant " + expr->subtrait + " not found in " + expr->typeexpr->describe(), "", "",
                                expr->at,CompilationError::typeinfo_undefined);
                        }
                    }
                } else if ( expr->trait=="mangled_name" ) {
                    if ( !expr->subexpr ) {
                        error("mangled name requires subexpression", "", "",
                            expr->at,CompilationError::typeinfo_undefined);
                    } else {
                        if ( expr->subexpr->rtti_isAddr() ) {
                            auto eaddr = static_pointer_cast<ExprAddr>(expr->subexpr);
                            if ( !eaddr->func ) {
                                error("mangled name of unknown @@function", "", "",
                                    expr->at,CompilationError::typeinfo_undefined);
                            } else {
                                reportAstChanged();
                                return make_smart<ExprConstString>(expr->at, eaddr->func->getMangledName());
                            }
                        } else {
                            error("unsupported mangled name subexpression ", expr->subexpr->__rtti, "",
                                expr->at,CompilationError::typeinfo_undefined);
                        }
                    }
                } else if ( expr->trait=="typename" ) {
                    reportAstChanged();
                    return make_smart<ExprConstString>(expr->at, expr->typeexpr->describe(TypeDecl::DescribeExtra::no, TypeDecl::DescribeContracts::no));
                } else if ( expr->trait=="undecorated_typename" ) {
                    reportAstChanged();
                    return make_smart<ExprConstString>(expr->at, expr->typeexpr->describe(TypeDecl::DescribeExtra::no, TypeDecl::DescribeContracts::no, TypeDecl::DescribeModule::no));
                } else if ( expr->trait=="fulltypename" ) {
                    reportAstChanged();
                    return make_smart<ExprConstString>(expr->at, expr->typeexpr->describe(TypeDecl::DescribeExtra::no, TypeDecl::DescribeContracts::yes));
                } else if ( expr->trait=="is_pod" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isPod());
                } else if ( expr->trait=="is_raw" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isRawPod());
                } else if ( expr->trait=="is_struct" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isStructure());
                } else if ( expr->trait=="is_class" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isClass());
                } else if ( expr->trait=="is_enum" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isEnum());
                } else if ( expr->trait=="is_bitfield" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isBitfield());
                } else if (expr->trait == "is_string") {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isString());
                } else if ( expr->trait=="is_handle" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isHandle());
                } else if ( expr->trait=="is_ref" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isRef());
                } else if ( expr->trait=="is_ref_type" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isRefType());
                } else if ( expr->trait=="is_ref_value" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, bool(expr->typeexpr->ref));
                } else if ( expr->trait=="is_const" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isConst());
                } else if ( expr->trait=="is_temp" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isTemp());
                } else if ( expr->trait=="is_temp_type" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isTempType());
                } else if ( expr->trait=="is_pointer" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isPointer());
                } else if ( expr->trait=="is_iterator" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isGoodIteratorType());
                } else if ( expr->trait=="is_iterable" ) {
                    reportAstChanged();
                    bool iterable = false;
                    if ( expr->typeexpr->dim.size() ) {
                        iterable = true;
                    } else if ( expr->typeexpr->isGoodIteratorType() ) {
                        iterable = true;
                    } else if ( expr->typeexpr->isGoodArrayType() ) {
                        iterable = true;
                    } else if ( expr->typeexpr->isRange() ) {
                        iterable = true;
                    } else if ( expr->typeexpr->isString() ) {
                        iterable = true;
                    } else if ( expr->typeexpr->isHandle() && expr->typeexpr->annotation->isIterable() ) {
                        iterable = true;
                    }
                    return make_smart<ExprConstBool>(expr->at, iterable);
                } else if ( expr->trait=="is_vector" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isVectorType());
                } else if ( expr->trait=="is_array" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isGoodArrayType());
                } else if ( expr->trait=="is_table" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isGoodTableType());
                } else if ( expr->trait=="is_numeric" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isNumeric());
                } else if ( expr->trait=="is_numeric_comparable" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->isNumericComparable());
                } else if ( expr->trait=="can_copy" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->canCopy());
                } else if ( expr->trait=="can_move" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->canMove());
                } else if ( expr->trait=="can_clone" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->canClone());
                } else if ( expr->trait=="can_delete" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->canDelete());
                } else if ( expr->trait=="need_delete" ) {
                    reportAstChanged();
                    return make_smart<ExprConstBool>(expr->at, expr->typeexpr->needDelete());
                } else if ( expr->trait=="has_field" || expr->trait=="safe_has_field" ) {
                    if ( expr->typeexpr->isStructure() ) {
                        reportAstChanged();
                        return make_smart<ExprConstBool>(expr->at, expr->typeexpr->structType->findField(expr->subtrait));
                    } else if ( expr->typeexpr->isHandle() ) {
                        reportAstChanged();
                        auto ft = expr->typeexpr->annotation->makeFieldType(expr->subtrait);
                        return make_smart<ExprConstBool>(expr->at, ft!=nullptr);
                    } else {
                        if ( expr->trait=="safe_has_field" ) {
                            return make_smart<ExprConstBool>(expr->at, false);
                        } else {
                            error("typeinfo(has_field<" + expr->subtrait
                                  + "> ...) is only defined for structures and handled types, "
                                    + expr->typeexpr->describe(),  "", "",
                                expr->at, CompilationError::typeinfo_undefined);
                        }
                    }
                } else if ( expr->trait=="struct_has_annotation" || expr->trait=="struct_safe_has_annotation" ) {
                    if ( expr->typeexpr->isStructure() ) {
                        reportAstChanged();
                        const auto & ann = expr->typeexpr->structType->annotations;
                        auto it = find_if ( ann.begin(), ann.end(), [&](const AnnotationDeclarationPtr & pa){
                            return pa->annotation->name == expr->subtrait;
                        });
                        return make_smart<ExprConstBool>(expr->at, it!=ann.end());
                    } else {
                        if ( expr->trait=="struct_safe_has_annotation" ) {
                            return make_smart<ExprConstBool>(expr->at, false);
                        } else {
                            error("typeinfo(struct_has_annotation<" + expr->subtrait
                                  + "> ...) is only defined for structures, "
                                    + expr->typeexpr->describe(), "", "",
                                expr->at, CompilationError::typeinfo_undefined);
                        }
                    }
                } else if ( expr->trait=="struct_has_annotation_argument" || expr->trait=="struct_safe_has_annotation_argument" ) {
                    if ( expr->typeexpr->isStructure() ) {
                        const auto & ann = expr->typeexpr->structType->annotations;
                        auto it = find_if ( ann.begin(), ann.end(), [&](const AnnotationDeclarationPtr & pa){
                            return pa->annotation->name == expr->subtrait;
                        });
                        if ( it == ann.end() ) {
                            if ( expr->trait=="struct_safe_has_annotation_argument" ) {
                                return make_smart<ExprConstBool>(expr->at, false);
                            } else {
                                error("typeinfo(struct_has_annotation_argument<" + expr->subtrait
                                      + ";"  + expr->extratrait + "> ...) annotation not found ", "", "",
                                      expr->at, CompilationError::typeinfo_undefined);
                            }
                        } else {
                            reportAstChanged();
                            const auto & args = (*it)->arguments;
                            auto ita = find_if ( args.begin(), args.end(), [&](const AnnotationArgument & arg){
                                return arg.name == expr->extratrait;
                            });
                            return make_smart<ExprConstBool>(expr->at, ita!=args.end());
                        }
                    } else {
                        if ( expr->trait=="struct_safe_has_annotation_argument" ) {
                            return make_smart<ExprConstBool>(expr->at, false);
                        } else {
                            error("typeinfo(struct_has_annotation_argument<" + expr->subtrait
                                  + "> ...) is only defined for structures, "
                                    + expr->typeexpr->describe(),  "", "",
                                expr->at, CompilationError::typeinfo_undefined);
                        }
                    }
                } else if ( expr->trait=="struct_get_annotation_argument" ) {
                    if ( expr->typeexpr->isStructure() ) {
                        const auto & ann = expr->typeexpr->structType->annotations;
                        auto it = find_if ( ann.begin(), ann.end(), [&](const AnnotationDeclarationPtr & pa){
                            return pa->annotation->name == expr->subtrait;
                        });
                        if ( it == ann.end() ) {
                                error("typeinfo(struct_get_annotation_argument<" + expr->subtrait
                                      + ";"  + expr->extratrait + "> ...) annotation not found ", "", "",
                                  expr->at, CompilationError::typeinfo_undefined);
                        } else {
                            const auto & args = (*it)->arguments;
                            auto ita = find_if ( args.begin(), args.end(), [&](const AnnotationArgument & arg){
                                return arg.name == expr->extratrait;
                            });
                            if ( ita == args.end() ) {
                                error("typeinfo(struct_get_annotation_argument<" + expr->subtrait
                                      + ";"  + expr->extratrait + "> ...) annotation argument not found ", "", "",
                                  expr->at, CompilationError::typeinfo_undefined);
                            } else {
                                switch ( ita->type ) {
                                    case Type::tBool:
                                        reportAstChanged();
                                        return make_smart<ExprConstBool>(expr->at, ita->bValue);
                                    case Type::tInt:
                                        reportAstChanged();
                                        return make_smart<ExprConstInt>(expr->at, ita->iValue);
                                    case Type::tFloat:
                                        reportAstChanged();
                                        return make_smart<ExprConstFloat>(expr->at, ita->fValue);
                                    case Type::tString:
                                        reportAstChanged();
                                        return make_smart<ExprConstString>(expr->at, ita->sValue);
                                    default:
                                        error("typeinfo(struct_get_annotation_argument<" + expr->subtrait
                                              + ";"  + expr->extratrait + "> ...) unsupported annotation argument type ", "", "",
                                          expr->at, CompilationError::typeinfo_undefined);
                                }
                            }
                        }
                    } else {
                        error("typeinfo(struct_get_annotation_argument<" + expr->subtrait
                              + "> ...) is only defined for structures, "
                                + expr->typeexpr->describe(), "", "",
                            expr->at, CompilationError::typeinfo_undefined);
                    }
                } else if ( expr->trait=="offsetof" ) {
                    if ( expr->typeexpr->isStructure() ) {
                        auto decl = expr->typeexpr->structType->findField(expr->subtrait);
                        // NOTE: we do need to check if its fully sealed here
                        if ( isFullySealedType(expr->typeexpr) ) {
                            reportAstChanged();
                            return make_smart<ExprConstInt>(expr->at, decl->offset);
                        } else {
                            error("typeinfo(offsetof<" + expr->subtrait
                                  + "> ...) of undefined type, " + expr->typeexpr->describe(), "", "",
                                expr->at, CompilationError::typeinfo_undefined);
                        }
                    } else {
                        error("typeinfo(offsetof<" + expr->subtrait
                              + "> ...) is only defined for structures, " + expr->typeexpr->describe(), "", "",
                            expr->at, CompilationError::typeinfo_undefined);
                    }
                } else {
                    auto mtis = program->findTypeInfoMacro(expr->trait);
                    if ( mtis.size()>1 ) {
                        error("typeinfo(" + expr->trait + " ...) too many macros match the trait", "", "",
                            expr->at, CompilationError::typeinfo_undefined);
                    } else if ( mtis.empty() ) {
                        error("typeinfo(" + expr->trait + " ...) is undefined, " + expr->typeexpr->describe(), "", "",
                            expr->at, CompilationError::typeinfo_undefined);
                    } else {
                        expr->macro = mtis.back().get();
                        string errors;
                        auto cexpr = expr->macro->getAstChange(expr, errors);
                        if ( cexpr ) {
                            reportAstChanged();
                            return cexpr;
                        } else if ( !errors.empty() ) {
                            error("typeinfo(" + expr->trait + " ...) macro reported error; " + errors, "", "",
                                expr->at, CompilationError::typeinfo_macro_error);
                        } else {
                            auto ctype = expr->macro->getAstType(program->library, expr, errors);
                            if ( ctype ) {
                                expr->type = ctype;
                                if ( func && expr->macro->noAot(expr) ) {
                                    func->noAot = true;
                                }
                                return Visitor::visit(expr);
                            } else if ( !errors.empty() ) {
                                error("typeinfo(" + expr->trait + " ...) macro reported error; " + errors, "", "",
                                    expr->at, CompilationError::typeinfo_macro_error);
                            } else {
                                error("typeinfo(" + expr->trait + " ...) is macro integration error; no ast change and no type", "", "",
                                    expr->at, CompilationError::typeinfo_macro_error);
                            }
                        }
                    }
                }
            }
            // infer
            return Visitor::visit(expr);
        }
    // ExprDelete
        void reportMissingFinalizer ( const string & message, const LineInfo & at, const TypeDeclPtr & ftype ) {
            auto fakeCall = make_smart<ExprCall>(at, "_::finalize");
            auto fakeVar = make_smart<ExprVar>(at, "this");
            fakeVar->type = make_smart<TypeDecl>(*ftype);
            fakeCall->arguments.push_back(fakeVar);
            vector<TypeDeclPtr> fakeTypes;
            fakeTypes.push_back(ftype);
            reportMissing(fakeCall.get(), fakeTypes, message, true, CompilationError::function_already_declared);
        }
        virtual ExpressionPtr visit ( ExprDelete * expr ) override {
            if ( !expr->subexpr->type ) return Visitor::visit(expr);
            // lets see if there is clone operator already (a user operator can ignore all the rules bellow)
            if ( !expr->native ) {
                auto fnList = getFinalizeFunc(expr->subexpr->type);
                if ( fnList.size() ) {
                    if ( verifyFinalizeFunc(fnList, expr->at) ) {
                        reportAstChanged();
                        auto fn = fnList[0];
                        // string finalizeName = (fn->module->name.empty() ? "_" : fn->module->name) + "::finalize";
                        string finalizeName = "_::finalize";
                        auto finalizeFn = make_smart<ExprCall>(expr->at, finalizeName);
                        finalizeFn->arguments.push_back(expr->subexpr->clone());
                        return ExpressionPtr(finalizeFn);
                    } else {
                        return Visitor::visit(expr);
                    }
                }
            }
            // infer
            if ( !expr->subexpr->type->canDelete() ) {
                expr->subexpr->type->canDelete();
                error("can't delete " + expr->subexpr->type->describe(), "", "",
                      expr->at, CompilationError::bad_delete);
            } else if ( !expr->subexpr->type->isRef() ) {
                error("can only delete reference " + expr->subexpr->type->describe(), "", "",
                      expr->at, CompilationError::bad_delete);
            } else if ( expr->subexpr->type->isConst() ) {
                error("can't delete constant expression " + expr->subexpr->type->describe(), "", "",
                      expr->at, CompilationError::bad_delete);
            } else if ( expr->subexpr->type->isPointer() ) {
                if ( !safeExpression(expr) ) {
                    error("delete of pointer requires unsafe",  "", "",
                        expr->at, CompilationError::unsafe);
                } else if ( expr->subexpr->type->firstType && expr->subexpr->type->firstType->isHandle() &&
                    expr->subexpr->type->firstType->annotation->isSmart() && !expr->subexpr->type->smartPtr ) {
                        error("can't delete smart pointer type via regular pointer delete",  "", "",
                            expr->at, CompilationError::bad_delete);
                } else if ( !expr->native ) {
                    if ( expr->subexpr->type->firstType && expr->subexpr->type->firstType->needDelete() ) {
                        auto ptrf = getFinalizeFunc(expr->subexpr->type);
                        if ( ptrf.size()==0 ) {
                            auto fnDel = generatePointerFinalizer(expr->subexpr->type, expr->at);
                            if ( !program->addFunction(fnDel) ) {
                                reportMissingFinalizer("finalizer mismatch ", expr->at, expr->subexpr->type);
                                return Visitor::visit(expr);
                            }
                        } else if ( ptrf.size() > 1 ) {
                            string candidates = program->describeCandidates(ptrf);
                            error("too many finalizers", candidates, "",
                                expr->at, CompilationError::function_already_declared);
                            return Visitor::visit(expr);
                        }
                        reportAstChanged();
                        expr->native = true;
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::finalize");
                        cloneFn->arguments.push_back(expr->subexpr->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        reportAstChanged();
                        expr->native = true;
                    }
                }
            } else {
                auto finalizeType = expr->subexpr->type;
                if ( finalizeType->isGoodIteratorType() ) {
                    reportAstChanged();
                    auto cloneFn = make_smart<ExprCall>(expr->at, "_builtin_iterator_delete");
                    cloneFn->arguments.push_back(expr->subexpr->clone());
                    return ExpressionPtr(cloneFn);
                } else if ( finalizeType->isGoodArrayType() || finalizeType->isGoodTableType() ) {
                    reportAstChanged();
                    auto cloneFn = make_smart<ExprCall>(expr->at, "_::finalize");
                    cloneFn->arguments.push_back(expr->subexpr->clone());
                    return ExpressionPtr(cloneFn);
                } else if ( finalizeType->isStructure() ) {
                    auto fnDel = generateStructureFinalizer(finalizeType->structType);
                    if ( program->addFunction(fnDel) ) {
                        reportAstChanged();
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::finalize");
                        cloneFn->arguments.push_back(expr->subexpr->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        reportMissingFinalizer("finalizer mismatch ", expr->at, expr->subexpr->type);
                        return Visitor::visit(expr);
                    }
                } else if ( finalizeType->isTuple() ) {
                    auto fnDel = generateTupleFinalizer(expr->at, finalizeType);
                    if ( program->addFunction(fnDel) ) {
                        reportAstChanged();
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::finalize");
                        cloneFn->arguments.push_back(expr->subexpr->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        reportMissingFinalizer("finalizer mismatch ", expr->at, expr->subexpr->type);
                        return Visitor::visit(expr);
                    }
                } else if ( finalizeType->isVariant() ) {
                    auto fnDel = generateVariantFinalizer(expr->at, finalizeType);
                    if ( program->addFunction(fnDel) ) {
                        reportAstChanged();
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::finalize");
                        cloneFn->arguments.push_back(expr->subexpr->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        reportMissingFinalizer("finalizer mismatch ", expr->at, expr->subexpr->type);
                        return Visitor::visit(expr);
                    }
                } else if ( finalizeType->dim.size() ) {
                    reportAstChanged();
                    auto cloneFn = make_smart<ExprCall>(expr->at, "finalize_dim");
                    cloneFn->arguments.push_back(expr->subexpr->clone());
                    return ExpressionPtr(cloneFn);
                } else {
                    expr->type = make_smart<TypeDecl>();
                    return Visitor::visit(expr);
                }
            }
            return Visitor::visit(expr);
        }
    // ExprCast
        TypeDeclPtr castStruct ( const LineInfo & at, const TypeDeclPtr & subexprType, const TypeDeclPtr & castType, bool upcast ) const {
            auto seT = subexprType;
            auto cT = castType;
            if ( seT->isStructure() ) {
                if ( cT->isStructure() ) {
                    bool compatibleCast;
                    if ( upcast ) {
                        compatibleCast = seT->structType->isCompatibleCast(*cT->structType);
                    } else {
                        compatibleCast = cT->structType->isCompatibleCast(*seT->structType);
                    }
                    if ( compatibleCast ) {
                        auto exprType = make_smart<TypeDecl>(*cT);
                        exprType->ref = seT->ref;
                        exprType->constant = seT->constant;
                        return exprType;
                    } else {
                        error("incompatible cast, can't cast " + seT->structType->name
                              + " to " + cT->structType->name,  "", "",
                            at, CompilationError::invalid_cast);
                    }
                } else {
                    error("invalid cast, expecting structure",  "", "",
                        at, CompilationError::invalid_cast);
                }
            } else if ( seT->isPointer() && seT->firstType && seT->firstType->isStructure() ) {
                if ( cT->isPointer() && cT->firstType->isStructure() ) {
                    bool compatibleCast;
                    if ( upcast ) {
                        compatibleCast = seT->firstType->structType->isCompatibleCast(*cT->firstType->structType);
                    } else {
                        compatibleCast = cT->firstType->structType->isCompatibleCast(*seT->firstType->structType);
                    }
                    if ( compatibleCast ) {
                        auto exprType = make_smart<TypeDecl>(*cT);
                        exprType->ref = seT->ref;
                        exprType->constant = seT->constant;
                        return exprType;
                    } else {
                        error("incompatible cast, can't cast " + seT->firstType->structType->name
                              + "? to " + cT->firstType->structType->name + "?",  "", "",
                            at, CompilationError::invalid_cast);
                    }
                } else {
                    error("invalid cast, expecting structure pointer",  "", "",
                        at, CompilationError::invalid_cast);
                }
            }
            return nullptr;
        }
        TypeDeclPtr castFunc ( const LineInfo & at, const TypeDeclPtr & subexprType, const TypeDeclPtr & castType, bool upcast ) const {
            auto seTF = subexprType;
            auto cTF = castType;
            if ( seTF->argTypes.size()!=cTF->argTypes.size() ) {
                error("invalid cast, number of arguments does not match",  "", "",
                    at, CompilationError::invalid_cast);
                return nullptr;
            }
            // result
            auto funT = make_smart<TypeDecl>(*seTF);
            auto cresT = cTF->firstType;
            auto resT = funT->firstType;
            if ( !cresT->isSameType(*resT,RefMatters::yes, ConstMatters::no, TemporaryMatters::no) ) {
                if ( resT->isStructure() || (resT->isPointer() && resT->firstType && resT->firstType->isStructure()) ) {
                    auto tryRes = castStruct(at, resT, cresT, upcast);
                    if ( tryRes ) {
                        funT->firstType = tryRes;
                    }
                }
            }
            funT->firstType->constant = cresT->constant;
            // arguments
            for ( size_t i=0; i!=seTF->argTypes.size(); ++i ) {
                auto seT = seTF->argTypes[i];
                auto cT = cTF->argTypes[i];
                if ( !cT->isSameType(*seT,RefMatters::yes, ConstMatters::no, TemporaryMatters::no) ) {
                    if ( seT->isStructure() || (seT->isPointer() && seT->firstType->isStructure()) ) {
                        auto tryArg = castStruct(at, seT, cT, upcast);
                        if ( tryArg ) {
                            funT->argTypes[i] = tryArg;
                        }
                    }
                }
                funT->argTypes[i]->constant = cT->constant;
            }
            if ( castType->isSameType(*funT, RefMatters::yes, ConstMatters::yes, TemporaryMatters::yes) ) {
                return funT;
            } else {
                error("incompatible cast, can't cast " + funT->describe() + " to " + castType->describe(),  "", "",
                    at, CompilationError::invalid_cast);
                return nullptr;
            }
        }

        virtual ExpressionPtr visit ( ExprCast * expr ) override {
            if ( !expr->subexpr->type ) return Visitor::visit(expr);
            if ( expr->castType && expr->castType->isExprType() ) {
                return Visitor::visit(expr);
            }
            if ( expr->castType->isAlias() ) {
                auto aT = inferAlias(expr->castType);
                if ( aT ) {
                    expr->castType = aT;
                    expr->castType->sanitize();
                    reportAstChanged();
                } else {
                    error("undefined type " + expr->castType->describe(),  "", "",
                        expr->at, CompilationError::type_not_found);
                    return Visitor::visit(expr);
                }
            }
            if ( expr->castType->isAuto() ) {
                error("casting to undefined type " + expr->castType->describe(),  "", "",
                    expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            if ( expr->subexpr->type->isSameType(*expr->castType, RefMatters::yes, ConstMatters::yes, TemporaryMatters::yes) ) {
                reportAstChanged();
                return expr->subexpr;
            }
            if ( expr->reinterpret ) {
                expr->type = make_smart<TypeDecl>(*expr->castType);
                expr->type->ref = expr->subexpr->type->ref;
            } else if ( expr->castType->isGoodBlockType() ||  expr->castType->isGoodFunctionType() || expr->castType->isGoodLambdaType() ) {
                expr->type = castFunc(expr->at, expr->subexpr->type, expr->castType, expr->upcast);
            } else {
                expr->type = castStruct(expr->at, expr->subexpr->type, expr->castType, expr->upcast);
            }
            if ( expr->upcast || expr->reinterpret ) {
                if ( !safeExpression(expr) ) {
                    error("cast requires unsafe",  "", "",
                        expr->at, CompilationError::unsafe);
                }
            }
            if ( expr->type ) {
                verifyType(expr->type);
            } else {
                error("can't cast " + expr->subexpr->type->describe() + " to " + expr->castType->describe(), "", "",
                      expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            return Visitor::visit(expr);
        }
    // ExprAscend
        virtual ExpressionPtr visit ( ExprAscend * expr ) override {
            if ( !expr->subexpr->type ) return Visitor::visit(expr);
            if ( expr->subexpr->type->baseType==Type::tHandle && expr->subexpr->rtti_isMakeStruct() ) {
                auto mks = static_pointer_cast<ExprMakeStruct>(expr->subexpr);
                if ( !mks->isNewHandle ) {
                    reportAstChanged();
                    mks->isNewHandle = true;
                }
            }
            if ( !expr->subexpr->type->isRef() ) {
                error("can't ascend (to heap) non-reference value",  "", "",
                    expr->at, CompilationError::invalid_new_type);
            } else if ( expr->subexpr->type->baseType==Type::tHandle ) {
                const auto & subt = expr->subexpr->type;
                if ( !subt->dim.empty() ) {
                    error("can't new [] of handled type, " + subt->describe(),  "", "",
                        expr->at, CompilationError::invalid_new_type);
                }
                if ( !subt->annotation->canNew() ) {
                    error("can't new this handled type at all, " + subt->describe(),  "", "",
                        expr->at, CompilationError::invalid_new_type);
                }
            }
            if ( expr->ascType ) {
                expr->type = make_smart<TypeDecl>(*expr->ascType);
            } else {
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*expr->subexpr->type);
                expr->type->firstType->ref = false;
                if ( expr->type->firstType->baseType==Type::tHandle ) {
                    expr->type->smartPtr = expr->type->firstType->annotation->isSmart();
                }
            }
            return Visitor::visit(expr);
        }
    // ExprNew
        virtual void preVisit ( ExprNew * call ) override {
            Visitor::preVisit(call);
            call->argumentsFailedToInfer = false;
        }
        virtual ExpressionPtr visitNewArg ( ExprNew * call, Expression * arg , bool last ) override {
            if ( !arg->type ) call->argumentsFailedToInfer = true;
            if ( arg->type && arg->type->isAlias() ) call->argumentsFailedToInfer = true;
            return Visitor::visitNewArg(call, arg, last);
        }
        virtual ExpressionPtr visit ( ExprNew * expr ) override {
            if ( !expr->typeexpr ) {
                error("new type did not infer", "", "",
                    expr->at, CompilationError::type_not_found);
                return Visitor::visit(expr);
            }
            if ( expr->typeexpr && expr->typeexpr->isExprType() ) {
                return Visitor::visit(expr);
            }
            // infer
            if ( expr->typeexpr->isAlias() ) {
                if ( auto aT = findAlias(expr->typeexpr->alias) ) {
                    expr->typeexpr = make_smart<TypeDecl>(*aT);
                    expr->typeexpr->ref = false;        // drop a ref
                    expr->typeexpr->constant = false;   // drop a const
                    expr->typeexpr->sanitize();
                    reportAstChanged();
                } else {
                    error("undefined type " + expr->typeexpr->describe(), "", "",
                          expr->at, CompilationError::type_not_found);
                    return Visitor::visit(expr);
                }
            }
            expr->name.clear();
            expr->func = nullptr;
            if ( expr->typeexpr->ref ) {
                error("can't new a reference",  "", "",
                    expr->at, CompilationError::invalid_new_type);
            } else if ( expr->typeexpr->baseType==Type::tStructure ) {
                if ( !expr->initializer && expr->typeexpr->structType->isClass ) {
                    error("new of class requires () syntax; " + expr->typeexpr->describe(), "", "",
                        expr->at, CompilationError::invalid_new_type);
                }
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*expr->typeexpr);
                expr->type->firstType->dim.clear();
                expr->type->dim = expr->typeexpr->dim;
                expr->name = expr->typeexpr->structType->getMangledName();
            } else if ( expr->typeexpr->baseType==Type::tHandle ) {
                if ( expr->typeexpr->annotation->canNew() ) {
                    expr->type = make_smart<TypeDecl>(Type::tPointer);
                    expr->type->firstType = make_smart<TypeDecl>(*expr->typeexpr);
                    expr->type->firstType->dim.clear();
                    expr->type->dim = expr->typeexpr->dim;
                    expr->type->smartPtr = expr->typeexpr->annotation->isSmart();
                } else {
                    error("can't new this type " + expr->typeexpr->describe(), "", "",
                          expr->at, CompilationError::invalid_new_type);
                }
            } else if ( expr->typeexpr->baseType==Type::tTuple ) {
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*expr->typeexpr);
                expr->type->firstType->dim.clear();
                expr->type->dim = expr->typeexpr->dim;
                expr->name = expr->typeexpr->getMangledName();
            } else if ( expr->typeexpr->baseType==Type::tVariant ) {
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*expr->typeexpr);
                expr->type->firstType->dim.clear();
                expr->type->dim = expr->typeexpr->dim;
                expr->name = expr->typeexpr->getMangledName();
            } else {
                error("can only new tuples, variants, structures or handled types, not " + expr->typeexpr->describe(), "", "",
                      expr->at, CompilationError::invalid_new_type);
            }
            if ( expr->initializer && expr->name.empty() ) {
                error("only native structures can have initializers, not " + expr->typeexpr->describe(), "", "",
                      expr->at, CompilationError::invalid_new_type);
            }
            if ( expr->type && expr->initializer && !expr->name.empty() ) {
                auto resultType = expr->type;
                expr->func = inferFunctionCall(expr).get();
                swap ( resultType, expr->type );
                if ( expr->func ) {
                    if ( !expr->type->firstType->isSameType(*resultType, RefMatters::yes, ConstMatters::yes, TemporaryMatters::yes) ) {
                        error("initializer returns " +resultType->describe() + " vs "
                            +  expr->type->firstType->describe(), "", "",
                              expr->at, CompilationError::invalid_new_type);
                    }
                }
                else {
                    error(expr->type->firstType->describe() + " does not have default initializer", "", "",
                        expr->at, CompilationError::invalid_new_type);
                }
            }
            verifyType(expr->typeexpr);
            return Visitor::visit(expr);
        }
    // ExprAt
        virtual ExpressionPtr visit ( ExprAt * expr ) override {
            if ( !expr->subexpr->type || !expr->index->type) return Visitor::visit(expr);
            // infer
            expr->index = Expression::autoDereference(expr->index);
            auto seT = expr->subexpr->type;
            auto ixT = expr->index->type;
            if ( seT->isGoodTableType() ) {
                if ( !seT->firstType->isSameType(*ixT,RefMatters::no, ConstMatters::no, TemporaryMatters::no) ) {
                    error("table index type mismatch, "
                        +seT->firstType->describe() + " vs " + ixT->describe(),  "", "",
                        expr->index->at, CompilationError::invalid_index_type);
                    return Visitor::visit(expr);
                }
                if ( seT->isConst() ) {
                    error("can't index in the constant table, use find instead", "", "",
                        expr->index->at, CompilationError::invalid_table_type);
                    return Visitor::visit(expr);
                }
                expr->type = make_smart<TypeDecl>(*seT->secondType);
                expr->type->ref = true;
                expr->type->constant |= seT->constant;
            } else if ( seT->isHandle() ) {
                if ( !seT->annotation->isIndexable(ixT) ) {
                    error("handle " + seT->annotation->name + " does not support index type " + ixT->describe(),  "", "",
                        expr->index->at, CompilationError::invalid_index_type);
                    return Visitor::visit(expr);
                }
                expr->type = seT->annotation->makeIndexType(expr->subexpr, expr->index);
                expr->type->constant |= seT->constant;
            } else if ( seT->isPointer() ) {
                if ( !safeExpression(expr) ) {
                    error("index of the pointer requires unsafe",  "", "",
                        expr->at, CompilationError::unsafe);
                }
                if ( !ixT->isIndex() ) {
                    error("index must be int or uint, not " + ixT->describe(),  "", "",
                        expr->index->at, CompilationError::invalid_index_type);
                    return Visitor::visit(expr);
                } else if ( !seT->firstType || seT->firstType->getSizeOf()==0 ){
                    error("can't index void pointer",  "", "",
                        expr->index->at, CompilationError::invalid_index_type);
                    return Visitor::visit(expr);
                } else {
                    expr->subexpr = Expression::autoDereference(expr->subexpr);
                    expr->type = make_smart<TypeDecl>(*seT->firstType);
                    expr->type->ref = true;
                    expr->type->constant |= seT->constant;
                }
            } else {
                if ( !ixT->isIndex() ) {
                    expr->type.reset();
                    error("index must be int or uint, not " + ixT->describe(),  "", "",
                        expr->index->at, CompilationError::invalid_index_type);
                    return Visitor::visit(expr);
                } else if ( seT->isVectorType() ) {
                    expr->type = make_smart<TypeDecl>(seT->getVectorBaseType());
                    expr->type->ref = seT->ref;
                    expr->type->constant = seT->constant;
                } else if ( seT->isGoodArrayType() ) {
                    expr->type = make_smart<TypeDecl>(*seT->firstType);
                    expr->type->ref = true;
                    expr->type->constant |= seT->constant;
                } else if ( !seT->dim.size() ) {
                    error("type can't be indexed " + seT->describe(),  "", "",
                        expr->subexpr->at, CompilationError::cant_index);
                    return Visitor::visit(expr);
                } else if ( !seT->isAutoArrayResolved() ) {
                    error("type dimensions are not resolved yet " + seT->describe(),  "", "",
                        expr->subexpr->at, CompilationError::cant_index);
                    return Visitor::visit(expr);
                } else {
                    expr->type = make_smart<TypeDecl>(*seT);
                    expr->type->ref = true;
                    expr->type->dim.erase(expr->type->dim.begin());
                    if ( !expr->type->dimExpr.empty() ) {
                        expr->type->dimExpr.erase(expr->type->dimExpr.begin());
                    }
                    expr->type->constant |= seT->constant;
                }
            }
            propagateTempType(expr->subexpr->type, expr->type);
            return Visitor::visit(expr);
        }
    // ExprSafeAt
        virtual ExpressionPtr visit ( ExprSafeAt * expr ) override {
            if ( !expr->subexpr->type || !expr->index->type) return Visitor::visit(expr);
            // infer
            if ( !expr->subexpr->type->isVectorType() ) {
                expr->subexpr = Expression::autoDereference(expr->subexpr);
            }
            expr->index = Expression::autoDereference(expr->index);
            auto ixT = expr->index->type;
            if ( expr->subexpr->type->isPointer() ) {
                if (!expr->subexpr->type->firstType) {
                    error("can't index void pointer", "", "",
                        expr->index->at, CompilationError::cant_index);
                    return Visitor::visit(expr);
                }
                auto seT = expr->subexpr->type->firstType;
                if (seT->isGoodTableType()) {
                    if ( !safeExpression(expr) ) {
                        error("safe-index of table<> requires unsafe",  "", "",
                            expr->at, CompilationError::unsafe);
                    }
                    if ( !seT->firstType->isSameType(*ixT,RefMatters::no, ConstMatters::no, TemporaryMatters::no) ) {
                        error("table safe-index type mismatch, "
                            + seT->firstType->describe() + " vs " + ixT->describe(),  "", "",
                            expr->index->at, CompilationError::invalid_index_type);
                        return Visitor::visit(expr);
                    }
                    expr->type = make_smart<TypeDecl>(Type::tPointer);
                    expr->type->firstType = make_smart<TypeDecl>(*seT->secondType);
                    expr->type->constant |= seT->constant;
                } else if (seT->isHandle()) {
                    // TODO: support handle safe index
                    // if (!seT->annotation->isIndexable(ixT)) {
                        error("handle " + seT->annotation->name + " does not support safe index type " + ixT->describe(), "", "",
                            expr->index->at, CompilationError::invalid_index_type);
                        return Visitor::visit(expr);
                    // }
                    // expr->type = seT->annotation->makeIndexType(expr->subexpr, expr->index);
                    // expr->type->constant |= seT->constant;
                }
                else if (seT->isPointer()) {
                    error("safe-index of pointer is not supported", "", "",
                        expr->index->at, CompilationError::cant_index);
                    return Visitor::visit(expr);
                } else {
                    if (!ixT->isIndex()) {
                        expr->type.reset();
                        error("index must be int or uint, not " + ixT->describe(), "", "",
                            expr->index->at, CompilationError::invalid_index_type);
                        return Visitor::visit(expr);
                    } else if (seT->isVectorType()) {
                        expr->type = make_smart<TypeDecl>(Type::tPointer);
                        expr->type->firstType = make_smart<TypeDecl>(seT->getVectorBaseType());
                        expr->type->firstType->constant = seT->constant;
                    } else if (seT->isGoodArrayType()) {
                        if ( !safeExpression(expr) ) {
                            error("safe-index of array<> requires unsafe",  "", "",
                                expr->at, CompilationError::unsafe);
                        }
                        expr->type = make_smart<TypeDecl>(Type::tPointer);
                        expr->type->firstType = make_smart<TypeDecl>(*seT->firstType);
                        expr->type->firstType->constant |= seT->constant;
                    } else if ( seT->dim.size() ) {
                        if ( !seT->isAutoArrayResolved() ) {
                            error("type dimensions are not resolved yet " + seT->describe(), "", "",
                                expr->subexpr->at, CompilationError::cant_index);
                            return Visitor::visit(expr);
                        } else {
                            expr->type = make_smart<TypeDecl>(Type::tPointer);
                            expr->type->firstType = make_smart<TypeDecl>(*seT);
                            expr->type->firstType->dim.erase(expr->type->firstType->dim.begin());
                            if ( !expr->type->firstType->dimExpr.empty() ) {
                                expr->type->firstType->dimExpr.erase(expr->type->firstType->dimExpr.begin());
                            }
                            expr->type->firstType->constant |= seT->constant;
                        }
                    } else if ( seT->isVectorType() ) {
                        expr->type = make_smart<TypeDecl>(Type::tPointer);
                        expr->type->firstType = make_smart<TypeDecl>(seT->getVectorBaseType());
                        expr->type->firstType->constant = seT->constant;
                    } else {
                        error("type can't be safe-indexed " + seT->describe(), "", "",
                            expr->subexpr->at, CompilationError::cant_index);
                        return Visitor::visit(expr);
                    }
                }
            } else if ( expr->subexpr->type->isGoodArrayType() ) {
                if ( !safeExpression(expr) ) {
                    error("safe-index of array<> requires unsafe",  "", "",
                        expr->at, CompilationError::unsafe);
                }
                auto seT = expr->subexpr->type;
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*seT->firstType);
                expr->type->firstType->constant |= seT->constant;
            } else if ( expr->subexpr->type->isGoodTableType() ) {
                if ( !safeExpression(expr) ) {
                    error("safe-index of table<> requires unsafe",  "", "",
                        expr->at, CompilationError::unsafe);
                }
                const auto & seT = expr->subexpr->type;
                if ( !seT->firstType->isSameType(*ixT,RefMatters::no, ConstMatters::no, TemporaryMatters::no) ) {
                    error("table safe-index type mismatch, "
                        + seT->firstType->describe() + " vs " + ixT->describe(),  "", "",
                        expr->index->at, CompilationError::invalid_index_type);
                    return Visitor::visit(expr);
                }
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*seT->secondType);
                expr->type->constant |= seT->constant;
            } else if ( expr->subexpr->type->dim.size() ) {
                if ( !safeExpression(expr) ) {
                    error("safe-index of [] requires unsafe",  "", "",
                        expr->at, CompilationError::unsafe);
                }
                const auto & seT = expr->subexpr->type;
                if ( !seT->isAutoArrayResolved() ) {
                    error("type dimensions are not resolved yet " + seT->describe(), "", "",
                        expr->subexpr->at, CompilationError::cant_index);
                }
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(*seT);
                expr->type->firstType->dim.erase(expr->type->firstType->dim.begin());
                if ( !expr->type->firstType->dimExpr.empty() ) {
                    expr->type->firstType->dimExpr.erase(expr->type->firstType->dimExpr.begin());
                }
                expr->type->firstType->constant |= seT->constant;
            } else if ( expr->subexpr->type->isVectorType() && expr->subexpr->type->isRef() ) {
                const auto & seT = expr->subexpr->type;
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = make_smart<TypeDecl>(seT->getVectorBaseType());
                expr->type->firstType->constant = seT->constant;
            } else {
                error("type can't be safe-indexed " + expr->subexpr->type->describe(), "", "",
                    expr->subexpr->at, CompilationError::cant_index);
                return Visitor::visit(expr);
            }
            propagateTempType(expr->subexpr->type, expr->type);
            return Visitor::visit(expr);
        }
    // ExprBlock
        void setBlockCopyMoveFlags ( ExprBlock * block ) {
            if ( block->returnType->isRefType() ) {
                if ( block->returnType->canCopy() ) {
                    block->copyOnReturn = true;
                    block->moveOnReturn = false;
                } else if ( block->returnType->canMove() ) {
                    block->copyOnReturn = false;
                    block->moveOnReturn = true;
                } else {
                    error("this type can't be returned at all " + block->returnType->describe(), "", "",
                          block->at, CompilationError::invalid_return_type);
                }
            } else {
                block->copyOnReturn = false;
                block->moveOnReturn = false;
            }
        }
        virtual void preVisit ( ExprBlock * block ) override {
            Visitor::preVisit(block);
            block->hasReturn = false;
            if ( block->isClosure ) {
                blocks.push_back(block);
                block->type = make_smart<TypeDecl>(*block->returnType);
            }
            scopes.push_back(block);
            pushVarStack();
        }
        virtual void preVisitBlockFinal ( ExprBlock * block ) override {
            Visitor::preVisitBlockFinal(block);
            if ( block->getFinallyEvalFlags() ) {
                error("finally section can't have break, return, or goto",  "", "",
                    block->at, CompilationError::return_or_break_in_finally );
            }
        }
        virtual void preVisitBlockArgument ( ExprBlock * block, const VariablePtr & var, bool lastArg ) override {
            Visitor::preVisitBlockArgument(block, var, lastArg);
            if ( !var->can_shadow && !program->policies.allow_block_variable_shadowing ) {
                if ( func ) {
                    for ( auto & fna : func->arguments ) {
                        if ( fna->name==var->name ) {
                            error("block argument " + var->name +" is shadowed by function argument "
                                + fna->name + " : " + fna->type->describe() + " at line " + to_string(fna->at.line), "", "",
                                    var->at, CompilationError::variable_not_found);
                        }
                    }
                }
                for ( auto & blk : blocks ) {
                    if ( blk == block ) continue;
                    for ( auto & bna : blk->arguments ) {
                        if ( bna->name==var->name ) {
                            error("block argument " + var->name +" is shadowed by another block argument "
                                + bna->name + " : " + bna->type->describe() + " at line " + to_string(bna->at.line), "", "",
                                    var->at, CompilationError::variable_not_found);
                        }
                    }
                }
                for ( auto & lv : local ) {
                    if ( lv->name==var->name ) {
                        error("block argument " + var->name +" is shadowed by local variable "
                            + lv->name + " : " + lv->type->describe() + " at line " + to_string(lv->at.line), "", "",
                                var->at, CompilationError::variable_not_found);
                        break;
                    }
                }
                if ( auto eW = hasMatchingWith(var->name) ) {
                    error("block argument " + var->name + " is shadowed by with expression at line " + to_string(eW->at.line), "", "",
                        var->at, CompilationError::variable_not_found);
                }
            }
            if ( var->type->isAlias() ) {
                auto aT = inferAlias(var->type);
                if ( aT ) {
                    var->type = aT;
                    reportAstChanged();
                } else {
                    error("undefined type " + var->type->describe(),  "", "",
                        var->at, CompilationError::type_not_found);
                }
            }
            if ( var->type->isAuto() && !var->init) {
                error("block argument type can't be infered, it needs an "
                    "initializer or to be passed to the function with the explicit block definition", "", "",
                      var->at, CompilationError::cant_infer_missing_initializer );
            }
            if ( var->type->ref && var->type->isRefType() ) { // silently fix a : Foo& into a : Foo
                var->type->ref = false;
            }
            verifyType(var->type,true);
        }
        virtual ExpressionPtr visitBlockArgumentInit (ExprBlock * block, const VariablePtr & arg, Expression * that ) override {
            if ( arg->type->isAuto() ) {
                auto argT = TypeDecl::inferGenericType(arg->type, arg->init->type);
                if ( !argT ) {
                    error("block argument initialization type can't be infered, "
                          + arg->type->describe() + " = " + arg->init->type->describe(), "", "",
                          arg->at, CompilationError::cant_infer_mismatching_restrictions );
                } else {
                    TypeDecl::applyAutoContracts(argT, arg->type);
                    arg->type = argT;
                    reportAstChanged();
                    return Visitor::visitBlockArgumentInit(block, arg, that);
                }
            }
            if (!arg->type->isAuto()) {
                if (!arg->init->type || !arg->type->isSameType(*arg->init->type, RefMatters::no, ConstMatters::no, TemporaryMatters::no)) {
                    error("block argument default value type mismatch "
                        + arg->type->describe() + " vs " + (arg->init->type ? arg->init->type->describe() : "???"),  "", "",
                        arg->init->at, CompilationError::invalid_argument_type);
                }
                if (arg->init->type && arg->type->ref && !arg->init->type->ref) {
                    error("block argument default value type mismatch " + arg->type->describe()
                        + " vs " + arg->init->type->describe() + ", reference matters",   "", "",
                        arg->init->at, CompilationError::invalid_argument_type);
                }
            }
            return Visitor::visitBlockArgumentInit(block, arg, that);
        }
        virtual ExpressionPtr visit ( ExprBlock * block ) override {
            // to the rest of it
            popVarStack();
            scopes.pop_back();
            if ( block->isClosure ) {
                blocks.pop_back();
                if ( block->list.size() ) {
                    uint32_t flags = block->getEvalFlags();
                    if ( flags & EvalFlags::stopForBreak ) {
                        error("captured block can't 'break' outside of the block",  "", "",
                              block->at, CompilationError::invalid_block);
                    }
                    if ( flags & EvalFlags::stopForContinue ) {
                        error("captured block can't 'continue' outside of the block",  "", "",
                              block->at, CompilationError::invalid_block);
                    }
                }
                if ( !block->hasReturn && block->type->isAuto() ) {
                    block->returnType = make_smart<TypeDecl>(Type::tVoid);
                    block->type = make_smart<TypeDecl>(Type::tVoid);
                    setBlockCopyMoveFlags(block);
                    reportAstChanged();
                }
                if ( block->returnType ) {
                    setBlockCopyMoveFlags(block);
                    verifyType(block->returnType);
                }
            }
            if ( block->needCollapse ) {
                block->needCollapse = false;
                if ( block->collapse() ) reportAstChanged();
            }
            return Visitor::visit(block);
        }
    // Swizzle
        virtual ExpressionPtr visit ( ExprSwizzle * expr ) override {
            if ( !expr->value->type ) return Visitor::visit(expr);
            auto valT = expr->value->type;
            int dim = valT->getVectorDim();
            if ( !TypeDecl::buildSwizzleMask(expr->mask, dim, expr->fields) ) {
                error("invalid swizzle mask",   "", "",
                    expr->at, CompilationError::invalid_swizzle_mask);
            } else {
                auto bt = valT->getVectorBaseType();
                auto rt = TypeDecl::getVectorType(bt, int(expr->fields.size()));
                expr->type = make_smart<TypeDecl>(rt);
                expr->type->constant = valT->constant;
                expr->type->ref = valT->ref;
                if ( expr->type->ref ) {
                    expr->type->ref &= TypeDecl::isSequencialMask(expr->fields);
                }
                if ( !expr->type->ref ) {
                    expr->value = Expression::autoDereference(expr->value);
                }
            }
            return Visitor::visit(expr);
        }
    // ExprAsVariant
        virtual ExpressionPtr visit(ExprAsVariant * expr) override {
            if (!expr->value->type) return Visitor::visit(expr);
            // implement variant macros
            ExpressionPtr substitute;
            auto thisModule = ctx.thisProgram->thisModule.get();
            auto modMacro = [&](Module * mod) -> bool {
                if ( thisModule->isVisibleDirectly(mod) && mod!=thisModule ) {
                    for ( const auto & pm : mod->variantMacros ) {
                        if ( (substitute = pm->visitAs(ctx.thisProgram, thisModule, expr)) ) {
                            return false;
                        }
                    }
                }
                return true;
            };
            Module::foreach(modMacro);
            if ( !substitute ) ctx.thisProgram->library.foreach(modMacro, "*");
            if ( substitute ) {
                reportAstChanged();
                return substitute;
            }
            // regular infer
            auto valT = expr->value->type;
            if ( !valT->isGoodVariantType() ) {
                error(" as " + expr->name + " only allowed for variants", "", "",
                    expr->at, CompilationError::invalid_type);
                return Visitor::visit(expr);

            }
            int index = valT->findArgumentIndex(expr->name);
            if ( index==-1 || index>=int(valT->argTypes.size()) ) {
                error("can't get variant field " + expr->name, "", "",
                    expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            }
            expr->fieldIndex = index;
            expr->type = make_smart<TypeDecl>(*valT->argTypes[expr->fieldIndex]);
            expr->type->ref = true;
            expr->type->constant |= valT->constant;
            propagateTempType(expr->value->type, expr->type);
            return Visitor::visit(expr);
        }
    // ExprSafeAsVariant
        virtual ExpressionPtr visit(ExprSafeAsVariant * expr) override {
            if (!expr->value->type) return Visitor::visit(expr);
            // implement variant macros
            ExpressionPtr substitute;
            auto thisModule = ctx.thisProgram->thisModule.get();
            auto modMacro = [&](Module * mod) -> bool {
                if ( thisModule->isVisibleDirectly(mod) && mod!=thisModule ) {
                    for ( const auto & pm : mod->variantMacros ) {
                        if ( (substitute = pm->visitSafeAs(ctx.thisProgram, thisModule, expr)) ) {
                            return false;
                        }
                    }
                }
                return true;
            };
            Module::foreach(modMacro);
            if ( !substitute ) ctx.thisProgram->library.foreach(modMacro, "*");
            if ( substitute ) {
                reportAstChanged();
                return substitute;
            }
            // regular infer
            if ( !expr->value->type->isPointer() && !safeExpression(expr) ) {
                error("variant ?as on non-pointer requires unsafe", "", "",
                    expr->at,CompilationError::unsafe);
            }
            auto valT = expr->value->type->isPointer() ? expr->value->type->firstType : expr->value->type;
            if ( !valT || !valT->isGoodVariantType() ) {
                error(" ?as " + expr->name + " only allowed for variants or pointers to variants", "", "",
                    expr->at, CompilationError::invalid_type);
                return Visitor::visit(expr);

            }
            int index = valT->findArgumentIndex(expr->name);
            if ( index==-1 || index>=int(valT->argTypes.size()) ) {
                error("can't get variant field " + expr->name, "", "",
                    expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            }
            expr->fieldIndex = index;
            expr->type = make_smart<TypeDecl>(*valT->argTypes[expr->fieldIndex]);
            expr->skipQQ = expr->type->isPointer();
            if ( !expr->skipQQ ) {
                auto fieldType = expr->type;
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = fieldType;
            }
            expr->type->constant |= valT->constant | expr->value->type->constant;
            propagateTempType(expr->value->type, expr->type);
            return Visitor::visit(expr);
        }
    // ExprIsVariant
        virtual ExpressionPtr visit(ExprIsVariant * expr) override {
            if (!expr->value->type) return Visitor::visit(expr);
            // implement variant macros
            ExpressionPtr substitute;
            auto thisModule = ctx.thisProgram->thisModule.get();
            auto modMacro = [&](Module * mod) -> bool {
                if ( thisModule->isVisibleDirectly(mod) && mod!=thisModule ) {
                    for ( const auto & pm : mod->variantMacros ) {
                        if ( (substitute = pm->visitIs(ctx.thisProgram, thisModule, expr)) ) {
                            return false;
                        }
                    }
                }
                return true;
            };
            Module::foreach(modMacro);
            if ( !substitute ) ctx.thisProgram->library.foreach(modMacro, "*");
            if ( substitute ) {
                reportAstChanged();
                return substitute;
            }
            // regular infer
            auto valT = expr->value->type;
            if ( !valT->isGoodVariantType() ) {
                error(" is " + expr->name + " only allowed for variants", "", "",
                    expr->at, CompilationError::invalid_type);
                return Visitor::visit(expr);

            }
            int index = valT->findArgumentIndex(expr->name);
            if ( index==-1 || index>=int(valT->argTypes.size()) ) {
                error("can't get variant field " + expr->name, "", "",
                    expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            }
            expr->fieldIndex = index;
            expr->type = make_smart<TypeDecl>(Type::tBool);
            return Visitor::visit(expr);
        }
    // ExprField
        virtual ExpressionPtr visit ( ExprField * expr ) override {
            if ( !expr->value->type ) return Visitor::visit(expr);
            if ( expr->name.empty() ) {
                error("syntax error, expecting field after .", "", "",
                        expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            }
            // infer
            auto valT = expr->value->type;
            if ( valT->isVectorType() ) {
                reportAstChanged();
                return make_smart<ExprSwizzle>(expr->at,expr->value,expr->name);
            } else if ( valT->isBitfield() ) {
                expr->value = Expression::autoDereference(expr->value);
                valT = expr->value->type;
                int index = expr->bitFieldIndex();
                if ( index==-1 || index>=int(valT->argNames.size()) ) {
                    error("can't get bit field " + expr->name + " in " + valT->describe(), "", "",
                        expr->at, CompilationError::cant_get_field);
                    return Visitor::visit(expr);
                }
                expr->fieldIndex = index;
            } else if ( valT->isHandle() ) {
                expr->annotation = valT->annotation;
                expr->type = expr->annotation->makeFieldType(expr->name);
            } else if ( valT->isStructure() ) {
                expr->field = valT->structType->findField(expr->name);
            } else if ( valT->isPointer() ) {
                expr->value = Expression::autoDereference(expr->value);
                expr->unsafeDeref = func ? func->unsafeDeref : false;
                if ( valT->firstType->isStructure() ) {
                    expr->field = valT->firstType->structType->findField(expr->name);
                } else if ( valT->firstType->isHandle() ) {
                    expr->annotation = valT->firstType->annotation;
                    expr->type = expr->annotation->makeFieldType(expr->name);
                } else if ( valT->firstType->isGoodTupleType() ) {
                    int index = expr->tupleFieldIndex();
                    if ( index==-1 || index>=int(valT->firstType->argTypes.size()) ) {
                        error("can't get tuple field " + expr->name, "", "",
                            expr->at, CompilationError::cant_get_field);
                        return Visitor::visit(expr);
                    }
                    expr->fieldIndex = index;
                } else if ( valT->firstType->isGoodVariantType() ) {
                    if ( !safeExpression(expr) ) {
                        error("variant.field requires unsafe", "", "",
                            expr->at, CompilationError::unsafe);
                        return Visitor::visit(expr);
                    }
                    int index = expr->variantFieldIndex();
                    if ( index==-1 || index>=int(valT->firstType->argTypes.size()) ) {
                        error("can't get variant field " + expr->name, "", "",
                            expr->at, CompilationError::cant_get_field);
                        return Visitor::visit(expr);
                    }
                    expr->fieldIndex = index;
                }
            } else if ( valT->isGoodTupleType() ) {
                int index = expr->tupleFieldIndex();
                if ( index==-1 || index>=int(valT->argTypes.size()) ) {
                    error("can't get tuple field " + expr->name, "", "",
                        expr->at, CompilationError::cant_get_field);
                    return Visitor::visit(expr);
                }
                expr->fieldIndex = index;
            } else if ( valT->isGoodVariantType() ) {
                if ( !safeExpression(expr) ) {
                    error("variant.field requires unsafe", "", "",
                        expr->at, CompilationError::unsafe);
                    return Visitor::visit(expr);
                }
                int index = expr->variantFieldIndex();
                if ( index==-1 || index>=int(valT->argTypes.size()) ) {
                    error("can't get variant field " + expr->name, "", "",
                        expr->at, CompilationError::cant_get_field);
                    return Visitor::visit(expr);
                }
                expr->fieldIndex = index;
            } else {
                error("can't get field " + expr->name + " of " + expr->value->type->describe(), "", "",
                      expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            }
            // handle
            if ( expr->field ) {
                expr->type = make_smart<TypeDecl>(*expr->field->type);
                expr->type->ref = true;
                expr->type->constant |= valT->constant;
                if ( valT->isPointer() && valT->firstType ) {
                    expr->type->constant |= valT->firstType->constant;
                }
                if ( !expr->ignoreCaptureConst ) {
                    expr->type->constant |= expr->field->capturedConstant;
                }
            } else if ( expr->fieldIndex!=-1 ) {
                if ( valT->isBitfield() ) {
                    expr->type = make_smart<TypeDecl>(Type::tBool);
                } else {
                    auto tupleT = valT->isPointer() ? valT->firstType : valT;
                    expr->type = make_smart<TypeDecl>(*tupleT->argTypes[expr->fieldIndex]);
                    expr->type->ref = true;
                    expr->type->constant |= tupleT->constant;
                }
            } else if ( !expr->type ) {
                error("field " + expr->name + " not found in " + expr->value->type->describe(), "", "",
                      expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            } else {
                expr->type->constant |= valT->constant;
            }
            propagateTempType(expr->value->type, expr->type);
            return Visitor::visit(expr);
        }
    // ExprSafeField
        virtual ExpressionPtr visit ( ExprSafeField * expr ) override {
            if ( !expr->value->type ) return Visitor::visit(expr);
            // infer
            auto valT = expr->value->type;
            if ( !valT->isPointer() || !valT->firstType ) {
                error("can only safe dereference a pointer to a tupe, a structure or a handle " + valT->describe(), "", "",
                      expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            }
            expr->value = Expression::autoDereference(expr->value);
            if ( valT->firstType->structType ) {
                expr->field = valT->firstType->structType->findField(expr->name);
                if ( !expr->field ) {
                    error("can't safe get field " + expr->name, "", "",
                        expr->at, CompilationError::cant_get_field);
                    return Visitor::visit(expr);
                }
                expr->type = make_smart<TypeDecl>(*expr->field->type);
            } else if ( valT->firstType->isHandle() ) {
                expr->annotation = valT->firstType->annotation;
                expr->type = expr->annotation->makeSafeFieldType(expr->name);
                if ( !expr->type ) {
                    error("can't safe get field " + expr->name, "", "",
                        expr->at, CompilationError::cant_get_field);
                    return Visitor::visit(expr);
                }
            } else if ( valT->firstType->isGoodTupleType() ) {
                int index = expr->tupleFieldIndex();
                if ( index==-1 || index>=int(valT->firstType->argTypes.size()) ) {
                    error("can't get tuple field " + expr->name, "", "",
                        expr->at, CompilationError::cant_get_field);
                    return Visitor::visit(expr);
                }
                expr->fieldIndex = index;
                expr->type = make_smart<TypeDecl>(*valT->firstType->argTypes[expr->fieldIndex]);
            } else if ( valT->firstType->isGoodVariantType() ) {
                if ( !safeExpression(expr) ) {
                    error("variant?.field requires unsafe", "", "",
                        expr->at, CompilationError::unsafe);
                    return Visitor::visit(expr);
                }
                int index = expr->variantFieldIndex();
                if ( index==-1 || index>=int(valT->firstType->argTypes.size()) ) {
                    error("can't get variant field " + expr->name, "", "",
                        expr->at, CompilationError::cant_get_field);
                    return Visitor::visit(expr);
                }
                expr->fieldIndex = index;
                expr->type = make_smart<TypeDecl>(*valT->firstType->argTypes[expr->fieldIndex]);
            } else {
                error("can only safe dereference a pointer to a tuple, a structure or a handle " + valT->describe(), "", "",
                      expr->at, CompilationError::cant_get_field);
                return Visitor::visit(expr);
            }
            expr->skipQQ = expr->type->isPointer();
            if ( !expr->skipQQ ) {
                auto fieldType = expr->type;
                expr->type = make_smart<TypeDecl>(Type::tPointer);
                expr->type->firstType = fieldType;
            }
            expr->type->constant |= valT->constant;
            propagateTempType(expr->value->type, expr->type);
            return Visitor::visit(expr);
        }
    // ExprVar
        vector<VariablePtr> findMatchingVar ( const string & name ) const {
            string moduleName, varName;
            splitTypeName(name, moduleName, varName);
            vector<VariablePtr> result;
            auto inWhichModule = getSearchModule(moduleName);
            program->library.foreach([&](Module * mod) -> bool {
                if ( auto var = mod->findVariable(varName) ) {
                    if ( inWhichModule->isVisibleDirectly(var->module) ) {
                        result.push_back(var);
                    }
                }
                return true;
            },moduleName);
            return result;
        }
        virtual void preVisit ( ExprVar * expr ) override {
            Visitor::preVisit(expr);
            expr->variable = nullptr;
            expr->local = false;
            expr->block = false;
            expr->pBlock = nullptr;
            expr->argument = false;
            expr->argumentIndex = -1;
        }
        virtual ExpressionPtr visit ( ExprVar * expr ) override {
            // local (that on the stack)
            for ( auto it = local.rbegin(); it!=local.rend(); ++it ) {
                auto var = *it;
                if ( var->name==expr->name ) {
                    expr->variable = var;
                    expr->local = true;
                    expr->type = make_smart<TypeDecl>(*var->type);
                    expr->type->ref = true;
                    return Visitor::visit(expr);
                }
            }
            // with
            if ( auto eW = hasMatchingWith(expr->name) ) {
                reportAstChanged();
                return make_smart<ExprField>(expr->at, forceAt(eW->with->clone(),expr->at), expr->name);
            }
            // block arguments
            for ( auto it = blocks.rbegin(); it!=blocks.rend(); ++it ) {
                ExprBlock * block = *it;
                int argumentIndex = 0;
                for ( auto & arg : block->arguments ) {
                    if ( arg->name==expr->name ) {
                        expr->variable = arg;
                        expr->argumentIndex = argumentIndex;
                        expr->block = true;
                        if ( blocks.rbegin() == it ) {
                            expr->thisBlock = true;
                        }
                        expr->type = make_smart<TypeDecl>(*arg->type);
                        if (!expr->type->isRefType())
                            expr->type->ref = true;
                        expr->type->sanitize();
                        expr->pBlock = static_cast<ExprBlock*>(block);
                        return Visitor::visit(expr);
                    }
                    argumentIndex ++;
                }
            }
            // function argument
            if ( func ) {
                int argumentIndex = 0;
                for ( auto & arg : func->arguments ) {
                    if ( arg->name==expr->name ) {
                        expr->variable = arg;
                        expr->argumentIndex = argumentIndex;
                        expr->argument = true;
                        expr->type = make_smart<TypeDecl>(*arg->type);
                        if (!expr->type->isRefType())
                            expr->type->ref = true;
                        expr->type->sanitize();
                        return Visitor::visit(expr);
                    }
                    argumentIndex ++;
                }
            }
            // global
            auto vars = findMatchingVar(expr->name);
            if ( vars.size()==1 ) {
                auto var = vars.back();
                expr->variable = var;
                expr->type = make_smart<TypeDecl>(*var->type);
                expr->type->ref = true;
                return Visitor::visit(expr);

            } else if ( vars.size()==0 ) {
                error("can't locate variable " + expr->name, "", "",
                    expr->at, CompilationError::variable_not_found);
            } else {
                TextWriter errs;
                for ( auto & var : vars ) {
                    errs << "\t" << var->module->name << "::" << var->name << " : " << var->type->describe() << "\n";
                }
                error("too many matching variables " + expr->name, "candidates are:\n" + errs.str(), "",
                    expr->at, CompilationError::variable_not_found);
            }
            return Visitor::visit(expr);
        }
    // ExprOp1
        virtual ExpressionPtr visit ( ExprOp1 * expr ) override {
            if ( !expr->subexpr->type ) return Visitor::visit(expr);
            // pointer arithmetics
            if ( expr->subexpr->type->isPointer() ) {
                if ( !expr->subexpr->type->firstType ) {
                    error("operations on void pointers are prohibited; " +
                        expr->subexpr->type->describe(), "", "",
                        expr->at, CompilationError::invalid_type);
                } else {
                    string pop;
                            if ( expr->op=="++" || expr->op=="+++" )   { pop = "i_das_ptr_inc"; }
                    else    if ( expr->op=="--" || expr->op=="---" )   { pop = "i_das_ptr_dec"; }
                    if ( !pop.empty() ) {
                        reportAstChanged();
                        auto popc = make_smart<ExprCall>(expr->at, pop);
                        auto stride = expr->subexpr->type->firstType->getSizeOf();
                        popc->arguments.push_back(expr->subexpr->clone());
                        popc->arguments.push_back(make_smart<ExprConstInt>(expr->at,stride));
                        return popc;
                    } else {
                        error("pointer arithmetics only allows +, -, +=, -=, ++, --; not" + expr->op, "", "",
                            expr->at, CompilationError::operator_not_found);
                    }
                }
            }
            // infer
            vector<TypeDeclPtr> types = { expr->subexpr->type };
            auto functions = findMatchingFunctions(expr->op, types);
            if ( functions.size()==0 ) {
                reportMissing(expr, types, "no matching operator ", true, CompilationError::operator_not_found);
            } else if ( functions.size()>1 ) {
                string candidates = program->describeCandidates(functions);
                error("too many matching operators '" + expr->op
                      + "' with argument " + expr->subexpr->type->describe(), "", "",
                    expr->at, CompilationError::operator_not_found);
            } else {
                expr->func = functions[0].get();
                if ( expr->func->firstArgReturnType ) {
                    expr->type = make_smart<TypeDecl>(*expr->arguments[0]->type);
                    expr->type->ref = false;
                } else {
                    expr->type = make_smart<TypeDecl>(*expr->func->result);
                }
                if ( !expr->func->arguments[0]->type->isRef() )
                    expr->subexpr = Expression::autoDereference(expr->subexpr);
                // lets try to fold it
                if ( expr->func && expr->func->unsafeOperation && safeExpression(expr) ) {
                    error("unsafe operator " + expr->name + " requires unsafe", "", "",
                        expr->at, CompilationError::unsafe);
                } else if ( enableInferTimeFolding && isConstExprFunc(expr->func) ) {
                    if ( auto se = getConstExpr(expr->subexpr.get()) ) {
                        expr->subexpr = se;
                        return evalAndFold(expr);
                    }
                }
            }
            return Visitor::visit(expr);
        }
    // ExprOp2
        bool isAssignmentOperator ( const string & op ) {
            return (op=="+=") || (op=="-=") || (op=="*=") || (op=="/=")
                || (op=="%=") || (op=="&=") || (op=="|=") || (op=="^=")
                || (op=="<<=") || (op==">>=") || (op=="<<<=") || (op==">>>=");
        }

        bool isSameSmartPtrType ( const TypeDeclPtr & lt, const TypeDeclPtr & rt, bool leftOnly = false ) {
            auto lt_smart = lt->smartPtr;
            auto rt_smart = rt->smartPtr;
            if ( leftOnly ) {
                // smart smart  - ok
                // smart ptr    - ok
                // ptr   smart  - not OK
                // ptr   ptr    - ok
                if ( !lt_smart && rt_smart ) return false;
            }
            lt->smartPtr = false;
            rt->smartPtr = false;
            bool res =  canCopyOrMoveType(lt,rt,TemporaryMatters::no);
            lt->smartPtr = lt_smart;
            rt->smartPtr = rt_smart;
            return res;
        }

        virtual ExpressionPtr visit ( ExprOp2 * expr ) override {
            if ( !expr->left->type || !expr->right->type ) return Visitor::visit(expr);
            // pointer arithmetics
            if ( expr->left->type->isPointer() && expr->right->type->isSimpleType(Type::tInt) ) {
                if ( !expr->left->type->firstType ) {
                    error("operations on void pointers are prohibited; " +
                        expr->left->type->describe(), "", "",
                        expr->at, CompilationError::invalid_type);
                } else {
                    string pop;
                            if ( expr->op=="+" )    { pop = "i_das_ptr_add"; }
                    else    if ( expr->op=="-" )    { pop = "i_das_ptr_sub"; }
                    else    if ( expr->op=="+=" )   { pop = "i_das_ptr_set_add"; }
                    else    if ( expr->op=="-=" )   { pop = "i_das_ptr_set_sub"; }
                    if ( !pop.empty() ) {
                        reportAstChanged();
                        auto popc = make_smart<ExprCall>(expr->at, pop);
                        auto stride = expr->left->type->firstType->getSizeOf();
                        popc->arguments.push_back(expr->left->clone());
                        popc->arguments.push_back(expr->right->clone());
                        popc->arguments.push_back(make_smart<ExprConstInt>(expr->at,stride));
                        return popc;
                    } else {
                        error("pointer arithmetics only allows +, -, +=, -=, ++, --; not" + expr->op, "", "",
                            expr->at, CompilationError::operator_not_found);
                    }
                }
            }
            // infer
            if ( expr->left->type->isPointer() && expr->right->type->isPointer() ) {
                if ( !isSameSmartPtrType(expr->left->type,expr->right->type) ) {
                    error("operations on incompatible pointers are prohibited; " +
                        expr->left->type->describe() + " vs " + expr->right->type->describe(), "", "",
                        expr->at, CompilationError::invalid_type);
                }
                if ( expr->op=="-" ) {
                    if ( !expr->left->type->firstType ) {
                        error("operations on void pointers are prohibited; " +
                            expr->left->type->describe(), "", "",
                            expr->at, CompilationError::invalid_type);
                    } else {
                        reportAstChanged();
                        auto popc = make_smart<ExprCall>(expr->at, "i_das_ptr_diff");
                        auto stride = expr->left->type->firstType->getSizeOf();
                        popc->arguments.push_back(expr->left->clone());
                        popc->arguments.push_back(expr->right->clone());
                        popc->arguments.push_back(make_smart<ExprConstInt>(expr->at,stride));
                        return popc;
                    }
                }
            }
            if ( expr->left->type->isEnum() && expr->right->type->isEnum() )
                if ( !expr->left->type->isSameType(*expr->right->type,RefMatters::no, ConstMatters::no, TemporaryMatters::no) )
                    error("operations on different enumerations are prohibited", "", "",
                        expr->at, CompilationError::invalid_type);
            vector<TypeDeclPtr> types = { expr->left->type, expr->right->type };
            auto functions = findMatchingFunctions("_::" + expr->op, types);    // NOTE: operators always in the context of the callee
            if (functions.size() != 1) {
                if (expr->left->type->isNumeric() && expr->right->type->isNumeric()) {
                    if ( isAssignmentOperator(expr->op) ) {
                        if ( !expr->left->type->ref ) {
                            error("numeric operator " + expr->op + " left side must be reference.", "", "",
                                expr->at, CompilationError::operator_not_found);
                        } else if ( expr->left->type->isConst() ) {
                            error("numeric operator " + expr->op + " left side can't be constant.", "", "",
                                expr->at, CompilationError::operator_not_found);
                        } else  {
                            TextWriter tw;
                            tw << "\t" << *expr->left << " " << expr->op << " " << das_to_string(expr->left->type->baseType) << " (" << *expr->right << ")\n";
                            error("numeric operator " + expr->op + " type mismatch. both sides have to be of the same type. " +
                                das_to_string(expr->left->type->baseType) + " " + expr->op + " " + das_to_string(expr->right->type->baseType)
                                + " is not defined", "", "try the following\n" + tw.str(),
                                expr->at, CompilationError::operator_not_found);
                        }
                    } else {
                        TextWriter tw;
                        tw << "\t" << *expr->left << " " << expr->op << " " << das_to_string(expr->left->type->baseType) << " (" << *expr->right << ")\n";
                        tw << "\t" << das_to_string(expr->right->type->baseType) << "(" << *expr->left << ") " << expr->op << " " << *expr->right << "\n";
                        error("numeric operator " + expr->op + " type mismatch. both sides have to be of the same type. " +
                            das_to_string(expr->left->type->baseType) + " " + expr->op + " " + das_to_string(expr->right->type->baseType)
                            + " is not defined", "", "try one of the following\n" + tw.str(),
                            expr->at, CompilationError::operator_not_found);
                    }
                } else if (functions.size() == 0) {
                    reportMissing(expr, types, "no matching operator ", true, CompilationError::operator_not_found);
                } else if (functions.size() > 1) {
                    string candidates = program->describeCandidates(functions);
                    error("too many matching operators '" + expr->op
                        + "' with arguments (" + expr->left->type->describe() + ", " + expr->right->type->describe()
                        + ")", candidates, "", expr->at, CompilationError::operator_not_found);
                }
            }
            else {
                expr->func = functions[0].get();
                if ( expr->func->firstArgReturnType ) {
                    expr->type = make_smart<TypeDecl>(*expr->arguments[0]->type);
                    expr->type->ref = false;
                } else {
                    expr->type = make_smart<TypeDecl>(*expr->func->result);
                }
                if ( !expr->func->arguments[0]->type->isRef() )
                    expr->left = Expression::autoDereference(expr->left);
                if ( !expr->func->arguments[1]->type->isRef() )
                    expr->right = Expression::autoDereference(expr->right);
                // lets try to fold it
                if ( expr->func && expr->func->unsafeOperation && !safeExpression(expr) ) {
                    error("unsafe operator " + expr->name + " requires unsafe", "", "",
                        expr->at, CompilationError::unsafe);
                } else if ( enableInferTimeFolding && isConstExprFunc(expr->func) ) {
                    auto lcc = getConstExpr(expr->left.get());
                    auto rcc = getConstExpr(expr->right.get());
                    if ( lcc && rcc ) {
                        expr->left = lcc;
                        expr->right = rcc;
                        return evalAndFold(expr);
                    }
                }
            }
            return Visitor::visit(expr);
        }
    // ExprOp3
        virtual ExpressionPtr visit ( ExprOp3 * expr ) override {
            if ( !expr->subexpr->type || !expr->left->type || !expr->right->type ) return Visitor::visit(expr);
            // infer
            if ( expr->op!="?" ) {
                error("Op3 currently only supports 'is'", "", "",
                    expr->at, CompilationError::operator_not_found);
                return Visitor::visit(expr);
            }
            expr->subexpr = Expression::autoDereference(expr->subexpr);
            if ( !expr->subexpr->type->isSimpleType(Type::tBool) ) {
                error("cond operator condition must be boolean", "", "",
                    expr->at, CompilationError::condition_must_be_bool);
            } else if ( !expr->left->type->isSameType(*expr->right->type,RefMatters::no, ConstMatters::no, TemporaryMatters::no) ) {
                error("cond operator must return the same types on both sides","", "",
                      expr->at, CompilationError::operator_not_found);
            } else {
                if ( expr->left->type->ref ^ expr->right->type->ref ) { // if either one is not ref
                    expr->left = Expression::autoDereference(expr->left);
                    expr->right = Expression::autoDereference(expr->right);
                }
                expr->type = make_smart<TypeDecl>(*expr->left->type);
                expr->type->constant |= expr->right->type->constant;
                // lets try to fold it
                if ( enableInferTimeFolding ) {
                    auto ccc = getConstExpr(expr->subexpr.get());
                    auto lcc = getConstExpr(expr->left.get());
                    auto rcc = getConstExpr(expr->right.get());
                    if ( ccc && lcc && rcc ) {
                        expr->subexpr = ccc;
                        expr->left = lcc;
                        expr->right = rcc;
                        return evalAndFold(expr);
                    }
                }
            }
            return Visitor::visit(expr);
        }
    // ExprMove
        bool canCopyOrMoveType ( const TypeDeclPtr & leftType, const TypeDeclPtr & rightType, TemporaryMatters tmatter ) const {
            if ( leftType->baseType==Type::tPointer ) {
                return leftType->isSameType(*rightType, RefMatters::no, ConstMatters::no, tmatter, AllowSubstitute::yes);
            } else {
                return leftType->isSameType(*rightType, RefMatters::no, ConstMatters::no, tmatter, AllowSubstitute::no);
            }
        }
        string moveErrorInfo(ExprMove * expr) const {
            return ", " + expr->left->type->describe() + " <- " + expr->right->type->describe();
        }
        virtual ExpressionPtr visit ( ExprMove * expr ) override {
            if ( !expr->left->type || !expr->right->type ) return Visitor::visit(expr);
            // infer
            if ( !canCopyOrMoveType(expr->left->type,expr->right->type,TemporaryMatters::no) ) {
                error("can only move compatible type"+moveErrorInfo(expr), "", "",
                    expr->at, CompilationError::operator_not_found);
            } else if ( !expr->left->type->isRef() ) {
                error("can only move to a reference"+moveErrorInfo(expr), "", "",
                    expr->at, CompilationError::cant_write_to_non_reference);
            } else if ( expr->left->type->constant ) {
                error("can't move to a constant value"+moveErrorInfo(expr), "", "",
                    expr->at, CompilationError::cant_move_to_const);
            } else if ( !expr->left->type->canMove() ) {
                error("this type can't be moved, use clone (:=) instead"+moveErrorInfo(expr), "", "",
                    expr->at, CompilationError::cant_move);
            } else if ( expr->right->type->constant ) {
                error("can't move from a constant value"+moveErrorInfo(expr), "", "",
                    expr->at, CompilationError::cant_move);
            } else if ( expr->right->type->isTemp(true,false) ) {
                error("can't move temporary value"+moveErrorInfo(expr), "", "",
                    expr->at, CompilationError::cant_pass_temporary);
            } else if ( expr->right->type->isPointer() && expr->right->type->smartPtr ) {
                if ( !expr->right->type->ref && !safeExpression(expr) && !expr->right->rtti_isAscend() ) {
                    error("moving from the smart pointer value requires unsafe",  "",
                        "try moving from reference instead",
                        expr->at, CompilationError::unsafe);
                    return Visitor::visit(expr);
                }
            } else if ( expr->left->type->hasClasses() && !safeExpression(expr) ) {
                error("moving classes requires unsafe"+moveErrorInfo(expr), "", "",
                    expr->at, CompilationError::unsafe);
            }
            expr->type = make_smart<TypeDecl>();  // we return nothing
            return Visitor::visit(expr);
        }
    // ExprCopy
        string copyErrorInfo(ExprCopy * expr) const {
            return ", " + expr->left->type->describe() + " = " + expr->right->type->describe();
        }
        virtual ExpressionPtr visit ( ExprCopy * expr ) override {
            if ( !expr->left->type || !expr->right->type ) return Visitor::visit(expr);
            // infer
            if ( !canCopyOrMoveType(expr->left->type,expr->right->type,TemporaryMatters::no) ) {
                error("can only copy compatible type"+copyErrorInfo(expr), "", "",
                    expr->at, CompilationError::operator_not_found);
            } else if ( !expr->left->type->isRef() ) {
                error("can only copy to a reference"+copyErrorInfo(expr), "", "",
                    expr->at, CompilationError::cant_write_to_non_reference);
            } else if ( expr->left->type->constant ) {
                error("can't write to a constant value"+copyErrorInfo(expr), "", "",
                    expr->at, CompilationError::cant_write_to_const);
            } else if ( expr->right->type->isTemp(true,false) ) {
                    error("can't copy temporary value"+copyErrorInfo(expr), "", "",
                        expr->at, CompilationError::cant_pass_temporary);
            } else if ( expr->left->type->hasClasses() && !safeExpression(expr) ) {
                error("copying classes requires unsafe"+copyErrorInfo(expr), "", "",
                    expr->at, CompilationError::unsafe);
            }
            if ( !expr->left->type->canCopy() ) {
                error("this type can't be copied"+copyErrorInfo(expr),
                    "", "use move (<-) or clone (:=) instead", expr->at, CompilationError::cant_copy);
            }
            expr->type = make_smart<TypeDecl>();  // we return nothing
            return Visitor::visit(expr);
        }
    // ExprClone
        virtual ExpressionPtr visit ( ExprClone * expr ) override {
            if ( !expr->left->type || !expr->right->type ) return Visitor::visit(expr);
            // lets see if there is clone operator already (a user operator can ignore all the rules bellow)
            auto fnList = getCloneFunc(expr->left->type, expr->right->type);
            if ( fnList.size() ) {
                if ( verifyCloneFunc(fnList, expr->at) ) {
                    auto fn = fnList[0];
                    string cloneName = "_::clone";
                    auto cloneFn = make_smart<ExprCall>(expr->at, cloneName);
                    cloneFn->arguments.push_back(expr->left->clone());
                    cloneFn->arguments.push_back(expr->right->clone());
                    return ExpressionPtr(cloneFn);
                } else {
                    return Visitor::visit(expr);
                }
            }
            // infer
            if ( !isSameSmartPtrType(expr->left->type,expr->right->type,true) ) {
                error("can only clone the same type " + expr->left->type->describe() + " vs " + expr->right->type->describe(), "", "",
                      expr->at, CompilationError::operator_not_found);
            } else if ( !expr->left->type->isRef() ) {
                error("can only clone to a reference", "", "",
                    expr->at, CompilationError::cant_write_to_non_reference);
            } else if ( expr->left->type->constant ) {
                error("can't write to a constant value", "", "",
                    expr->at, CompilationError::cant_write_to_const);
            } else if ( !expr->left->type->canClone() ) {
                error("type " + expr->left->type->describe() + " can't be cloned from " + expr->right->type->describe(), "", "",
                    expr->at, CompilationError::cant_copy);
            } else {
                auto cloneType = expr->left->type;
                if ( cloneType->isHandle() ) {
                    expr->type = make_smart<TypeDecl>();  // we return nothing
                    return Visitor::visit(expr);
                } else if ( cloneType->isString() && expr->right->type->isTemp() ) {
                    reportAstChanged();
                    auto cloneFn = make_smart<ExprCall>(expr->at, "clone_string");
                    cloneFn->arguments.push_back(expr->right->clone());
                    return make_smart<ExprCopy>(expr->at, expr->left->clone(), cloneFn);
                } else if ( cloneType->isPointer() && cloneType->smartPtr ) {
                    auto fnClone = makeCloneSmartPtr(expr->at, cloneType, expr->right->type);
                    if ( program->addFunction(fnClone) ) {
                        reportAstChanged();
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::clone");
                        cloneFn->arguments.push_back(expr->left->clone());
                        cloneFn->arguments.push_back(expr->right->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        reportMissingFinalizer("smart pointer clone mismatch ", expr->at, cloneType);
                        return Visitor::visit(expr);
                    }
                } else if ( cloneType->canCopy() ) {
                    if ( expr->right->type->isTemp(true,false) ) {
                        error("can't clone (copy) temporary value", "", "",
                            expr->at, CompilationError::cant_pass_temporary);
                    } else {
                        reportAstChanged();
                        return make_smart<ExprCopy>(expr->at, expr->left->clone(), expr->right->clone());
                    }
                } else if ( cloneType->isGoodArrayType() || cloneType->isGoodTableType() ) {
                    reportAstChanged();
                    auto cloneFn = make_smart<ExprCall>(expr->at, "_::clone");
                    cloneFn->arguments.push_back(expr->left->clone());
                    cloneFn->arguments.push_back(expr->right->clone());
                    return ExpressionPtr(cloneFn);
                } else if ( cloneType->isStructure() ) {
                    reportAstChanged();
                    auto stt = cloneType->structType;
                    fnList = getCloneFunc(cloneType,cloneType);
                    if ( verifyCloneFunc(fnList, expr->at) ) {
                        if ( fnList.size()==0 ) {
                            auto clf = makeClone(stt);
                            clf->privateFunction = true;
                            extraFunctions.push_back(clf);
                        }
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::clone");
                        cloneFn->arguments.push_back(expr->left->clone());
                        cloneFn->arguments.push_back(expr->right->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        return Visitor::visit(expr);
                    }
                } else if ( cloneType->isTuple() ) {
                    reportAstChanged();
                    fnList = getCloneFunc(cloneType,cloneType);
                    if ( verifyCloneFunc(fnList, expr->at) ) {
                        if ( fnList.size()==0 ) {
                            auto clf = makeCloneTuple(expr->at, cloneType);
                            clf->privateFunction = true;
                            extraFunctions.push_back(clf);
                        }
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::clone");
                        cloneFn->arguments.push_back(expr->left->clone());
                        cloneFn->arguments.push_back(expr->right->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        return Visitor::visit(expr);
                    }
                } else if ( cloneType->isVariant() ) {
                    reportAstChanged();
                    fnList = getCloneFunc(cloneType,cloneType);
                    if ( verifyCloneFunc(fnList, expr->at) ) {
                        if ( fnList.size()==0 ) {
                            auto clf = makeCloneVariant(expr->at, cloneType);
                            clf->privateFunction = true;
                            extraFunctions.push_back(clf);
                        }
                        auto cloneFn = make_smart<ExprCall>(expr->at, "_::clone");
                        cloneFn->arguments.push_back(expr->left->clone());
                        cloneFn->arguments.push_back(expr->right->clone());
                        return ExpressionPtr(cloneFn);
                    } else {
                        return Visitor::visit(expr);
                    }
                } else if ( cloneType->dim.size() ) {
                    reportAstChanged();
                    auto cloneFn = make_smart<ExprCall>(expr->at, "clone_dim");
                    cloneFn->arguments.push_back(expr->left->clone());
                    cloneFn->arguments.push_back(expr->right->clone());
                    return ExpressionPtr(cloneFn);
                } else {
                    error("this type can't be cloned " + cloneType->describe(), "", "",
                        expr->at, CompilationError::cant_copy);
                }
            }
            return Visitor::visit(expr);
        }
    // ExprTryCatch
        // do nothing
    // ExprReturn
        bool inferReturnType ( TypeDeclPtr & resType, ExprReturn * expr ) {
            if ( resType->isAuto() ) {
                if ( expr->subexpr ) {
                    if (!expr->subexpr->type) {
                        error("subexpresion type is not resolved yet", "", "", expr->at);
                        return false;
                    } else if (expr->subexpr->type->isAutoOrAlias()) {
                        error("subexpresion type is not fully resolved yet", "", "", expr->at);
                        return true;
                    }
                    auto resT = TypeDecl::inferGenericType(resType, expr->subexpr->type);
                    if ( !resT ) {
                        error("type can't be infered, "
                              + resType->describe() + ", returns " + expr->subexpr->type->describe(),"", "",
                              expr->at, CompilationError::cant_infer_mismatching_restrictions );
                    } else {
                        resT->ref = false;
                        TypeDecl::applyAutoContracts(resT, resType);
                        resType = resT;
                        reportAstChanged();
                        return true;
                    }
                } else {
                    resType = make_smart<TypeDecl>(Type::tVoid);
                    reportAstChanged();
                    return true;
                }
            }
            if ( resType->isVoid() ) {
                if ( expr->subexpr ) {
                    error("not expecting a return value", "", "",
                        expr->at, CompilationError::not_expecting_return_value);
                }
            } else {
                if ( !expr->subexpr ) {
                    error("expecting a return value", "", "",
                        expr->at, CompilationError::expecting_return_value);
                } else {
                    if ( !canCopyOrMoveType(resType,expr->subexpr->type,TemporaryMatters::yes) ) {
                        error("incompatible return type, expecting "
                              + resType->describe() + ", passing " + expr->subexpr->type->describe(), "", "",
                              expr->at, CompilationError::invalid_return_type);
                    }
                    if ( resType->ref && !expr->subexpr->type->ref ) {
                        error("incompatible return type, reference matters. expecting "
                              + resType->describe() + ", passing " + expr->subexpr->type->describe(), "", "",
                              expr->at, CompilationError::invalid_return_type);
                    }
                    if ( resType->isRef() && !resType->isConst() && expr->subexpr->type->isConst() ) {
                        error("incompatible return type, constant matters. expecting "
                              + resType->describe() + ", passing " + expr->subexpr->type->describe(), "", "",
                              expr->at, CompilationError::invalid_return_type);
                    }
                }
            }
            if ( resType->isRefType() ) {
                if ( !resType->canCopy() && !resType->canMove() ) {
                    error("this type can't be returned at all " + resType->describe(), "", "",
                          expr->at, CompilationError::invalid_return_type);
                }
            }
            if ( expr->moveSemantics && expr->subexpr->type->isConst() ) {
                error("can't return via move from a constant value", "", "",
                    expr->at, CompilationError::cant_move);
            }
            return false;
        }
        virtual void preVisit ( ExprReturn * expr ) override {
            Visitor::preVisit(expr);
            expr->block = nullptr;
        }
        virtual ExpressionPtr visit ( ExprReturn * expr ) override {
            if ( blocks.size() ) {
                ExprBlock * block = blocks.back();
                expr->block = block;
                block->hasReturn = true;
                if ( expr->subexpr ) {
                    if ( !expr->subexpr->type ) return Visitor::visit(expr);
                    if ( !block->returnType->ref ) {
                        expr->subexpr = Expression::autoDereference(expr->subexpr);
                    } else {
                        expr->returnReference = true;
                    }
                    expr->returnInBlock = true;
                }
                if ( inferReturnType(block->type, expr) ) {
                    block->returnType = make_smart<TypeDecl>(*block->type);
                    setBlockCopyMoveFlags(block);
                }
                if ( block->moveOnReturn && !expr->moveSemantics ) {
                    error("this type can't be copied; " + block->type->describe(),"","use return <- instead",
                          expr->at, CompilationError::invalid_return_semantics );
                }
                if ( block->returnType && block->returnType->ref && !safeExpression(expr) ) {
                    error("returning reference requires unsafe", "", "",
                        func->result->at,CompilationError::invalid_return_type);
                }
                if ( block->returnType && block->returnType->isTemp() && !safeExpression(expr) ) {
                    error("returning temporary value requires unsafe", "", "",
                        func->result->at,CompilationError::invalid_return_type);
                }
            } else {
                // infer
                func->hasReturn = true;
                if ( expr->subexpr ) {
                    if ( !expr->subexpr->type ) return Visitor::visit(expr);
                    if ( !func->result->ref ) {
                        expr->subexpr = Expression::autoDereference(expr->subexpr);
                    } else {
                        expr->returnReference = true;
                    }
                }
                inferReturnType(func->result, expr);
                if ( func->moveOnReturn && !expr->moveSemantics ) {
                    error("this type can't be copied; " + func->result->describe(),"","use return <- instead",
                          expr->at, CompilationError::invalid_return_semantics );
                }
                if ( func->result->ref && !safeExpression(expr) ) {
                    error("returning reference requires unsafe", "", "",
                        func->result->at,CompilationError::invalid_return_type);
                }
                if ( func->result->isTemp() && !safeExpression(expr) ) {
                    error("returning temporary value requires unsafe", "", "",
                        func->result->at,CompilationError::invalid_return_type);
                }
            }
            expr->type = make_smart<TypeDecl>();
            return Visitor::visit(expr);
        }
    // ExprYield
        virtual ExpressionPtr visit ( ExprYield * expr ) override {
            if ( blocks.size() ) {
                error("can't yield from inside the block", "", "",
                      expr->at, CompilationError::invalid_yield );
            } else {
                if ( !func->generator ) {
                    error("can't yield from non-generator", "", "",
                          expr->at, CompilationError::invalid_yield );
                    return Visitor::visit(expr);
                }
                if ( !expr->subexpr->type ) return Visitor::visit(expr);
                // const auto & yarg = func->arguments[1];
                // TODO: verify yield type so that error is 'yield' error, not copy or move error
                auto blk = generateYield(expr, func);
                scopes.back()->needCollapse = true;
                reportAstChanged();
                return blk;
            }
            expr->type = make_smart<TypeDecl>();
            return Visitor::visit(expr);
        }

    // ExprBreak
        virtual ExpressionPtr visit ( ExprBreak * expr ) override {
            if ( !loop.size() )
                error("'break' without a loop",  "", "",
                    expr->at, CompilationError::invalid_loop);
            return Visitor::visit(expr);
        }
    // ExprContinue
        virtual ExpressionPtr visit ( ExprContinue * expr ) override {
            if ( !loop.size() )
                error("'continue' without a loop",  "", "",
                    expr->at, CompilationError::invalid_loop);
            return Visitor::visit(expr);
        }
    // ExprIfThenElse
        bool isConstExprFunc(Function * fun) const {
            return (fun->sideEffectFlags == 0) && (fun->builtIn) && (fun->result->isFoldable());
        }
        ExpressionPtr getConstExpr ( Expression * expr ) {
            if ( expr->rtti_isConstant() && expr->type && expr->type->isFoldable() ) {
                if ( expr->type->isEnum() ) {
                    auto enumc = static_cast<ExprConstEnumeration *>(expr);
                    auto enumv = enumc->enumType->find(enumc->text);
                    if ( !enumv.second ) return nullptr;                        // not found???
                    if ( !enumv.first || !enumv.first->type ) return nullptr;   // not resolved
                    if ( !enumv.first->rtti_isConstant() ) return nullptr;      // not a constant
                    if ( enumc->baseType != enumv.first->type->baseType ) return nullptr;   // not a constant of the same type
                }
                return expr;
            }
            if ( expr->rtti_isR2V() ) {
                auto r2v = static_cast<ExprRef2Value *>(expr);
                return getConstExpr(r2v->subexpr.get());
            }
            if ( expr->rtti_isVar() ) { // global variable which happens to be constant
                auto var = static_cast<ExprVar *>(expr);
                auto variable = var->variable;
                if ( variable && variable->init && variable->type->isConst() && variable->type->isFoldable() ) {
                    if ( !var->local && !var->argument && !var->block ) {
                        if ( variable->init->rtti_isConstant() ) {
                            return variable->init;
                        }
                    }
                }
            }
            return nullptr;
        }
        virtual bool canVisitIfSubexpr ( ExprIfThenElse * expr ) override {
            if ( expr->isStatic ) {
                // static_if prevents normal resolve flow
                //  so we say 'hasReturn' for current block or function
                //  to prevent it from becoming void
                //  until static_if resolves that is
                if ( blocks.size() ) {
                    ExprBlock * block = blocks.back();
                    block->hasReturn = true;
                } else {
                    func->hasReturn = true;
                }
            }
            return !expr->isStatic;
        }
        virtual ExpressionPtr visit ( ExprIfThenElse * expr ) override {
            if ( !expr->cond->type ) {
                return Visitor::visit(expr);
            }
            // infer
            if ( !expr->cond->type->isSimpleType(Type::tBool) ) {
                error("if-then-else condition must be boolean",  "", "",
                    expr->at, CompilationError::condition_must_be_bool);
                return Visitor::visit(expr);
            }
            if ( expr->cond->type->isRef() ) {
                expr->cond = Expression::autoDereference(expr->cond);
            }
            // now, for the static if
            if ( enableInferTimeFolding || expr->isStatic ) {
                if ( auto constCond = getConstExpr(expr->cond.get()) ) {
                    reportAstChanged();
                    auto condR = static_pointer_cast<ExprConstBool>(constCond)->getValue();
                    if ( condR ) {
                        return expr->if_true;
                    } else {
                        return expr->if_false;
                    }
                } else if ( expr->isStatic ) {
                    error("static_if must resolve to constant", "", "",
                        expr->at, CompilationError::condition_must_be_static);
                    return Visitor::visit(expr);
                }
            }
            // now, to unwrap the generator
            if ( func && func->generator ) {
                // only topmost
                //  which in case of generator is 2, due to
                //  def GENERATOR { with LAMBDA { ...collapse here... } }
                if ( !blocks.empty() /* || scopes.size()!=2 */ ) {
                    return Visitor::visit(expr);
                }
                uint32_t tf = expr->if_true->getEvalFlags();
                uint32_t ff = expr->if_false ? expr->if_false->getEvalFlags() : 0;
                if ( (tf|ff) & EvalFlags::yield ) { // only unwrap if it has "yield"
                    // verify if it can be cloned at all
                    if ( expr->if_true->rtti_isBlock() ){
                        auto iftb = static_pointer_cast<ExprBlock>(expr->if_true);
                        if ( !iftb->finalList.empty() ) {
                            error("can't have final section in the if-then-else inside generator yet", "", "",
                                  expr->at, CompilationError::invalid_yield);
                            return Visitor::visit(expr);
                        }
                    }
                    if ( expr->if_false && expr->if_false->rtti_isBlock() ){
                        auto iffb = static_pointer_cast<ExprBlock>(expr->if_false);
                        if ( !iffb->finalList.empty() ) {
                            error("can't have final section in the if-then-else inside generator yet", "", "",
                                  expr->at, CompilationError::invalid_yield);
                            return Visitor::visit(expr);
                        }
                    }
                    auto blk = replaceGeneratorIfThenElse(expr, func);
                    scopes.back()->needCollapse = true;
                    reportAstChanged();
                    return blk;
                }
            }
            return Visitor::visit(expr);
        }
    // ExprWith
        virtual void preVisit ( ExprWith * expr ) override {
            Visitor::preVisit(expr);
            with.push_back(expr);
        }
        virtual ExpressionPtr visit ( ExprWith * expr ) override {
            if ( auto wT = expr->with->type ) {
                StructurePtr pSt;
                if ( wT->dim.size() ) {
                    error("with array in undefined, " + wT->describe(), "", "",
                        expr->at, CompilationError::invalid_with_type );
                } else if ( wT->isStructure() ) {
                    pSt = wT->structType;
                } else if ( wT->isPointer() && wT->firstType && wT->firstType->isStructure() ) {
                    pSt = wT->firstType->structType;
                } else {
                    error("unexpected with type " + wT->describe(), "", "",
                        expr->at, CompilationError::invalid_with_type );
                }
                if ( pSt ) {
                    for ( auto fi : pSt->fields ) {
                        for ( auto & lv : local ) {
                            if ( lv->name==fi.name ) {
                                error("with expression shadows local variable " +
                                      lv->name + " at line " + to_string(lv->at.line), "", "",
                                        expr->at, CompilationError::variable_not_found);
                            }
                        }
                    }
                }
            }
            with.pop_back();
            return Visitor::visit(expr);
        }
    // ExprWhile
        virtual void preVisit ( ExprWhile * expr ) override {
            Visitor::preVisit(expr);
            loop.push_back(expr);
        }
        virtual ExpressionPtr visit ( ExprWhile * expr ) override {
            loop.pop_back();
            if ( !expr->cond->type ) return Visitor::visit(expr);
            // infer
            if ( !expr->cond->type->isSimpleType(Type::tBool) ) {
                error("while loop condition must be boolean", "", "",
                    expr->at, CompilationError::invalid_loop);
            }
            if ( expr->cond->type->isRef() ) {
                expr->cond = Expression::autoDereference(expr->cond);
            }
            // now, to unwrap the generator
            if ( func && func->generator ) {
                // only topmost
                //  which in case of generator is 2, due to
                //  def GENERATOR { with LAMBDA { ...collapse here... } }
                if ( !blocks.empty() /* || scopes.size()!=2 */ ) {
                    return Visitor::visit(expr);
                }
                uint32_t tf = expr->body->getEvalFlags();
                if ( tf & EvalFlags::yield ) { // only unwrap if it has "yield"
                    auto blk = replaceGeneratorWhile(expr, func);
                    scopes.back()->needCollapse = true;
                    reportAstChanged();
                    return blk;
                }
            }
            return Visitor::visit(expr);
        }
    // ExprFor
        virtual void preVisit ( ExprFor * expr ) override {
            Visitor::preVisit(expr);
            DAS_ASSERT(expr->visibility.line);
            loop.push_back(expr);
            pushVarStack();
        }
        virtual void preVisitForStack ( ExprFor * expr ) override {
            Visitor::preVisitForStack(expr);
            if ( !expr->iterators.size() ) {
                error("for loop needs at least one iterator", "", "",
                    expr->at, CompilationError::invalid_loop);
                return;
            } else if ( expr->iterators.size() != expr->sources.size() ) {
                error("for loop needs as many iterators as there are sources", "", "",
                    expr->at, CompilationError::invalid_loop);
                return;
            } else if ( expr->sources.size()>MAX_FOR_ITERATORS ) {
                error("for loop has too many sources", "", "",
                    expr->at, CompilationError::invalid_loop);
                return;
            }
            // iterator variables
            int idx = 0;
            expr->iteratorVariables.clear();
            for ( auto & src : expr->sources ) {
                if (!src->type) {
                    idx++;
                    continue;
                }
                auto pVar = make_smart<Variable>();
                pVar->name = expr->iterators[idx];
                pVar->at = expr->iteratorsAt[idx];
                if ( src->type->dim.size() ) {
                    pVar->type = make_smart<TypeDecl>(*src->type);
                    pVar->type->ref = true;
                    pVar->type->dim.erase(pVar->type->dim.begin());
                    if ( !pVar->type->dimExpr.empty() ) {
                        pVar->type->dimExpr.erase(pVar->type->dimExpr.begin());
                    }
                } else if ( src->type->isGoodIteratorType() ) {
                    pVar->type = make_smart<TypeDecl>(*src->type->firstType);
                } else if ( src->type->isGoodArrayType() ) {
                    pVar->type = make_smart<TypeDecl>(*src->type->firstType);
                    pVar->type->ref = true;
                } else if ( src->type->isRange() ) {
                    pVar->type = make_smart<TypeDecl>(src->type->getRangeBaseType());
                    pVar->type->ref = false;
                    pVar->type->constant = true;
                } else if ( src->type->isString() ) {
                    pVar->type = make_smart<TypeDecl>(Type::tInt);
                    pVar->type->ref = false;
                    pVar->type->constant = true;
                } else if ( src->type->isHandle() && src->type->annotation->isIterable() ) {
                    pVar->type = make_smart<TypeDecl>(*src->type->annotation->makeIteratorType(src));
                } else {
                    error("unsupported iteration type for the loop variable " + pVar->name + ", iterating over " + src->type->describe(), "", "",
                        expr->at, CompilationError::invalid_iteration_source);
                    return;
                }
                pVar->type->constant |= src->type->isConst();
                pVar->type->temporary |= src->type->isTemp();
                pVar->source = src;
                local.push_back(pVar);
                expr->iteratorVariables.push_back(pVar);
                ++ idx;
            }
        }
        virtual ExpressionPtr visitForSource ( ExprFor * expr, Expression * that , bool last ) override {
            if ( that->type && that->type->isRef() ) {
                return Expression::autoDereference(that);
            }
            return Visitor::visitForSource(expr, that, last);
        }
        virtual ExpressionPtr visit ( ExprFor * expr ) override {
            popVarStack();
            loop.pop_back();
            // now, to unwrap the generator
            if ( func && func->generator ) {
                // only fully resolved loop
                if ( expr->sources.empty() ) {
                    return Visitor::visit(expr);
                } else if ( expr->iteratorVariables.size() != expr->sources.size() ) {
                    return Visitor::visit(expr);
                }
                // only topmost
                //  which in case of generator is 2, due to
                //  def GENERATOR { with LAMBDA { ...collapse here... } }
                if ( !blocks.empty() /* || scopes.size()!=2 */ ) {
                    return Visitor::visit(expr);
                }
                uint32_t tf = expr->body->getEvalFlags();
                if ( tf & EvalFlags::yield ) { // only unwrap if it has "yield"
                    auto blk = replaceGeneratorFor(expr, func);
                    scopes.back()->needCollapse = true;
                    reportAstChanged();
                    return blk;
                }
            }
            return Visitor::visit(expr);
        }
    // ExprLet
        virtual void preVisit ( ExprLet * expr ) override {
            Visitor::preVisit(expr);
            DAS_ASSERT(!scopes.empty());
            auto scope = scopes.back();
            expr->visibility.fileInfo = expr->at.fileInfo;
            expr->visibility.column = expr->atInit.last_column;
            expr->visibility.line = expr->atInit.last_line;
            expr->visibility.last_column = scope->at.last_column;
            expr->visibility.last_line = scope->at.last_line;
            DAS_ASSERT(expr->visibility.line);
        }
        virtual void preVisitLet ( ExprLet * expr, const VariablePtr & var, bool last ) override {
            Visitor::preVisitLet(expr, var, last);
            if ( var->type && var->type->isExprType() ) {
                return;
            }
            if ( var->type->isAlias() ) {
                auto aT = inferAlias(var->type);
                if ( aT ) {
                    var->type = aT;
                    var->type->sanitize();
                    reportAstChanged();
                } else {
                    error("undefined type " + var->type->describe(), "", "",
                        var->at, CompilationError::type_not_found);
                }
            }
            if ( var->type->isAuto() && !var->init) {
                error("local variable type can't be infered, it needs an initializer", "", "",
                      var->at, CompilationError::cant_infer_missing_initializer );
            }
            if ( func ) {
                for ( auto & fna : func->arguments ) {
                    if ( fna->name==var->name ) {
                        error("local variable " + var->name +" is shadowed by function argument "
                            + fna->name + " : " + fna->type->describe() + " at line " + to_string(fna->at.line), "", "",
                                var->at, CompilationError::variable_not_found);
                    }
                }
            }
            for ( auto & blk : blocks ) {
                for ( auto & bna : blk->arguments ) {
                    if ( bna->name==var->name ) {
                        error("local variable " + var->name +" is shadowed by block argument "
                            + bna->name + " : " + bna->type->describe() + " at line " + to_string(bna->at.line), "", "",
                                var->at, CompilationError::variable_not_found);
                    }
                }
            }
            for ( auto & lv : local ) {
                if ( lv->name==var->name ) {
                    error("local variable " + var->name +" is shadowed by another local variable "
                          + lv->name + " : " + lv->type->describe() + " at line " + to_string(lv->at.line), "", "",
                          var->at, CompilationError::variable_not_found);
                    break;
                }
            }
            if ( auto eW = hasMatchingWith(var->name) ) {
                error("local variable " + var->name + " is shadowed by with expression at line " + to_string(eW->at.line), "", "",
                      var->at, CompilationError::variable_not_found);
            }
            if ( !var->init ) {
                local.push_back(var);
            }
        }
        virtual VariablePtr visitLet ( ExprLet * expr, const VariablePtr & var, bool last ) override {
            if ( var->type && var->type->isExprType() ) {
                return Visitor::visitLet(expr,var,last);
            }
            if ( isCircularType(var->type) ) {
                return Visitor::visitLet(expr,var,last);
            }
            if ( var->type->ref && !var->init )
                error("local reference has to be initialized", "", "",
                      var->at, CompilationError::invalid_variable_type);
            if ( var->type->isVoid() )
                error("local variable can't be declared void", "", "",
                      var->at, CompilationError::invalid_variable_type);
            if ( !var->type->isLocal() && !var->type->ref )
                error("can't have local variable of type " + var->type->describe(), "", "",
                      var->at, CompilationError::invalid_variable_type);
            if ( var->type->hasClasses() && !safeExpression(expr) ) {
                error("local class requires unsafe " + var->type->describe(), "", "",
                      var->at, CompilationError::unsafe);
            }
            if ( !var->type->isAutoOrAlias() ){
                if ( var->init && var->init->rtti_isCast() ) {
                    auto castExpr = static_pointer_cast<ExprCast>(var->init);
                    if ( castExpr->castType->isAuto() ) {
                        reportAstChanged();
                        castExpr->castType = make_smart<TypeDecl>(*var->type);
                    }
                }
            }
            verifyType(var->type);
            return Visitor::visitLet(expr,var,last);
        }
        ExpressionPtr promoteToCloneToMove ( const VariablePtr & var ) {
            reportAstChanged();
            var->init_via_clone = false;
            var->init_via_move = true;
            auto c2m = make_smart<ExprCall>(var->at,"clone_to_move");
            c2m->arguments.push_back(var->init);
            return c2m;
        }
        virtual ExpressionPtr visitLetInit ( ExprLet * expr, const VariablePtr & var, Expression * init ) override {
            local.push_back(var);
            if ( !var->init->type ) {
                return Visitor::visitLetInit(expr, var, init);
            }
            if ( var->type->isAuto() ) {
                auto varT = TypeDecl::inferGenericInitType(var->type, var->init->type);
                if ( !varT || varT->isAlias() ) {
                    error("local variable " + var->name + " initialization type can't be infered, "
                          + var->type->describe() + " = " + var->init->type->describe(), "", "",
                          var->at, CompilationError::cant_infer_mismatching_restrictions );
                } else {
                    varT->ref = false;
                    TypeDecl::applyAutoContracts(varT, var->type);
                    var->type = varT;
                    var->type->sanitize();
                    reportAstChanged();
                }
            } else if ( !canCopyOrMoveType(var->type,var->init->type,TemporaryMatters::no) ) {
                error("local variable " + var->name + " initialization type mismatch, "
                      + var->type->describe() + " = " + var->init->type->describe(), "", "",
                    var->at, CompilationError::invalid_initialization_type);
            } else if ( var->type->ref && !var->init->type->isRef()) {
                error("local variable " + var->name + " initialization type mismatch. reference can't be initialized via value, "
                      + var->type->describe() + " = " + var->init->type->describe(), "", "",
                    var->at, CompilationError::invalid_initialization_type);
            } else if ( var->type->ref &&  !var->type->isConst() && var->init->type->isConst() ) {
                error("local variable " + var->name + " initialization type mismatch. const matters, "
                      + var->type->describe() + " = " + var->init->type->describe(), "", "",
                    var->at, CompilationError::invalid_initialization_type);
            } else if ( !var->type->ref && !var->init->type->canCopy() && !var->init->type->canMove() ) {
                error("local variable " + var->name + " can't be initialized at all", "", "",
                    var->at, CompilationError::invalid_initialization_type);
            } else if ( !var->type->ref && !var->init->type->canCopy()
                       && var->init->type->canMove() && !(var->init_via_move || var->init_via_clone) ) {
                error("local variable " + var->name + " can only be move-initialized","","use <- for that",
                    var->at, CompilationError::invalid_initialization_type);
            } else if ( var->init_via_move && var->init->type->isConst() ) {
                error("local variable " + var->name + " can't init (move) from a constant value", "", "",
                    var->at, CompilationError::cant_move);
            } else if ( var->init_via_clone && !var->init->type->canClone() ) {
                auto varType = make_smart<TypeDecl>(*var->type);
                varType->ref = true;
                auto fnList = getCloneFunc(varType, var->init->type);
                if ( fnList.size() && verifyCloneFunc(fnList, expr->at) ) {
                    return promoteToCloneToMove(var);
                } else {
                    error("local variable " + var->name + " of type " + var->type->describe()
                    + " can't be cloned from " + var->init->type->describe(),"", "",
                      var->at, CompilationError::cant_copy);
                }
            } else {
                if ( var->init_via_clone ) {
                    return promoteToCloneToMove(var);
                }
            }
            return Visitor::visitLetInit(expr, var, init);
        }
        virtual ExpressionPtr visit ( ExprLet * expr ) override {
            if ( func && func->generator ) {
                // only topmost
                //  which in case of generator is 2, due to
                //  def GENERATOR { with LAMBDA { ...collapse here... } }
                if ( !blocks.empty() || scopes.size()!=2 ) {
                    return Visitor::visit(expr);
                }
                for ( auto & var : expr->variables ) {
                    // TODO: verify
                    // if ( !isFullySealedType(var->type) ) {
                    if ( var->type->isAutoOrAlias() ) {
                        error("type not ready yet", "", "", var->at);
                        return Visitor::visit(expr);
                    }
                }
                auto blk = replaceGeneratorLet(expr, func, scopes.back());
                scopes.back()->needCollapse = true;
                // need to update finalizer
                vector<FunctionPtr> finFuncs = getFinalizeFunc ( func->arguments[0]->type );
                if ( finFuncs.size()==1 ) {
                    auto finFunc = finFuncs.back();
                    auto stype = func->arguments[0]->type->structType;
                    auto lname = stype->name;
                    auto newFinalizer = generateStructureFinalizer(stype);
                    finFunc->body = newFinalizer->body;
                }
                // ---
                reportAstChanged();
                return blk;
            }
            return Visitor::visit(expr);
        }
    // ExprCallMacro
        virtual ExpressionPtr visit ( ExprCallMacro * expr ) override {
            // implement call macro
            auto errc = ctx.thisProgram->errors.size();
            auto thisModule = ctx.thisProgram->thisModule.get();
            auto substitute = expr->macro->visit(ctx.thisProgram, thisModule, expr);
            if ( substitute ) {
                reportAstChanged();
                return substitute;
            }
            if ( errc==ctx.thisProgram->errors.size() ) {
                error("unsupported call macro " + expr->macro->name,  "", "",
                    expr->at, CompilationError::unsupported_call_macro);
            }
            return Visitor::visit(expr);
        }
    // ExprLooksLikeCall
        virtual void preVisit ( ExprLooksLikeCall * call ) override {
            Visitor::preVisit(call);
            call->argumentsFailedToInfer = false;
        }
        virtual ExpressionPtr visitLooksLikeCallArg ( ExprLooksLikeCall * call, Expression * arg , bool last ) override {
            if ( !arg->type ) call->argumentsFailedToInfer = true;
            if ( arg->type && arg->type->isAlias() ) call->argumentsFailedToInfer = true;
            return Visitor::visitLooksLikeCallArg(call, arg, last);
        }
    // ExprNamedCall
        ExpressionPtr demoteCall ( ExprNamedCall * expr, const FunctionPtr & pFn ) {
            auto newCall = make_smart<ExprCall>(expr->at,pFn->name);
            size_t fnArgIndex = 0;
            for ( size_t ai = 0; ai != expr->arguments.size(); ++ai ) {
                auto & arg = expr->arguments[ai];
                for ( ;; ) {
                    DAS_ASSERTF ( fnArgIndex < pFn->arguments.size(), "somehow matched function which does not match. not enough args" );
                    auto & fnArg = pFn->arguments[fnArgIndex];
                    if ( fnArg->name == arg->name ) {
                        break;
                    }
                    DAS_ASSERTF( fnArg->init, "somehow matched function, which does not match. can only skip defaults");
                    newCall->arguments.push_back(fnArg->init->clone());
                    fnArgIndex ++;
                }
                newCall->arguments.push_back(arg->value->clone());
                fnArgIndex ++;
            }
            while ( fnArgIndex < pFn->arguments.size() ) {
                auto & fnArg = pFn->arguments[fnArgIndex];
                DAS_ASSERTF( fnArg->init, "somehow matched function, which does not match. tail has to be defaults");
                newCall->arguments.push_back(fnArg->init->clone());
                fnArgIndex ++;
            }
            return newCall;
        }
        virtual void preVisit ( ExprNamedCall * call ) override {
            Visitor::preVisit(call);
            call->argumentsFailedToInfer = false;
        }
        virtual MakeFieldDeclPtr visitNamedCallArg ( ExprNamedCall * call, MakeFieldDecl * arg , bool last ) override {
            if (!arg->value->type) {
                call->argumentsFailedToInfer = true;
            } else if (arg->value->type && arg->value->type->isAlias()) {
                call->argumentsFailedToInfer = true;
            }
            return Visitor::visitNamedCallArg(call, arg, last);
        }
        virtual void reportMissing ( ExprNamedCall * expr, const string & msg, bool reportDetails,
                                    CompilationError cerror = CompilationError::function_not_found) {
            auto can1 = findCandidates(expr->name, expr->arguments);
            auto can2 = findGenericCandidates(expr->name, expr->arguments);
            can1.insert(can1.end(), can2.begin(), can2.end());
            reportFunctionNotFound(expr->name, msg + expr->name, expr->at, can1, expr->arguments, false, true, reportDetails, cerror);
        }
        virtual void reportExcess ( ExprNamedCall * expr, const string & msg, vector<FunctionPtr> can1, vector<FunctionPtr> can2,
                                    CompilationError cerror = CompilationError::function_not_found) {
            can1.insert(can1.end(), can2.begin(), can2.end());
            reportFunctionNotFound(expr->name, msg + expr->name, expr->at, can1, expr->arguments, false, true, false, cerror);
        }
        virtual void reportMissing ( ExprLooksLikeCall * expr, const vector<TypeDeclPtr>  & types,
                                    const string & msg, bool reportDetails,
                                    CompilationError cerror = CompilationError::function_not_found) {
            auto can1 = findCandidates(expr->name, types);
            auto can2 = findGenericCandidates(expr->name, types);
            can1.insert(can1.end(), can2.begin(), can2.end());
            reportFunctionNotFound(expr->name, msg + expr->describe(), expr->at, can1, types, true, true, reportDetails, cerror);
        }
        virtual void reportExcess ( ExprLooksLikeCall * expr, const vector<TypeDeclPtr>  & types,
                                   const string & msg, vector<FunctionPtr> can1, vector<FunctionPtr> can2,
                                    CompilationError cerror = CompilationError::function_not_found) {
            can1.insert(can1.end(), can2.begin(), can2.end());
            reportFunctionNotFound(expr->name, msg + expr->name, expr->at, can1, types, false, true, false, cerror);
        }
        virtual ExpressionPtr visit ( ExprNamedCall * expr ) override {
            if ( expr->argumentsFailedToInfer ) return Visitor::visit(expr);
            auto functions = findMatchingFunctions(expr->name, expr->arguments, true);
            auto generics = findMatchingGenerics(expr->name, expr->arguments);
            if ( generics.size()>1 || functions.size()>1 ) {
                reportExcess(expr, "too many matching functions or generics ", functions, generics);
            } else if ( functions.size()==0 ) {
                if ( generics.size()==1 ) {
                    reportAstChanged();
                    return demoteCall(expr,generics.back());
                } else {
                    reportMissing(expr, "no matching functions or generics ", true);
                }
            } else {
                auto fun = functions.back();
                if ( generics.size()==1 ) {
                    auto gen = generics.back();
                    if ( fun->fromGeneric != gen.get() ) {
                        reportExcess(expr, "too many matching functions or generics ", functions, generics);
                        return Visitor::visit(expr);
                    }
                }
                reportAstChanged();
                return demoteCall(expr,fun);
            }
            return Visitor::visit(expr);
        }
    // ExprCall
        virtual void preVisit ( ExprCall * call ) override {
            Visitor::preVisit(call);
            call->argumentsFailedToInfer = false;
        }
        virtual ExpressionPtr visitCallArg ( ExprCall * call, Expression * arg , bool last ) override {
            if (!arg->type) {
                call->argumentsFailedToInfer = true;
            } else if (arg->type && arg->type->isAlias()) {
                call->argumentsFailedToInfer = true;
            }
            return Visitor::visitCallArg(call, arg, last);
        }
        string getGenericInstanceName(const Function * fn) const {
            string name;
            if ( fn->module ) {
                if ( fn->module->name=="$" ) {
                    name += "builtin";
                } else {
                    name += fn->module->name;
                }
            }
            name += "`";
            name += fn->name;
            for ( auto & ch : name ) {
                if ( !isalnum(ch) && ch!='_' && ch!='`' ) {
                    ch = '_';
                }
            }
            return name;
        }
        string callCloneName ( const string & name ) {
            return "__::" + name;
        }

        // however many casts is where its at
        int computeSubstituteDistance ( ExprLooksLikeCall * expr, const FunctionPtr & fn ) const {
            int distance = 0;
            for ( size_t i=0; i!=expr->arguments.size(); ++i ) {
                const auto & argType = expr->arguments[i]->type;
                const auto & funType = fn->arguments[i]->type;
                if ( !argType->isSameType ( *funType, RefMatters::no, ConstMatters::no,
                    TemporaryMatters::no, AllowSubstitute::no) ) {
                    distance ++;
                }
            }
            return distance;
        }

        // -1 - less specialized, +1 - more specialized, 0 - we don't know
        int moreSpecialized ( const TypeDeclPtr & t1, const TypeDeclPtr & t2, TypeDeclPtr & passType  ) {
        // 1. non auto is more specialized than auto
            bool a1 = t1->isAutoOrAlias();
            bool a2 = t2->isAutoOrAlias();
            if ( a1!=a2 ) {             // if one is auto
                return a1 ? -1 : 1;
            }
        // 2. if both non-auto, the one without cast is more specialized
            if ( !a1 && !a2 ) {        // if both solid types
                bool s1 = passType->isSameType(*t1,RefMatters::no,ConstMatters::no,TemporaryMatters::no,AllowSubstitute::no);
                bool s2 = passType->isSameType(*t2,RefMatters::no,ConstMatters::no,TemporaryMatters::no,AllowSubstitute::no);
                if ( s1!=s2 ) {
                    return s1 ? 1 : -1;
                } else {
                    return 0;
                }
            }
    // at this point we are dealing with 2 auto types
        // 3. one with dim is more specialized, than one without
        //      if both have dim, one with actual value is more specialized, than the other one
            int d1 = t1->dim.size() ? t1->dim[0] : 0;
            int d2 = t2->dim.size() ? t2->dim[0] : 0;
            if ( d1!=d2 ) {
                if ( d1 && d2 ) {
                    return d1==-1 ? -1 : 1;
                } else {
                    return d1 ? 1 : -1;
                }
            }
        // 4. the one with base type of auto\alias is less specialized
        //      if both are auto\alias - we assume its the same level of specialization
            bool ba1 = t1->baseType==Type::autoinfer || t1->baseType==Type::alias;
            bool ba2 = t2->baseType==Type::autoinfer || t2->baseType==Type::alias;
            if ( ba1!=ba2 ) {
                return ba1 ? -1 : 1;
            } else if ( ba1 && ba2 ) {
                return 0;
            }
    // at this point base type is not auto for both, so lets compare the subtypes
            DAS_ASSERT(t1->baseType==passType->baseType && "how did it match otherwise?");
            DAS_ASSERT(t2->baseType==passType->baseType && "how did it match otherwise?");
        // if its an array or a pointer, we compare specialization of subtype
            if ( t1->baseType==Type::tPointer || t1->baseType==Type::tArray ) {
                return moreSpecialized(t1->firstType, t2->firstType, passType->firstType);
        // if its a table, we compare both subtypes
            } else if ( t1->baseType==Type::tTable ) {
                int i1 = moreSpecialized(t1->firstType, t2->firstType, passType->firstType);
                int i2 = moreSpecialized(t1->secondType, t2->secondType, passType->secondType);
                bool less = (i1<0) || (i2<0);
                bool more = (i1>0) || (i2>0);
                if ( less && more ) return 0;
                else if ( less ) return -1;
                else if ( more ) return 1;
                else return 0;
        // if its a tuple or a variant, we compare all subtypes
            } else if ( t1->baseType==Type::tVariant || t1->baseType==Type::tTuple ) {
                bool less = false;
                bool more = false;
                DAS_ASSERT(t1->argTypes.size() && passType->argTypes.size() && "how did it match otherwise?");
                DAS_ASSERT(t2->argTypes.size() && passType->argTypes.size() && "how did it match otherwise?");
                for ( size_t aI=0; aI!=t1->argTypes.size(); ++aI ) {
                    int cmpr = moreSpecialized(
                        t1->argTypes[aI],
                        t2->argTypes[aI],
                        passType->argTypes[aI]);
                    if ( cmpr<0 ) less = true;
                    else if ( cmpr>0 ) more = true;
                }
                if ( less && more ) return 0;
                else if ( less ) return -1;
                else if ( more ) return 1;
                else return 0;
        // if its a block, a function, or a lambda, we compare all subtypes and firstType (result)
            } else if ( t1->baseType==Type::tBlock || t1->baseType==Type::tLambda || t1->baseType==Type::tFunction ) {
                bool less = false;
                bool more = false;
                int cmpr = moreSpecialized(t1->firstType, t2->firstType, passType->firstType);
                if ( cmpr<0 ) less = true;
                else if ( cmpr>0 ) more = true;
                DAS_ASSERT(t1->argTypes.size() && passType->argTypes.size() && "how did it match otherwise?");
                DAS_ASSERT(t2->argTypes.size() && passType->argTypes.size() && "how did it match otherwise?");
                for ( size_t aI=0; aI!=t1->argTypes.size(); ++aI ) {
                    cmpr = moreSpecialized(
                        t1->argTypes[aI],
                        t2->argTypes[aI],
                        passType->argTypes[aI]);
                    if ( cmpr<0 ) less = true;
                    else if ( cmpr>0 ) more = true;
                }
                if ( less && more ) return 0;
                else if ( less ) return -1;
                else if ( more ) return 1;
                else return 0;
            }
            return 0;
        }

        // compares two generics
        //  one generic is more specialized than the other, if ALL arguments are more specialized or the
        bool copmareFunctionSpecialization ( const FunctionPtr & f1, const FunctionPtr & f2, ExprLooksLikeCall * expr ) {
            size_t nArgs = expr->arguments.size();
            bool less = false;
            bool more = false;
            for ( size_t aI = 0; aI!=nArgs; ++aI ) {
                const auto & f1A = f1->arguments[aI]->type;
                const auto & f2A = f2->arguments[aI]->type;
                int cmpr = moreSpecialized(f1A,f2A,expr->arguments[aI]->type);
                if ( cmpr<0 ) less = true;
                else if ( cmpr>0 ) more = true;
            }
            if ( more && !less ) {
                return true;
            } else {
                return false;
            }
        }

        FunctionPtr inferFunctionCall ( ExprLooksLikeCall * expr ) {
            // infer
            vector<TypeDeclPtr> types;
            types.reserve(expr->arguments.size());
            for ( auto & ar : expr->arguments ) {
                if ( !ar->type ) {
                    return nullptr;
                }
                // if its an auto or an alias
                // we only allow it, if its a block or lambda
                if ( ar->type->baseType!=Type::tBlock && ar->type->baseType!=Type::tLambda && ar->type->baseType!=Type::tFunction ) {
                    if ( ar->type->isAutoOrAlias() ) {
                        return nullptr;
                    }
                }
                types.push_back(ar->type);
            }
            auto functions = findMatchingFunctions(expr->name, types, true);
            auto generics = findMatchingGenerics(expr->name, types);
            if ( functions.size()>1 ) {
                vector<pair<int,FunctionPtr>> fnm;
                for ( auto & fn : functions ) {
                    auto dist = computeSubstituteDistance(expr, fn);
                    fnm.push_back(make_pair(dist,fn));
                }
                sort ( fnm.begin(), fnm.end(), [&] ( auto a, auto b ) {
                    return a.first < b.first;
                });
                int count = 1;
                int depth = fnm[0].first;
                while ( count < int(fnm.size()) ) {
                    if ( fnm[count].first != depth ) break;
                    count ++;
                }
                if ( count == 1 ) {
                    functions.resize(1);
                    functions[0] = fnm[0].second;
                }
            }
            if ( functions.size()==1 ) {
                auto funcC = functions.back();
                if ( funcC->firstArgReturnType ) {
                    expr->type = make_smart<TypeDecl>(*expr->arguments[0]->type);
                    expr->type->ref = false;
                } else {
                    expr->type = make_smart<TypeDecl>(*funcC->result);
                }
                // infer FORWARD types
                for ( size_t iF=0; iF!=expr->arguments.size(); ++iF ) {
                    auto & arg = expr->arguments[iF];
                    if ( arg->type->isAuto() && (arg->type->isGoodBlockType() || arg->type->isGoodLambdaType() || arg->type->isGoodFunctionType()) ) {
                        DAS_ASSERTF ( arg->rtti_isMakeBlock(), "it's always MakeBlock. this is how we construct new [[ ]]" );
                        auto mkBlock = static_pointer_cast<ExprMakeBlock>(arg);
                        auto block = static_pointer_cast<ExprBlock>(mkBlock->block);
                        auto retT = TypeDecl::inferGenericType(mkBlock->type, funcC->arguments[iF]->type);
                        DAS_ASSERTF ( retT, "how? it matched during findMatchingFunctions the same way");
                        TypeDecl::applyAutoContracts(mkBlock->type, funcC->arguments[iF]->type);
                        block->returnType = make_smart<TypeDecl>(*retT->firstType);
                        for ( size_t ba=0; ba!=retT->argTypes.size(); ++ba ) {
                            block->arguments[ba]->type = make_smart<TypeDecl>(*retT->argTypes[ba]);
                        }
                        setBlockCopyMoveFlags(block.get());
                        reportAstChanged();
                    }
                }
                // append default arguments
                for ( size_t iT = expr->arguments.size(); iT != funcC->arguments.size(); ++iT ) {
                    auto newArg = funcC->arguments[iT]->init->clone();
                    if ( !newArg->type ) {
                        // recursive resolve???
                        newArg = newArg->visit(*this);
                    }
                    if ( newArg->type && newArg->type->baseType==Type::fakeLineInfo ) {
                        newArg->at = expr->at;
                    }
                    expr->arguments.push_back(newArg);
                }
                // dereference what needs dereferences
                for ( size_t iA = 0; iA != expr->arguments.size(); ++iA )
                    if ( !funcC->arguments[iA]->type->isRef() )
                        expr->arguments[iA] = Expression::autoDereference(expr->arguments[iA]);
                // and all good
                return funcC;
            } else if ( functions.size()>1 ) {
                reportExcess(expr, types, "too many matching functions or generics ", functions, generics);
            } else if ( functions.size()==0 ) {
                // if there is more than one, we pick more specialized
                if ( generics.size()>1 ) {
                    stable_sort(generics.begin(), generics.end(), [&](const FunctionPtr & f1, const FunctionPtr & f2 ){
                        return copmareFunctionSpecialization(f1,f2,expr);
                    });
                    // if one is most specialized, we pick it, otherwise we report all of them
                    if ( copmareFunctionSpecialization(generics[0],generics[1],expr) ) {
                        generics.resize(1);
                    }
                }
                if ( generics.size()==1 ) {
                    auto oneGeneric = generics.back();
                    auto genName = getGenericInstanceName(oneGeneric.get());
                    auto instancedFunctions = findMatchingFunctions("__::" + genName, types, true);
                    if ( instancedFunctions.size() > 1 ) {
                        TextWriter ss;
                        for ( auto & instFn : instancedFunctions ) {
                            ss  << "\t" << instFn->describe() << " in "
                                << (instFn->module->name.empty() ? "this module" : instFn->module->name)
                                << "\n";
                        }
                        error("internal compiler error. multiple instances of " + genName, ss.str(), "", expr->at);
                    } else if (instancedFunctions.size() == 1) {
                        expr->name = callCloneName(genName);
                        reportAstChanged();
                    } else if (instancedFunctions.size() == 0) {
                        auto clone = oneGeneric->clone();
                        clone->name = genName;
                        clone->fromGeneric = oneGeneric.get();
                        clone->privateFunction = true;
                        if (func) {
                            clone->inferStack.emplace_back(expr->at, func);
                            clone->inferStack.insert(clone->inferStack.end(), func->inferStack.begin(), func->inferStack.end());
                        }
                        // we build alias map for the generic
                        AliasMap aliases;
                        for (;; ) {
                            bool anyFailed = false;
                            auto totalAliases = aliases.size();
                            for (size_t ai = 0; ai != types.size(); ++ai) {
                                auto argType = clone->arguments[ai]->type;
                                if (argType->isAlias()) {
                                    argType = inferPartialAliases(argType, clone, &aliases);
                                }
                                auto passType = types[ai];
                                if (!isMatchingArgument(clone, argType, passType, true, true, &aliases)) {
                                    anyFailed = true;
                                    continue;
                                }
                                TypeDecl::updateAliasMap(argType, passType, aliases);
                            }
                            if (!anyFailed) break;
                            if (totalAliases == aliases.size()) {
                                DAS_ASSERTF(0, "we should not be here. function matched arguments!");
                                break;
                            }
                        }
                        // now, we resolve types given infered aliases
                        for (size_t sz = 0; sz != types.size(); ++sz) {
                            auto & argT = clone->arguments[sz]->type;
                            if (argT->isAlias()) {
                                argT = inferPartialAliases(argT, clone, &aliases);
                            }
                            if (argT->isAuto()) {
                                auto & passT = types[sz];
                                auto resT = TypeDecl::inferGenericType(argT, passT, &aliases);
                                DAS_ASSERTF(resT, "how? we had this working at findMatchingGenerics");
                                resT->ref = false;                          // by default no ref
                                resT->implicit = argT->implicit;            // copy implicit on the arguments
                                resT->explicitConst = argT->explicitConst;  // copy const explicitness
                                TypeDecl::applyAutoContracts(resT, argT);
                                if (resT->isRefType()) {   // we don't pass boxed type by reference ever
                                    resT->ref = false;
                                }
                                resT->isExplicit = true; // this is generic for this type, and this type only
                                argT = resT;
                            }
                        }
                        // resolve tail-end types
                        for ( size_t ai = types.size(); ai!= clone->arguments.size(); ++ai ) {
                            auto & arg = clone->arguments[ai];
                            if ( arg->type->isAuto() ) {
                                if ( arg->init ) {
                                    arg->init = arg->init->visit(*this);
                                    if (arg->init->type && !arg->init->type->isAutoOrAlias()) {
                                        arg->type = make_smart<TypeDecl>(*arg->init->type);
                                        continue;
                                    }
                                }
                                error("unknown type of argument " + clone->arguments[ai]->name
                                    + "; can't instance " + oneGeneric->describe(), "",
                                    "provide argument type explicitly",
                                    expr->at, CompilationError::invalid_type);
                                return nullptr;
                            }
                        }
                        // now we verify if tail end can indeed be fully infered
                        if (!program->addFunction(clone)) {
                            auto exf = program->thisModule->functions[clone->getMangledName()];
                            DAS_ASSERTF(exf, "if we can't add, this means there is function with exactly this mangled name");
                            if (exf->fromGeneric != clone->fromGeneric) {
                                error("can't instance generic " + clone->describe(),
                                    + "\ttrying to instance from module " + clone->fromGeneric->module->name + "\n"
                                    + "\texisting instance from module " + exf->fromGeneric->module->name, "",
                                    expr->at, CompilationError::function_already_declared);
                                return nullptr;
                            }
                        }
                        expr->name = callCloneName(clone->name);
                        reportAstChanged();
                    }
                } else if ( generics.size()>1 ) {
                    reportExcess(expr, types, "too many matching functions or generics ", functions, generics);
                } else {
                    if ( auto aliasT = findAlias(expr->name) ) {
                        if ( aliasT->isCtorType() ) {
                            expr->name = das_to_string(aliasT->baseType);
                            reportAstChanged();
                        } else {
                            reportMissing(expr, types, "no matching functions or generics ", true);
                        }
                    } else {
                        reportMissing(expr, types, "no matching functions or generics ", true);
                    }
                }
            }
            return nullptr;
        }
        virtual ExpressionPtr visit ( ExprCall * expr ) override {
            if (expr->argumentsFailedToInfer) return Visitor::visit(expr);
            expr->func = inferFunctionCall(expr).get();
            /*
            // NOTE: this should not be necessary, since infer function call suppose to report every time
            if ( !expr->func ) {
                error("unknwon function " + expr->name, "", "",
                    expr->at, CompilationError::function_not_found);
            }
            */
            if ( expr->func && expr->func->unsafeOperation && !safeExpression(expr) ) {
                error("unsafe call " + expr->name + " requires unsafe", "", "",
                    expr->at, CompilationError::unsafe);
            } else if (enableInferTimeFolding && expr->func && isConstExprFunc(expr->func)) {
                vector<ExpressionPtr> cargs; cargs.reserve(expr->arguments.size());
                bool failed = false;
                for (auto & arg : expr->arguments) {
                    if ( auto carg = getConstExpr(arg.get()) ) {
                        cargs.push_back(carg);
                    } else {
                        failed = true;
                        break;
                    }
                }
                if ( !failed ) {
                    swap(cargs, expr->arguments);
                    return evalAndFold(expr);
                }
            }
            if ( expr->func ) {
                for ( const auto & ann : expr->func->annotations ) {
                    if ( ann->annotation->rtti_isFunctionAnnotation() ) {
                        auto fnAnn = static_pointer_cast<FunctionAnnotation>(ann->annotation);
                        string err;
                        auto fexpr = fnAnn->transformCall(expr, err);
                        if ( !err.empty() ) {
                            error("call annotated by " + fnAnn->name + " failed to transform, " + err, "", "",
                                           expr->at, CompilationError::annotation_failed);
                        } else if ( fexpr ) {
                            reportAstChanged();
                            return fexpr;
                        }
                    }
                }
            }
            return Visitor::visit(expr);
        }
    // StringBuilder
        virtual ExpressionPtr visitStringBuilderElement ( ExprStringBuilder *, Expression * expr, bool ) override {
            return Expression::autoDereference(expr);
        }
        virtual ExpressionPtr visit ( ExprStringBuilder * expr ) override {
            expr->type = make_smart<TypeDecl>(Type::tString);
            return Visitor::visit(expr);
        }
    // make variant
        virtual void preVisit ( ExprMakeVariant * expr ) override {
            Visitor::preVisit(expr);
            if ( expr->makeType && expr->makeType->isExprType() ) {
                return;
            }
            verifyType(expr->makeType);
            if ( expr->makeType->baseType != Type::tVariant ) {
                error("[[variant" + expr->makeType->describe() + "]] with non-variant type","", "",
                    expr->at, CompilationError::invalid_type);
            }
            if ( expr->makeType->dim.size()>1 ) {
                error("[[" + expr->makeType->describe() + "]] can only initialize single dimension arrays", "", "",
                    expr->at, CompilationError::invalid_type);
            } else if ( expr->makeType->dim.size()==1 && expr->makeType->dim[0]!=int32_t(expr->variants.size()) ) {
                error("[[" + expr->makeType->describe() + "]] dimension mismatch, provided " +
                    to_string(expr->variants.size()) + " elements", "", "",
                    expr->at, CompilationError::invalid_type);
            } else if ( expr->makeType->ref ) {
                error("[[" + expr->makeType->describe() + "]] can't be reference", "", "",
                    expr->at, CompilationError::invalid_type);
            }
        }
        virtual MakeFieldDeclPtr visitMakeVariantField ( ExprMakeVariant * expr, int index, MakeFieldDecl * decl, bool last ) override {
            if ( !decl->value->type ) {
                return Visitor::visitMakeVariantField(expr,index,decl,last);
            }
            auto fieldVariant = expr->makeType->findArgumentIndex(decl->name);
            if (fieldVariant != -1) {
                auto fieldType = expr->makeType->argTypes[fieldVariant];
                if ( !canCopyOrMoveType(fieldType,decl->value->type,TemporaryMatters::no) ) {
                    error("can't initialize field " + decl->name + "; expecting "
                        + fieldType->describe() + ", passing " + decl->value->type->describe(),"", "",
                        decl->value->at, CompilationError::invalid_type);
                }
                if (!fieldType->canCopy() && !decl->moveSemantics) {
                    error("field " + decl->name + " can't be copied; " + fieldType->describe(),"","use <- instead",
                        decl->at, CompilationError::invalid_type);
                } else if (decl->moveSemantics && decl->value->type->isConst()) {
                    error("can't move from a constant value " + decl->value->type->describe(), "", "",
                        decl->value->at, CompilationError::cant_move);
                }
            } else {
                error("field not found, " + decl->name, "", "",
                    decl->at, CompilationError::cant_get_field);
            }
            return Visitor::visitMakeVariantField(expr,index,decl,last);
        }
        virtual ExpressionPtr visit ( ExprMakeVariant * expr ) override {
            if ( expr->makeType && expr->makeType->isExprType() ) {
                return Visitor::visit(expr);
            }
            // result type
            auto resT = make_smart<TypeDecl>(*expr->makeType);
            uint32_t resDim = uint32_t(expr->variants.size());
            if ( resDim==0 ) {
                resT->dim.clear();
            } else if ( resDim!=1 ) {
                resT->dim.resize(1);
                resT->dim[0] = resDim;
            } else {
                resT->dim.clear();
            }
            expr->type = resT;
            verifyType(expr->type);
            return Visitor::visit(expr);
        }
    // make structure
        virtual void preVisit ( ExprMakeStruct * expr ) override {
            Visitor::preVisit(expr);
            if ( expr->makeType && expr->makeType->isExprType() ) {
                return;
            }
            verifyType(expr->makeType);
            if ( expr->makeType->baseType!=Type::tStructure && expr->makeType->baseType!=Type::tHandle ) {
                if ( expr->structs.size() ) {
                    error("[[" + expr->makeType->describe() + "]] with non-structure type", "", "",
                          expr->at, CompilationError::invalid_type);
                }
            }
            if ( expr->makeType->dim.size()>1 ) {
                error("[[" + expr->makeType->describe() + "]] can only initialize single dimension arrays", "", "",
                    expr->at, CompilationError::invalid_type);
            } else if ( expr->makeType->dim.size()==1 && expr->makeType->dim[0]!=int32_t(expr->structs.size()) ) {
                error("[[" + expr->makeType->describe() + "]] dimension mismatch, provided " +
                      to_string(expr->structs.size()) + " elements", "", "",
                    expr->at, CompilationError::invalid_type);
            } else if ( expr->makeType->ref ) {
                error("[[" + expr->makeType->describe() + "]] can't be reference", "", "",
                    expr->at, CompilationError::invalid_type);
            } else if ( !expr->makeType->isLocal() && !expr->isNewHandle ) {
                error("[[" + expr->makeType->describe() + "]] can't make a non local type", "", "",
                    expr->at, CompilationError::invalid_type);
            } else if ( expr->makeType->baseType==Type::tHandle && expr->isNewHandle && !expr->useInitializer ) {
                error("new [[" + expr->makeType->describe() + "]] requires initializer syntax", "",
                        "use new [[" + expr->makeType->describe() + "()]] instead",
                    expr->at, CompilationError::invalid_type);
            }
        }
        virtual MakeFieldDeclPtr visitMakeStructureField ( ExprMakeStruct * expr, int index, MakeFieldDecl * decl, bool last ) override {
            if ( !decl->value->type ) {
                return Visitor::visitMakeStructureField(expr,index,decl,last);
            }
            if ( decl->cloneSemantics ) {
                if ( !expr->block ) expr->block = makeStructWhereBlock(expr);
                DAS_ASSERT(expr->block->rtti_isMakeBlock());
                auto mkb = static_pointer_cast<ExprMakeBlock>(expr->block);
                DAS_ASSERT(mkb->block->rtti_isBlock());
                auto blk = static_pointer_cast<ExprBlock>(mkb->block);
                auto cle = convertToCloneExpr(expr,index,decl);
                blk->list.insert(blk->list.begin(), cle); // TODO: fix order. we are making them backwards now
                reportAstChanged();
                return nullptr;
            }
            if ( expr->makeType->baseType == Type::tStructure ) {
                if ( auto field = expr->makeType->structType->findField(decl->name) ) {
                    if ( !canCopyOrMoveType(field->type,decl->value->type,TemporaryMatters::no) ) {
                        error("can't initialize field " + decl->name + "; expecting "
                              +field->type->describe()+", passing "+decl->value->type->describe(), "", "",
                                decl->value->at, CompilationError::invalid_type );
                    }
                    if( !field->type->canCopy() && !decl->moveSemantics ) {
                        error("field " + decl->name + " can't be copied; " + field->type->describe(),"","use <- instead",
                              decl->at, CompilationError::invalid_type );
                    } else if (decl->moveSemantics && decl->value->type->isConst()) {
                        error("can't move from a constant value " + decl->value->type->describe(), "", "",
                            decl->value->at, CompilationError::cant_move);
                    }
                } else {
                    error("field not found, " + decl->name, "", "",
                        decl->at, CompilationError::cant_get_field);
                }
            } else if ( expr->makeType->baseType == Type::tHandle ) {
                if ( auto fldt = expr->makeType->annotation->makeFieldType(decl->name) ) {
                    if ( !fldt->isRef() ) {
                        error("field is a property, not a value; " + decl->name, "", "",
                            decl->at, CompilationError::cant_get_field);
                    }
                    if ( !canCopyOrMoveType(fldt,decl->value->type,TemporaryMatters::no) ) {
                        error("can't initialize field " + decl->name + "; expecting "
                              + fldt->describe()+", passing "+decl->value->type->describe(), "", "",
                                decl->value->at, CompilationError::invalid_type );
                    }
                    if( !fldt->canCopy() && !decl->moveSemantics ) {
                        error("field " + decl->name + " can't be copied; " + fldt->describe(),"","use <- instead",
                              decl->at, CompilationError::invalid_type );
                    } else if (decl->moveSemantics && decl->value->type->isConst()) {
                        error("can't move from a constant value " + decl->value->type->describe(), "", "",
                            decl->value->at, CompilationError::cant_move);
                    }
                } else {
                    error("annotation field not found, " + decl->name, "", "",
                        decl->at, CompilationError::cant_get_field);
                }
            }
            return Visitor::visitMakeStructureField(expr,index,decl,last);
        }
        virtual ExpressionPtr visit ( ExprMakeStruct * expr ) override {
            if ( expr->makeType && expr->makeType->isExprType() ) {
                return Visitor::visit(expr);
            }
            if ( expr->makeType && expr->makeType->isAlias() ) {
                if ( auto aT = inferAlias(expr->makeType) ) {
                    expr->makeType = aT;
                    reportAstChanged();
                } else {
                    error("undefined type " + expr->makeType->describe(),  "", "",
                        expr->makeType->at, CompilationError::type_not_found );
                }
            }
            if ( expr->block ) {
                DAS_ASSERT(expr->block->rtti_isMakeBlock());
                auto mkb = static_pointer_cast<ExprMakeBlock>(expr->block);
                DAS_ASSERT(mkb->block->rtti_isBlock());
                auto blk = static_pointer_cast<ExprBlock>(mkb->block);
                if ( blk->arguments.size()!=1 ) {
                    error("where closure should only have one argument",  "", "",
                        expr->block->at, CompilationError::invalid_block );
                } else {
                    auto arg = blk->arguments[0];
                    if ( arg->type ) {
                        int32_t rec = 0;
                        if ( expr->structs.size()>1 ) rec = int32_t(expr->structs.size());
                        auto passT = make_smart<TypeDecl>(*expr->makeType);
                        passT->dim.clear();
                        if ( rec ) passT->dim.push_back(rec);
                        if ( arg->type->isAuto() ) {
                            auto nargT = TypeDecl::inferGenericType(passT, arg->type, nullptr);
                            if ( nargT ) {
                                TypeDecl::applyAutoContracts(nargT, arg->type);
                                arg->type = nargT;
                            } else {
                                error("can't infer where closure block argument",  "", "",
                                    arg->at, CompilationError::invalid_block );
                            }
                        }
                        if ( !arg->type->isSameType(*passT,RefMatters::no,ConstMatters::no,TemporaryMatters::no) ) {
                            error("where closure block argument type mismatch, " +
                                arg->type->describe() + " vs " + expr->makeType->describe(),  "", "",
                                    arg->at, CompilationError::invalid_block );
                        } else if ( arg->type->constant ) {
                            error("where closure block argument can't be constant, " +
                                arg->type->describe() + " vs " + expr->makeType->describe(),  "", "",
                                    arg->at, CompilationError::invalid_block );
                        }
                    }
                }
            }
            // promote to make variant
            if ( expr->makeType->baseType == Type::tVariant ) {
                if ( expr->block ) {
                    error("[[variant]] can't have where closure",  "", "",
                        expr->block->at, CompilationError::invalid_block );
                    return Visitor::visit(expr);
                }
                auto mkv = make_smart<ExprMakeVariant>(expr->at);
                mkv->makeType = make_smart<TypeDecl>(*expr->makeType);
                auto allGood = true;
                for (auto & st : expr->structs) {
                    if (st->size() != 1) {
                        error("variant only supports one initializer", "", "",
                            st->front()->at, CompilationError::field_already_initialized);
                        allGood = false;
                    } else {
                        mkv->variants.push_back(st->front()->clone());
                    }
                }
                if ( allGood ) {
                    reportAstChanged();
                    return mkv;
                }
            }
            // see if there are any duplicate fields
            if ( expr->makeType->baseType == Type::tStructure ) {
                bool anyDuplicates = false;
                for ( auto & st : expr->structs ) {
                    das_set<string> fld;
                    for ( auto & fi : *st ) {
                        if ( fld.find(fi->name) != fld.end() ) {
                            error("field " + fi->name + " is already initialized", "", "",
                                fi->at, CompilationError::field_already_initialized);
                            anyDuplicates = true;
                        } else {
                            fld.insert(fi->name);
                        }
                    }
                }
                if ( anyDuplicates ) return Visitor::visit(expr);
                // see if we need to fill in missing fields
                if ( expr->useInitializer && expr->makeType->structType ) {
                    for ( auto & stf : expr->makeType->structType->fields  ) {
                        if ( stf.init  ) {
                            if ( !stf.init->type || stf.init->type->isAuto() ) {
                                error("not fully resolved yet", "", "", expr->at);
                                return Visitor::visit(expr);
                            }
                        }
                    }
                    bool anyInit = false;
                    for ( auto & st : expr->structs ) {
                        for ( auto & fi : expr->makeType->structType->fields ) {
                            if ( fi.init ) {
                                anyInit = true;
                                auto it = find_if(st->begin(), st->end(), [&](const MakeFieldDeclPtr & fd){
                                    return fd->name == fi.name;
                                });
                                if ( it==st->end() ) {
                                    auto msf = make_smart<MakeFieldDecl>(fi.at, fi.name, fi.init->clone(), !fi.init->type->canCopy(), false);
                                    st->push_back(msf);
                                    reportAstChanged();
                                }
                            }
                        }
                    }
                    if ( !anyInit ) {
                        error("[[" + expr->makeType->describe() + "() ]] does not have default initializer", "", "",
                              expr->at, CompilationError::invalid_type);
                    } else {
                        expr->useInitializer = false;
                    }
                }
                // see if we need to init fields
                if ( expr->makeType->structType ) {
                    expr->initAllFields = !expr->structs.empty();
                    for ( auto & st : expr->structs ) {
                        if ( st->size() == expr->makeType->structType->fields.size() ) {
                            for ( auto & va : *st ) {
                                if ( va->value->rtti_isMakeLocal() ) {
                                    auto mkl = static_pointer_cast<ExprMakeLocal>(va->value);
                                    expr->initAllFields &= mkl->initAllFields;
                                }
                            }
                        } else {
                            expr->initAllFields = false;
                            break;
                        }
                    }
                } else {
                    expr->initAllFields = false;
                }
            }
            // result type
            auto resT = make_smart<TypeDecl>(*expr->makeType);
            uint32_t resDim = uint32_t(expr->structs.size());
            if ( resDim==0 ) {
                resT->dim.clear();
            } else if ( resDim!=1 ) {
                resT->dim.resize(1);
                resT->dim[0] = resDim;
            } else {
                resT->dim.clear();
            }
            expr->type = resT;
            if ( expr->type->isString() ) {
                reportAstChanged();
                auto ecs = make_smart<ExprConstString>(expr->at);
                ecs->type = make_smart<TypeDecl>(Type::tString);
                return ecs;
            } else if ( expr->type->isEnumT() ) {
                auto f0 = expr->type->enumType->find(0,"");
                if ( !f0.empty() ) {
                    reportAstChanged();
                    auto et = make_smart<TypeDecl>(*expr->type);
                    et->ref = false;
                    auto ens = make_smart<ExprConstEnumeration>(expr->at, f0, et);
                    ens->type = make_smart<TypeDecl>(*et);
                    return ens;
                } else {
                    error("[[" + expr->makeType->describe() + "() ]] enumeration is missing 0 value", "", "",
                          expr->at, CompilationError::invalid_type);
                }
            } else if ( expr->type->isWorkhorseType() ) {
                expr->type->ref = false;
                reportAstChanged();
                auto ews = Program::makeConst(expr->at, expr->type, v_zero());
                ews->type = make_smart<TypeDecl>(*expr->type);
                return ews;
            } else if ( expr->type->isPointer() ) {
                expr->type->ref = false;
                reportAstChanged();
                auto ews = make_smart<ExprConstPtr>(expr->at);
                ews->type = make_smart<TypeDecl>(*expr->type);
                ews->isSmartPtr = expr->type->smartPtr;
                return ews;
            } else if ( !expr->type->isRefType() ) {
                expr->type->ref = true;
            }
            verifyType(expr->type);
            return Visitor::visit(expr);
        }
    // make tuple
        virtual void preVisit ( ExprMakeTuple * expr ) override {
            Visitor::preVisit(expr);
            expr->makeType.reset();
            expr->initAllFields = true;
        }
        virtual ExpressionPtr visitMakeTupleIndex ( ExprMakeTuple * expr, int index, Expression * init, bool lastField ) override {
            if (!init->type) {
                return Visitor::visitMakeArrayIndex(expr, index, init, lastField);
            }
            if (!init->type->canCopy() && init->type->canMove() && init->type->isConst()) {
                error("can't move from a constant value " + init->type->describe(), "", "",
                    init->at, CompilationError::cant_move);
            }
            if ( init->rtti_isMakeLocal() ) {
                auto initl = static_cast<ExprMakeLocal *>(init);
                expr->initAllFields &= initl->initAllFields;
            }
            return Visitor::visitMakeTupleIndex(expr, index, init, lastField);
        }
        virtual ExpressionPtr visit ( ExprMakeTuple * expr ) override {
            for ( auto & val : expr->values ) {
                if ( !val->type || val->type->isAutoOrAlias() ) {
                    error("not fully defined tuple element type", "", "",
                        expr->at, CompilationError::invalid_type);
                    return Visitor::visit(expr);
                }
            }
            if ( expr->recordType ) {
                if ( !expr->recordType->isTuple() ) {
                    error("internal error. ExprMakeTuple with non-tuple record type", "", "",
                        expr->at, CompilationError::invalid_type);
                    return Visitor::visit(expr);
                }
                size_t argCount = expr->values.size();
                if ( expr->recordType->argTypes.size() != argCount ) {
                    error("expecting " + to_string(argCount) + " arguments in " + expr->recordType->describe(), "", "",
                        expr->at, CompilationError::invalid_type);
                    return Visitor::visit(expr);
                }
                auto mkt = make_smart<TypeDecl>(Type::tTuple);
                for ( size_t ai=0; ai!=argCount; ++ai ) {
                    const auto & val = expr->values[ai];
                    const auto & argT = expr->recordType->argTypes[ai];
                    if ( !argT->isSameType(*val->type,RefMatters::no, ConstMatters::no, TemporaryMatters::no) ) {
                        error("invalid argument _" + to_string(ai) + ", expecting " +
                                argT->describe() + ", passing " + val->type->describe(), "", "",
                              expr->at, CompilationError::invalid_type);
                    }
                    auto valT = make_smart<TypeDecl>(*argT);
                    valT->ref = false;
                    valT->constant = false;
                    mkt->argTypes.push_back(valT);
                }
                expr->makeType = mkt;
            } else {
                expr->makeType = make_smart<TypeDecl>(Type::tTuple);
                for ( auto & val : expr->values ) {
                    auto valT = make_smart<TypeDecl>(*val->type);
                    valT->ref = false;
                    valT->constant = false;
                    expr->makeType->argTypes.push_back(valT);
                }
            }
            expr->type = make_smart<TypeDecl>(*expr->makeType);
            verifyType(expr->type);
            if ( expr->isKeyValue ) {
                auto keyType = expr->makeType->argTypes[0];
                if ( keyType->ref ) {
                    error("a => b tuple key can't be declared as a reference", "", "",
                          keyType->at,CompilationError::invalid_table_type);
                }
                if ( !keyType->isWorkhorseType() ) {
                    error("a => b tuple key has to be declare as a basic 'hashable' type", "", "",
                          keyType->at,CompilationError::invalid_table_type);
                }
            }
            for (auto & argType : expr->makeType->argTypes) {
                if ( !argType->canCopy() && !argType->canMove() ) {
                    error("tuple element has to be copyable or moveable", "", "",
                        expr->at, CompilationError::invalid_type);
                }
            }
            return Visitor::visit(expr);
        }
    // make array
        virtual void preVisit ( ExprMakeArray * expr ) override {
            Visitor::preVisit(expr);
            if ( expr->makeType && expr->makeType->isExprType() ) {
                return;
            }
            if ( expr->makeType && expr->makeType->isAuto() ) {
                return;
            }
            verifyType(expr->makeType);
            if ( expr->makeType->dim.size()>1 ) {
                error("[[" + expr->makeType->describe() + "]] can only initialize single dimension arrays", "", "",
                      expr->at, CompilationError::invalid_type);
            } else if ( expr->makeType->dim.size()==1 && expr->makeType->dim[0]!=int32_t(expr->values.size()) ) {
                error("[[" + expr->makeType->describe() + "]] dimension mismatch, provided " +
                      to_string(expr->values.size()) + " elements", "", "",
                    expr->at, CompilationError::invalid_type);
            } else if ( expr->makeType->ref ) {
                error("[[" + expr->makeType->describe() + "]] can't be reference", "", "",
                    expr->at, CompilationError::invalid_type);
            }
            expr->recordType = make_smart<TypeDecl>(*expr->makeType);
            expr->recordType->dim.clear();
            expr->initAllFields = true;
        }
        virtual ExpressionPtr visitMakeArrayIndex ( ExprMakeArray * expr, int index, Expression * init, bool last ) override {
            if ( expr->makeType && expr->makeType->isExprType() ) {
                return Visitor::visitMakeArrayIndex(expr,index,init,last);
            }
            if ( !expr->recordType ) {
                if ( index==0 ) {
                    if ( init->type && !init->type->isAutoOrAlias() ) {
                        // blah[] vs blah
                        TypeDeclPtr mkt;
                        if ( expr->makeType->dim.size() && !init->type->dim.size() ) {
                            if (expr->makeType->dim.size() == 1 && expr->makeType->dim[0] == TypeDecl::dimAuto) {
                                auto infT = make_smart<TypeDecl>(*expr->makeType);
                                infT->dim.clear();
                                mkt = TypeDecl::inferGenericType(infT, init->type);
                                if (mkt) {
                                    mkt->dim.resize(1);
                                    mkt->dim[0] = int32_t(expr->values.size());
                                }
                            }
                        } else {
                            mkt = TypeDecl::inferGenericType(expr->makeType, init->type);
                        }
                        if ( !mkt ) {
                            error("array type can't be infered, "
                                  + expr->makeType->describe() + " = " + init->type->describe(), "", "",
                                  init->at, CompilationError::invalid_array_type );
                        } else {
                            mkt->ref = false;
                            mkt->constant = false;
                            TypeDecl::applyAutoContracts(mkt, init->type);
                            expr->makeType = mkt;
                            reportAstChanged();
                            return Visitor::visitMakeArrayIndex(expr,index,init,last);
                        }
                    } else {
                        error("can't infer array auto type, first element type is undefined", "", "",
                            init->at, CompilationError::invalid_array_type );
                    }
                }
            }
            if ( !init->type|| !expr->recordType ) {
                return Visitor::visitMakeArrayIndex(expr,index,init,last);
            }
            if ( !canCopyOrMoveType(expr->recordType,init->type,TemporaryMatters::no) ) {
                error("can't initialize array element " + to_string(index) + "; expecting "
                      +expr->recordType->describe()+", passing "+init->type->describe(), "", "",
                        init->at, CompilationError::invalid_type );
            } else if ( !expr->recordType->canCopy() && expr->recordType->canMove() && init->type->isConst() ) {
                error("can't move from a constant value\n\t" + init->type->describe(), "", "",
                    init->at, CompilationError::cant_move);
            }
            if ( init->rtti_isMakeLocal() ) {
                auto initl = static_cast<ExprMakeLocal *>(init);
                expr->initAllFields &= initl->initAllFields;
            }
            return Visitor::visitMakeArrayIndex(expr,index,init,last);
        }
        virtual ExpressionPtr visit ( ExprMakeArray * expr ) override {
            if ( expr->makeType && expr->makeType->isExprType() ) {
                return Visitor::visit(expr);
            }
            if ( !expr->recordType ) {
                return Visitor::visit(expr);
            }
            if ( !expr->recordType->canCopy() && !expr->recordType->canMove() ) {
                error("array element has to be copyable or moveable", "", "",
                    expr->at, CompilationError::invalid_type);
            }
            auto resT = make_smart<TypeDecl>(*expr->makeType);
            uint32_t resDim = uint32_t(expr->values.size());
            if ( resDim!=1 || expr->makeType->dim.size() ) {
                resT->dim.resize(1);
                resT->dim[0] = resDim;
            } else {
                DAS_ASSERT(expr->values.size()==1);
                reportAstChanged();
                auto resExpr = expr->values[0];
                if ( resExpr->rtti_isMakeTuple() ) {
                    auto mkt = static_pointer_cast<ExprMakeTuple>(resExpr);
                    mkt->recordType = make_smart<TypeDecl>(*expr->recordType);
                    mkt->makeType.reset();
                }
                return resExpr;
            }
            expr->type = resT;
            verifyType(expr->type);
            return Visitor::visit(expr);
        }
    // array comprehension
        virtual void preVisitArrayComprehensionSubexpr ( ExprArrayComprehension * expr, Expression * subexpr ) override {
            Visitor::preVisitArrayComprehensionSubexpr(expr, subexpr);
            pushVarStack();
            auto eFor = static_cast<ExprFor *>(expr->exprFor.get());
            preVisitForStack(eFor);
        }
        virtual void preVisitArrayComprehensionWhere ( ExprArrayComprehension * expr, Expression * where ) override {
            Visitor::preVisitArrayComprehensionWhere(expr, where);
        }
        virtual ExpressionPtr visit ( ExprArrayComprehension * expr ) override {
            popVarStack();
            if ( expr->subexpr->type ) {
                if ( !expr->subexpr->type->canCopy() && !expr->subexpr->type->canMove() ) {
                    error("comprehension element has to be copyable or moveable", "", "",
                        expr->at, CompilationError::invalid_type);
                } else if ( expr->subexpr->type->isAutoOrAlias() ) {
                    error("comprehension element type is not resolved", "", "",
                        expr->at, CompilationError::invalid_type);
                } else {
                    auto pAC = expr->generatorSyntax ?
                        generateComprehensionIterator(expr) : generateComprehension(expr);
                    reportAstChanged();
                    return pAC;
                }
            }
            return Visitor::visit(expr);
        }
    };

    // try infer, if failed - no macros
    // run macros til any of them does work, then reinfer and restart (i.e. infer after each macro)
    void Program::inferTypes(TextWriter &logs, ModuleGroup & libGroup) {
        newLambdaIndex = 1;
        inferTypesDirty(logs);
        if ( failed() ) return;
        bool anyMacrosDidWork;
        do {
            anyMacrosDidWork = false;
            auto modMacro = [&](Module * mod) -> bool {    // we run all macros for each module
                if ( thisModule->isVisibleDirectly(mod) && mod!=thisModule.get() ) {
                    for ( const auto & pm : mod->macros ) {
                        bool anyWork = pm->apply(this, thisModule.get());
                        if ( failed() ) {                       // if macro failed, we report it, and we are done
                            error("macro " + mod->name + "::" + pm->name + " failed", "", "", LineInfo());
                            return false;
                        }
                        if ( anyWork ) {                        // if macro did anything, we done
                            inferTypesDirty(logs);
                            if ( failed() ) {                   // if it failed to infer types after, we report it
                                error("macro " + mod->name + "::" + pm->name + " failed to infer", "", "", LineInfo());
                                return false;
                            }
                            anyMacrosDidWork = true;            // if any work been done, we start over
                            return false;
                        }
                    }
                }
                return true;
            };
            Module::foreach(modMacro);
            if (failed()) break;
            if (anyMacrosDidWork) continue;
            libGroup.foreach(modMacro, "*");
        } while ( !failed() && anyMacrosDidWork );
    }

    void Program::inferTypesDirty(TextWriter & logs) {
        const bool log = options.getBoolOption("log_infer_passes",false);
        int pass = 0, maxPasses = 50;
        if (auto maxP = options.find("max_infer_passes", Type::tInt)) {
            maxPasses = maxP->iValue;
        }
        if ( log ) {
            logs << "INITIAL CODE:\n" << *this;
        }
        for ( pass = 0; pass < maxPasses; ++pass ) {
            failToCompile = false;
            errors.clear();
            InferTypes context(this);
            visit(context);
            for ( auto efn : context.extraFunctions ) {
                addFunction(efn);
            }
            bool anyMacrosDidWork = false;
            auto modMacro = [&](Module * mod) -> bool {
                if ( thisModule->isVisibleDirectly(mod) && mod!=thisModule.get() ) {
                    for ( const auto & pm : mod->inferMacros ) {
                        anyMacrosDidWork |= pm->apply(this, thisModule.get());
                    }
                }
                return true;
            };
            Module::foreach(modMacro);
            library.foreach(modMacro, "*");
            if ( log ) {
                logs << "PASS " << pass << ":\n" << *this;
                sort(errors.begin(), errors.end());
                for (auto & err : errors) {
                    logs << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
                }
            }
            if ( anyMacrosDidWork ) continue;
            if ( context.finished() ) break;
        }
        if (pass == maxPasses) {
            error("type inference exceeded maximum allowed number of passes ("+to_string(maxPasses)+")\n"
                    "this is likely due to a loop in the type system", "", "",
                LineInfo(), CompilationError::too_many_infer_passes);
        }
    }
}

