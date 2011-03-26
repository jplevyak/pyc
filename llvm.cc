/*
  Copyright 2010 John Plevyak, All Rights Reserved
*/
#include "defs.h"

#undef Module

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/IRBuilder.h"
#include <iostream>
#include <fstream>

using namespace llvm;

// #define DEBUG_INFO 1

/*
  TODO
  - static initialization of global variables
 */

#ifndef DW_LANG_Python
#define DW_LANG_Python 0x0014
#endif

static cchar *main_fn = 0;
static DIFactory *di_factory = 0;
static DICompileUnit *di_compile_unit = 0;
static HashMap<cchar *, StringHashFns, DIFile *> di_file;
static Map<Fun *, DISubprogram *> di_subprogram;

static void build_main_function(LLVMContext &c, Module &m) {
  Function *main_func = cast<Function>(
    m.getOrInsertFunction("main", IntegerType::getInt32Ty(c),
                          IntegerType::getInt32Ty(c),
                          PointerType::getUnqual(PointerType::getUnqual(IntegerType::getInt8Ty(c))), NULL));
  Function::arg_iterator args = main_func->arg_begin();
  Value *arg_0 = args++;
  arg_0->setName("argc");
  Value *arg_1 = args++;
  arg_1->setName("argv");
  BasicBlock *bb = BasicBlock::Create(c, "main.0", main_func);
  CallInst *main_call = CallInst::Create(m.getFunction("__main__"), "", bb);
  main_call->setTailCall(false);
  ReturnInst::Create(c, ConstantInt::get(c, APInt(32, 0)), bb);
}

static Value *llvm_value(IRBuilder<> &b, Var *v) {
  return b.CreateLoad(v->llvm_value);
}

static void simple_move(IRBuilder<> &b, Var *lhs, Var *rhs) {
  if (!lhs->live)
    return;
  if (lhs->sym->type_kind || rhs->sym->type_kind)
    return;
  if (rhs->type == sym_void->type || lhs->type == sym_void->type)
    return;
  if (!lhs->llvm_value || !rhs->llvm_value)
    return;
  b.CreateStore(b.CreateLoad(rhs->llvm_value), lhs->llvm_value);
}

#ifdef DEBUG_INFO
static DIFile *get_di_file(cchar *fn) {
  if (!fn) 
    fn = main_fn;
  DIFile *n = di_file.get(fn);
  if (n)
    return n;
  n = di_factory->CreateFile(fn, "", *di_compile_unit);
  di_file.put(fn, n);
  return n;
}
 
static DISubprogram *   
  n = new DISubprogram(di_factory->CreateSubprogram(
                         *di_compile_unit,
                         fn, fn, fn /* linkage name */,
                         false /* internal linkage */
                         1, /* */
                         llvm::DIType(), 
                         false, 
                         true));

#endif

static int write_llvm_prim(PNode *n, LLVMContext &c, IRBuilder<> &b, Module &m) {
  //int o = (n->rvals.v[0]->sym == sym_primitive) ? 2 : 1;
  //bool listish_tuple = false;
  switch (n->prim->index) {
    default: return 0;
    case P_prim_reply:
      b.CreateRet(llvm_value(b, n->rvals[3]));
      break;
  }
  return 1;
}

static void write_llvm_send(PNode *n, LLVMContext &c, IRBuilder<> &b, Module &m) {
}

static void build_llvm_pnode(Fun *f, PNode *n,
                             LLVMContext &c, IRBuilder<> &b, Module &m,
                             Vec<PNode*> &done) 
{
#ifdef DEBUG_INFO
  b.SetCurrentDebugLocation(
    DebugLoc::getFromDILocation(
      di_factory->CreateLocation(
        (unsigned int)n->code->ast->line(), 
        (unsigned int)n->code->ast->column(),
        *get_di_subprogram(f))));
#endif
  if (n->live && n->fa_live) {
    if (n->code->kind == Code_MOVE) {
      for (int i = 0; i < n->lvals.n; i++)
        simple_move(b, n->lvals[i], n->rvals.v[i]);
    } else if (n->code->kind == Code_SEND) {
      if (!n->prim || !write_llvm_prim(n, c, b, m))
        write_llvm_send(n, c, b, m);
    }
  }
  switch (n->code->kind) {
    case Code_IF:
      if (n->live && n->fa_live) {
        if (n->rvals[0]->sym == true_type->v[0]->sym) {
          do_phy_nodes(n, 0);
          do_phi_nodes(n, 0);
          if (done.set_add(n->cfg_succ[0]))
            build_llvm_pnode(f, n->cfg_succ[0], c, b, m, done);
        } else if (n->rvals[0]->sym == false_type->v[0]->sym) {
          do_phy_nodes(n, 1);
          do_phi_nodes(n, 1);
          if (done.set_add(n->cfg_succ[1]))
            build_llvm_pnode(f, n->cfg_succ[1], c, b, m, done);
        } else {
          b.CreateCondBr(llvm_value(b, n->rvals[0]));
          do_phy_nodes(n, 0);
          do_phi_nodes(n, 0);
          if (done.set_add(n->cfg_succ[0]))
            build_llvm_pnode(f, n->cfg_succ[0], c, b, m, done);
          else
            fprintf("  goto L%d;\n", n->code->label[0]->id);
          fprintf("  } else {\n");
          do_phy_nodes(n, 1);
          do_phi_nodes(n, 1);
          if (done.set_add(n->cfg_succ[1]))
            build_llvm_pnode(f, n->cfg_succ[1], c, b, m, done);
          else
            fprintf("  goto L%d;\n", n->code->label[1]->id);
          fputs("  }\n", fp);
        }
      } else {
        do_phy_nodes(n, 0);
        do_phi_nodes(n, 0);
      }
      break;
    case Code_GOTO:
      do_phi_nodes(n, 0);
      if (n->live && n->fa_live)
        fprintf("  goto L%d;\n", n->code->label[0]->id);
      break;
    case Code_SEND:
      if ((!n->live || !n->fa_live) && n->prim && n->prim->index == P_prim_reply)
        fprintf("  return 0;\n");
      else
        do_phi_nodes(n, 0);
      break;
    default:
      do_phi_nodes(n, 0);
      break;
  }
  // InsertPoint saveAndClearIP()
  // restoreIP(ip);
  // SetInstDebugLocation(Instruction);
}

static void build_llvm_fun(Fun *f, LLVMContext &c, IRBuilder<> &b, Module &m) {
  Function *fun = f->llvm;
  //m.getOrInsertNamedMetadata(f->sym->name);
  BasicBlock *bb = BasicBlock::Create(c, "entry", fun);
  b.SetInsertPoint(bb);
  // local variables
  Vec<Var *> vars, defs;
  f->collect_Vars(vars, 0, FUN_COLLECT_VARS_NO_TVALS);
  forv_Var(v, vars)
    if (v->sym->is_local || v->sym->is_fake)
      assert(!v->llvm_value);
  forv_Var(v, vars) if (!v->is_internal && !v->sym->is_fake)
    if (!v->llvm_value && v->live && !v->sym->is_symbol && v->type != sym_continuation)
      v->llvm_value = b.CreateAlloca(v->type->llvm_type);
  if (!f->entry)
    b.CreateRet(llvm_value(f->sym->ret->var));
  else {
    rebuild_cfg_pred_index(f);
    Vec<PNode *> done;
    done.set_add(f->entry);
    build_llvm_pnode(f, f->entry, c, b, m, done);
  }
  verifyFunction(*fun);
}

static void build_llvm_prototype(Fun *f, LLVMContext &c, IRBuilder<> &b, Module &m, PATypeHolder *h) {
  std::vector<const Type*> argtype;
  MPosition p;
  p.push(1);
  for (int i = 0; i < f->sym->has.n; i++) {
    MPosition *cp = cannonicalize_mposition(p);
    p.inc();
    Var *v = f->args.get(cp);
    if (!v->live) continue;
    const Type *t = v->type->llvm_type;
    if (!t) t = PointerType::getUnqual(h[v->type->id]);
    argtype.push_back(t);
  }
  const Type *rettype = 0;
  if (f->sym->ret->var->live) {
    rettype = f->sym->ret->var->type->llvm_type;
    if (!rettype) rettype = PointerType::getUnqual(h[f->sym->ret->var->type->id]);
  } else
    rettype = Type::getVoidTy(c);
  FunctionType *ft = FunctionType::get(rettype, argtype, false);
  cast<OpaqueType>(h[f->sym->id].get())->refineAbstractTypeTo(ft);
  m.getOrInsertFunction(f->sym->name, ft);
}

static const Type *
num_type(Sym *s, LLVMContext &c) {
  switch (s->num_kind) {
    default: assert(!"case");
    case IF1_NUM_KIND_UINT:
    case IF1_NUM_KIND_INT:
      switch (s->num_index) {
        case IF1_INT_TYPE_1:  return Type::getInt1Ty(c);
        case IF1_INT_TYPE_8:  return Type::getInt8Ty(c);
        case IF1_INT_TYPE_16: return Type::getInt16Ty(c);
        case IF1_INT_TYPE_32: return Type::getInt32Ty(c);
        case IF1_INT_TYPE_64: return Type::getInt64Ty(c);
        default: assert(!"case");
      }
      break;
    case IF1_NUM_KIND_FLOAT:
      switch (s->num_index) {
        case IF1_FLOAT_TYPE_32:  return Type::getFloatTy(c);
        case IF1_FLOAT_TYPE_64:  return Type::getDoubleTy(c);
        default: assert(!"case");
          break;
      }
      break;
  }
  return 0;
}

static GlobalVariable *create_global_string(LLVMContext &c, Module &m, cchar *s) {
  Constant *str = ConstantArray::get(c, s, true);
  GlobalVariable *gv = new GlobalVariable(m, str->getType(),
                                          true, GlobalValue::InternalLinkage,
                                          str, "", 0, false);
  // gv->setName(name); optionally set name
  return gv;
}

static void build_llvm_globals(LLVMContext &c, IRBuilder<> &b, Module &m, Vec<Var *> &globals) {
  forv_Var(v, globals) {
    if (!v->live)
      continue;
    if (v->sym->is_symbol) {
      v->llvm_type = v->sym->llvm_type;
      v->llvm_value = ConstantInt::get(Type::getInt32Ty(c), v->sym->id);
    } else if (v->sym->is_constant) {
      if (v->type == sym_string) {
        v->llvm_value = create_global_string(c, m, v->sym->constant);
        v->llvm_type = v->llvm_value->getType();
      } else if (v->sym->imm.const_kind != IF1_NUM_KIND_NONE) {
        if (v->sym->imm.const_kind == IF1_NUM_KIND_FLOAT) {
          v->llvm_type = num_type(v->sym, c);
          if (v->sym->imm.num_index == IF1_FLOAT_TYPE_32)
            v->llvm_value = ConstantFP::get(v->llvm_type, v->sym->imm.v_float32);
          else 
            v->llvm_value = ConstantFP::get(v->llvm_type, v->sym->imm.v_float64);
        } else if (v->sym->imm.const_kind == IF1_NUM_KIND_UINT || 
                   v->sym->imm.const_kind == IF1_NUM_KIND_INT) {
          v->llvm_type = v->sym->type->llvm_type;
          switch (v->sym->imm.num_index) {
            case IF1_INT_TYPE_1:  ConstantInt::get(v->llvm_type, v->sym->imm.v_bool); break;
            case IF1_INT_TYPE_8:  ConstantInt::get(v->llvm_type, v->sym->imm.v_uint8); break;
            case IF1_INT_TYPE_16: ConstantInt::get(v->llvm_type, v->sym->imm.v_uint16); break;
            case IF1_INT_TYPE_32: ConstantInt::get(v->llvm_type, v->sym->imm.v_uint32); break;
            case IF1_INT_TYPE_64: ConstantInt::get(v->llvm_type, v->sym->imm.v_uint64); break;
            default: assert(!"case");
          }
        } else if (v->sym->imm.const_kind == IF1_CONST_KIND_STRING) {
          v->llvm_value = create_global_string(c, m, v->sym->imm.v_string);
          v->llvm_type = v->llvm_value->getType();
        } else {
          assert(!"case");
        }
      }
    } else if (!v->sym->is_symbol && !v->sym->is_fun) {
      v->llvm_type = v->sym->llvm_type;
      v->llvm_value = new GlobalVariable(m, v->llvm_type, false, GlobalValue::ExternalLinkage,
                                         NULL, v->sym->name);
    }
  }
}

static void build_llvm_types_and_globals(LLVMContext &c, IRBuilder<> &b, Module &m) {
  Vec<Sym *> typesyms;
  Vec<Var *> globals;

  collect_types_and_globals(fa, typesyms, globals);
  sym_void->type->llvm_type = Type::getVoidTy(c);
  PATypeHolder *h = new PATypeHolder[fa->pdb->if1->allsyms.n];
  // setup opaque types
  forv_Sym(s, typesyms) {
    if (s->type == sym_string)
      s->llvm_type = b.getInt8PtrTy();
    else if (s->num_kind)
      s->llvm_type = num_type(s, c);
    else if (s->is_symbol)
      s->llvm_type = Type::getInt32Ty(c);
    else if (!s->llvm_type)
      switch (s->type_kind) {
        default: b.getInt8PtrTy();
        case Type_FUN:
        case Type_RECORD: {
          h[s->id] = OpaqueType::get(c);
          break;
        }
      }
  }
  forv_Fun(f, fa->funs)
    if (f->live && !h[f->sym->id])
      h[f->sym->id] = OpaqueType::get(c);
  // build record types
  forv_Sym(s, typesyms) {
    if (!s->llvm_type && (s->type_kind == Type_RECORD || (s->type_kind == Type_FUN && !s->fun))) {
      if (s->has.n) {
        std::vector<const Type*> elements;
        forv_Sym(e, s->has) {
          const Type *t = e->llvm_type;
          if (!t) t = PointerType::getUnqual(h[e->id]);
          assert(t);
          elements.push_back(t);
        }
        const StructType *new_type = StructType::get(c, elements);
        cast<OpaqueType>(h[s->id].get())->refineAbstractTypeTo(new_type);
      } else
        s->llvm_type = Type::getVoidTy(c);
    }
  }
  // build function prototypes
  forv_Fun(f, fa->funs)
    if (f->live)
      build_llvm_prototype(f, c, b, m, h);
  // resolve record types
  forv_Sym(s, typesyms) {
    if (!s->llvm_type && (s->type_kind == Type_RECORD || (s->type_kind == Type_FUN && !s->fun))) {
      if (s->has.n) {
        const StructType *new_type = cast<StructType>(h[s->id]);
        m.addTypeName(s->name, new_type);
        s->llvm_type = new_type;
      }
    }
  }
  // resolve function types
  forv_Fun(f, fa->funs)
    if (f->live)
      f->sym->llvm_type = cast<FunctionType>(h[f->sym->id]);
  delete[] h;
  // build forward function declarations
  forv_Fun(f, fa->funs) if (f->live) {
    const FunctionType *ft = cast<FunctionType>(f->sym->llvm_type);
    f->llvm = Function::Create(ft, Function::ExternalLinkage, f->sym->name);
  }
  build_llvm_globals(c, b, m, globals);
}

static void build_llvm_ir(LLVMContext &c, Module &m, Fun *main) {
  IRBuilder<> b(c);

  build_llvm_types_and_globals(c, b, m);
  forv_Fun(f, fa->funs) if (!f->is_external && f->live)
    build_llvm_fun(f, c, b, m);
  build_main_function(c, m);
}

void llvm_codegen(FA *fa_unused, Fun *main, cchar *fn) {
  main_fn = fn;
  LLVMContext &c = getGlobalContext();
  Module m("pyc", c);
  char ver[64] = "pyc ";
  get_version(ver + strlen(ver));
  di_factory = new DIFactory(m);
  di_compile_unit = new DICompileUnit(di_factory->CreateCompileUnit(
                                        DW_LANG_Python,
                                        fn,
                                        "", // directory
                                        ver, // producer
                                        true, // main
                                        false, // optimized
                                        "" // flags
                                        ));
  build_llvm_ir(c, m, main);
  if (codegen_jit) {
    InitializeNativeTarget(); 
    ExecutionEngine *ee = EngineBuilder(&m).create();
    std::vector<GenericValue> args;
    Function *f = m.getFunction("__main__");
    ee->runFunction(f, args);
  } else {
    char target[512];
    strcpy(target, fn);
    *strrchr(target, '.') = 0;
    strcat(target, ".bc");
    std::string errinfo;
    raw_ostream *out = new raw_fd_ostream(target, errinfo, raw_fd_ostream::F_Binary);
    WriteBitcodeToFile(&m, *out);
    delete out;
  }
  // llvm_shutdown();
}

int llvm_codegen_compile(cchar *filename) {
  char target[512], s[1024];
  strcpy(target, filename);
  *strrchr(target, '.') = 0;
  sprintf(s, "make --no-print-directory -f %s/Makefile.llvm_cg CG_ROOT=%s CG_TARGET=%s CG_FILES=%s.o %s %s",
          system_dir, system_dir, target, target, codegen_optimize ? "OPTIMIZE=1" : "", codegen_debug ? "DEBUG=1" : "");
  return system(s);
}

