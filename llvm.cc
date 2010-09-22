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
#include "llvm/Target/TargetSelect.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/IRBuilder.h"
#include <iostream>
#include <fstream>

using namespace llvm;

static void build_main_function(Module *m) {
  Function *main_func = cast<Function>(
    m->getOrInsertFunction("main", IntegerType::getInt32Ty(m->getContext()),
                           IntegerType::getInt32Ty(m->getContext()),
                           PointerType::getUnqual(PointerType::getUnqual(
                                                    IntegerType::getInt8Ty(m->getContext()))), NULL));
  Function::arg_iterator args = main_func->arg_begin();
  Value *arg_0 = args++;
  arg_0->setName("argc");
  Value *arg_1 = args++;
  arg_1->setName("argv");
  BasicBlock *bb = BasicBlock::Create(m->getContext(), "main.0", main_func);
  CallInst *main_call = CallInst::Create(m->getFunction("__main__"), "", bb);
  main_call->setTailCall(false);
  ReturnInst::Create(m->getContext(), ConstantInt::get(m->getContext(), APInt(32, 0)), bb);
}

static void build_llvm_fun(Fun *f, LLVMContext &c, IRBuilder<> &b, Module *m) {
  const FunctionType *ft = cast<FunctionType>(f->sym->llvm_type);
  f->llvm = Function::Create(ft, Function::ExternalLinkage, f->sym->name, m);
  // Function *fun = f->llvm;
  // BasicBlock *bb = BasicBlock::Create(c, "entry", fun);
  // b.SetCurrentDebugLocation(DebugLoc::get(s->line(), s->column()));
  // SetInsertionPoint(BB, InsertPt);
  // InsertPoint saveAndClearIP()
  // restoreIP(ip);
  // SetInstDebugLocation(Instruction);
}

static void build_llvm_prototype(Fun *f, LLVMContext &c, IRBuilder<> &b, Module *m, PATypeHolder *h) {
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
  m->getOrInsertFunction(f->sym->name, ft);
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

static void build_llvm_types(LLVMContext &c, IRBuilder<> &b, Module *m, FA *fa) {
  Vec<Sym *> typesyms;
  Vec<Var *> globals;

  collect_types_and_globals(fa, typesyms, globals);
  sym_void->type->llvm_type = Type::getVoidTy(c);
  PATypeHolder *h = new PATypeHolder[fa->pdb->if1->allsyms.n];
  // setup opaque types
  forv_Sym(s, typesyms) {
    if (s->num_kind)
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
        m->addTypeName(s->name, new_type);
        s->llvm_type = new_type;
      }
    }
  }
  // resolve function types
  forv_Fun(f, fa->funs)
    if (f->live)
      f->sym->llvm_type = cast<FunctionType>(h[f->sym->id]);
  delete[] h;
}

static Module *build_llvm_ir(LLVMContext &c, FA *fa, Fun *main) {
  llvm::IRBuilder<> b(c);
  Module *m = new Module("pyc", c);

  build_llvm_types(c, b, m, fa);
  forv_Fun(f, fa->funs) if (f != main && !f->is_external && f->live)
    build_llvm_fun(f, c, b, m);
  build_main_function(m);
  return m;
}

void llvm_codegen(FA *fa, Fun *main, cchar *fn) {
  LLVMContext &c = getGlobalContext();
  
  Module *m = build_llvm_ir(c, fa, main);
  if (codegen_jit) {
    InitializeNativeTarget(); 
    ExecutionEngine *ee = EngineBuilder(m).create();
    std::vector<GenericValue> args;
    Function *f = m->getFunction("__main__");
    ee->runFunction(f, args);
  } else {
    char target[512];
    strcpy(target, fn);
    *strrchr(target, '.') = 0;
    strcat(target, ".bc");
    std::string errinfo;
    raw_ostream *out = new raw_fd_ostream(target, errinfo, raw_fd_ostream::F_Binary);
    WriteBitcodeToFile(m, *out);
    delete out;
  }
  delete m;
  llvm_shutdown();
}

int llvm_codegen_compile(cchar *filename) {
  char target[512], s[1024];
  strcpy(target, filename);
  *strrchr(target, '.') = 0;
  sprintf(s, "make --no-print-directory -f %s/Makefile.llvm_cg CG_ROOT=%s CG_TARGET=%s CG_FILES=%s.o %s %s",
          system_dir, system_dir, target, target, codegen_optimize ? "OPTIMIZE=1" : "", codegen_debug ? "DEBUG=1" : "");
  return system(s);
}

