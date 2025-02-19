/*
  Copyright 2010 John Plevyak, All Rights Reserved
*/
#include "defs.h"

// Fixup namespace contamination.
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef Tuple
#undef Tuple
#endif
#ifdef Expr
#undef Expr
#endif

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/DebugInfo/DIContext.h"
#include <llvm/ExecutionEngine/GenericValue.h>
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ManagedStatic.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

using Block = llvm::BasicBlock;


// #define DEBUG_INFO 1

/*
  TODO
  - static initialization of global variables
 */

#ifndef DW_LANG_Python
#define DW_LANG_Python 0x0014
#endif

static cchar *main_fn = 0;
#ifdef DEBUG_INFO
static DIFactory *di_factory = 0;
static DICompileUnit *di_compile_unit = 0;
static HashMap<cchar *, StringHashFns, DIFile *> di_file;
static Map<Fun *, DISubprogram *> di_subprogram;
#endif

struct LLVM_CG {};

void build_main_function(LLVMContext &c, Module &m, Function *f, IRBuilder<> &b, BasicBlock *bb) {
  b.SetInsertPoint(bb);
  Function::arg_iterator args = f->arg_begin();
  args->setName("argc");
  args++;
  args->setName("argv");
  args++;
  b.CreateCall(m.getFunction("__main__"));
  b.CreateRet(ConstantInt::get(c, APInt(32, 0)));
}

Value *llvm_value(IRBuilder<> &b, Var *v) { return b.CreateLoad(v->llvm_type, v->llvm_value); }

void simple_move(IRBuilder<> &b, Var *lhs, Var *rhs) {
  if (!lhs->live) return;
  if (lhs->sym->type_kind || rhs->sym->type_kind) return;
  if (rhs->type == sym_void->type || lhs->type == sym_void->type) return;
  if (!lhs->llvm_value || !rhs->llvm_value) return;
  b.CreateStore(b.CreateLoad(rhs->llvm_type, rhs->llvm_value), lhs->llvm_value);
}

#ifdef DEBUG_INFO
DIFile *get_di_file(cchar *fn) {
  if (!fn) fn = main_fn;
  DIFile *n = di_file.get(fn);
  if (n) return n;
  n = di_factory->CreateFile(fn, "", *di_compile_unit);
  di_file.put(fn, n);
  return n;
}

DISubprogram *n = new DISubprogram(di_factory->CreateSubprogram(*di_compile_unit, fn, fn, fn /* linkage name */,
                                                                false /* internal linkage */
                                                                1,    /* */
                                                                DIType(),
                                                                false, true));

#endif

int write_llvm_prim(PNode *n, LLVMContext &c, IRBuilder<> &b, Module &m) {
  switch (n->prim->index) {
    default:
      return 0;
    case P_prim_reply:
      b.CreateRet(llvm_value(b, n->rvals[3]));
      break;
    case P_prim_period:
    case P_prim_setter:
    case P_prim_pow:
      return 0;
    case P_prim_mult:
      n->lvals[0]->llvm_value = b.CreateMul(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_div:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFDiv(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_UINT)
        n->lvals[0]->llvm_value = b.CreateUDiv(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateSDiv(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_mod:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_UINT)
        n->lvals[0]->llvm_value = b.CreateURem(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateSRem(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_add:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFAdd(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateAdd(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_subtract:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFSub(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateSub(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_lsh:
      n->lvals[0]->llvm_value = b.CreateShl(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_rsh:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_UINT)
        n->lvals[0]->llvm_value = b.CreateLShr(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateAShr(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_less:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFCmpOLT(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_UINT)
        n->lvals[0]->llvm_value = b.CreateICmpULT(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateICmpSLT(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_lessorequal:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFCmpOLE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_UINT)
        n->lvals[0]->llvm_value = b.CreateICmpULE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateICmpSLE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_greater:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFCmpOGT(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_UINT)
        n->lvals[0]->llvm_value = b.CreateICmpUGT(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateICmpSGT(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_greaterorequal:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFCmpOGE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_UINT)
        n->lvals[0]->llvm_value = b.CreateICmpUGE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateICmpSGE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_equal:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFCmpOEQ(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateICmpEQ(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_notequal:
      if (n->lvals[0]->type->num_kind == IF1_NUM_KIND_FLOAT)
        n->lvals[0]->llvm_value = b.CreateFCmpONE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      else
        n->lvals[0]->llvm_value = b.CreateICmpNE(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_and:
      n->lvals[0]->llvm_value = b.CreateAnd(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_xor:
      n->lvals[0]->llvm_value = b.CreateXor(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_or:
      n->lvals[0]->llvm_value = b.CreateOr(llvm_value(b, n->rvals[3]), llvm_value(b, n->rvals[4]));
      return 1;
    case P_prim_land: {
      auto v1 = b.CreateICmpEQ(llvm_value(b, n->rvals[3]), ConstantInt::get(n->rvals[3]->llvm_type, 0));
      auto v2 = b.CreateICmpEQ(llvm_value(b, n->rvals[4]), ConstantInt::get(n->rvals[4]->llvm_type, 0));
      n->lvals[0]->llvm_value = b.CreateAnd(v1, v2);
      return 1;
    }
    case P_prim_lor:
    case P_prim_assign:
    case P_prim_apply:
    case P_prim_by:
    case P_prim_seqcat:
    case P_prim_plus:
    case P_prim_minus:
    case P_prim_not:
    case P_prim_lnot:
    case P_prim_deref:
    case P_prim_cast:
    case P_prim_strcat:
    case P_prim_ref:
      return 0;
  }
  return 1;
}

void build_llvm_label_basicblocks(Fun *f, LLVMContext &c, IRBuilder<> &b) {
  auto theFunction = b.GetInsertBlock()->getParent();
  forv_PNode(p, f->fa_all_PNodes) {
    if (p->live && p->fa_live && p->code->kind == Code_LABEL)
      p->code->label[0]->bb = BasicBlock::Create(c, "label", theFunction);
  }
}

void write_llvm_send(PNode *n, Fun *f, LLVMContext &c, IRBuilder<> &b, Module &m) {
  if (n->prim) {
    // builtin functions
    // b.CreateCall(
    return;
  }
  Vec<Fun *> *fns = f->calls.get(n);
  if (!fns || fns->n != 1) {
    if (!fruntime_errors) fail("unable to resolve to a single function at call site");
  }
  Fun *target = fns->v[0];
  Vec<Value*> values;
  forv_MPosition(p, target->positional_arg_positions) {
    Var *av = target->args.get(p);
    if (!av->live) continue;
    values.add(av->llvm_value);
  }
  // auto ft = cast<FunctionType>(f->sym->llvm_type);
  b.CreateCall(target->llvm, ArrayRef<Value*>(values.v, values.n), target->sym->name);
}

void do_phy_nodes(PNode *n, int isucc, IRBuilder<> &b) {
  forv_PNode(p, n->phy) simple_move(b, p->lvals[isucc], p->rvals.v[0]);
}

void do_phi_nodes(PNode *n, int isucc, IRBuilder<> &b) {
  if (n->cfg_succ.n) {
    PNode *succ = n->cfg_succ[isucc];
    if (succ->phi.n) {
      int i = succ->cfg_pred_index.get(n);
      forv_PNode(pp, succ->phi) simple_move(b, pp->lvals[0], pp->rvals.v[i]);
    }
  }
}

void build_llvm_pnode(Fun *f, PNode *n, LLVMContext &c, IRBuilder<> &b, Module &m, Vec<PNode *> &done) {
#ifdef DEBUG_INFO
  b.SetCurrentDebugLocation(DebugLoc::getFromDILocation(di_factory->CreateLocation(
          (unsigned int)n->code->ast->line(), (unsigned int)n->code->ast->column(), *get_di_subprogram(f))));
#endif
  if (n->live && n->fa_live) switch(n->code->kind) {
    case Code_LABEL:
      b.CreateBr(n->code->label[0]->bb);
      b.SetInsertPoint(n->code->label[0]->bb);
      break;
    case Code_MOVE:
      for (int i = 0; i < n->lvals.n; i++) simple_move(b, n->lvals[i], n->rvals.v[i]);
      break;
    case Code_SEND:
      if (!n->prim || !write_llvm_prim(n, c, b, m)) write_llvm_send(n, f, c, b, m);
      break;
    case Code_IF:
    case Code_GOTO:
      break;
    default:
      assert(!"case");
  }
  switch (n->code->kind) {
    case Code_IF:
      if (n->live && n->fa_live) {
        if (n->rvals[0]->sym == true_type->v[0]->sym) {
          do_phy_nodes(n, 0, b);
          do_phi_nodes(n, 0, b);
          if (done.set_add(n->cfg_succ[0]))
            build_llvm_pnode(f, n->cfg_succ[0], c, b, m, done);
          else
            b.CreateBr(n->code->label[0]->bb);
        } else if (n->rvals[0]->sym == false_type->v[0]->sym) {
          do_phy_nodes(n, 1, b);
          do_phi_nodes(n, 1, b);
          if (done.set_add(n->cfg_succ[1]))
            build_llvm_pnode(f, n->cfg_succ[1], c, b, m, done);
           else
            b.CreateBr(n->code->label[1]->bb);
        } else {
          b.CreateCondBr(llvm_value(b, n->rvals[0]), n->code->label[0]->bb, n->code->label[1]->bb);
          b.SetInsertPoint(n->code->label[0]->bb);
          do_phy_nodes(n, 0, b);
          do_phi_nodes(n, 0, b);
          if (done.set_add(n->cfg_succ[0]))
            build_llvm_pnode(f, n->cfg_succ[0], c, b, m, done);
          else
            b.CreateBr(n->code->label[0]->bb);
          do_phy_nodes(n, 1, b);
          do_phi_nodes(n, 1, b);
          if (done.set_add(n->cfg_succ[1]))
            build_llvm_pnode(f, n->cfg_succ[1], c, b, m, done);
          else
            b.CreateBr(n->code->label[1]->bb);
        }
      } else {
        do_phy_nodes(n, 0, b);
        do_phi_nodes(n, 0, b);
      }
      break;
    case Code_GOTO:
      do_phi_nodes(n, 0, b);
      if (n->live && n->fa_live)
        b.CreateBr(n->code->label[0]->bb);
      break;
    case Code_SEND:
      if ((!n->live || !n->fa_live) && n->prim && n->prim->index == P_prim_reply)
        b.CreateRet(ConstantInt::get(c, APInt(32, 0)));
      else
        do_phi_nodes(n, 0, b);
      break;
    default:
      do_phi_nodes(n, 0, b);
      break;
  }
  int extra_goto = n->cfg_succ.n == 1 && n->code->kind != Code_GOTO && n->code->kind != Code_LABEL;
  forv_PNode(p, n->cfg_succ) if (done.set_add(p)) {
    build_llvm_pnode(f, p, c, b, m, done);
    extra_goto = 0;
  }
  if (extra_goto && n->cfg_succ[0]->live && n->cfg_succ[0]->fa_live) {
    assert(n->cfg_succ[0]->code->kind == Code_LABEL);
    b.CreateBr(n->cfg_succ[0]->code->label[0]->bb);
  }
  // InsertPoint saveAndClearIP()
  // restoreIP(ip);
  // SetInstDebugLocation(Instruction);
}

void build_llvm_fun(Fun *f, LLVMContext &c, IRBuilder<> &b, Module &m) {
  auto fun = f->llvm;
  m.getOrInsertNamedMetadata(f->sym->name);
  BasicBlock *bb = BasicBlock::Create(c, "entry", fun);
  b.SetInsertPoint(bb);
  // local variables
  Vec<Var *> vars, defs;
  f->collect_Vars(vars, 0, FUN_COLLECT_VARS_NO_TVALS);
  forv_Var(v, vars)
    if (v->sym->is_local || v->sym->is_fake)
      assert(!v->llvm_value);
  forv_Var(v, vars)
    if (!v->is_internal && !v->sym->is_fake)
      if (!v->llvm_value && v->live && !v->sym->is_symbol && v->type != sym_continuation)
        v->llvm_value = b.CreateAlloca(v->type->llvm_type);
  if (!f->entry)
    b.CreateRet(llvm_value(b, f->sym->ret->var));
  else {
    rebuild_cfg_pred_index(f);
    Vec<PNode *> done;
    done.set_add(f->entry);
    build_llvm_label_basicblocks(f, c, b);
    build_llvm_pnode(f, f->entry, c, b, m, done);
  }
  verifyFunction(*fun);
}

void build_llvm_prototype(Fun *f, LLVMContext &c, IRBuilder<> &b, Module &m, Type *h[]) {
  std::vector<Type *> argtype;
  MPosition p;
  p.push(1);
  for (int i = 0; i < f->sym->has.n; i++) {
    MPosition *cp = cannonicalize_mposition(p);
    p.inc();
    Var *v = f->args.get(cp);
    if (!v->live) continue;
    Type *t = v->type->llvm_type;
    if (!t) t = PointerType::getUnqual(h[v->type->id]);
    argtype.push_back(t);
  }
  Type *rettype = 0;
  if (f->sym->ret->var->live) {
    rettype = f->sym->ret->var->type->llvm_type;
    if (!rettype) rettype = PointerType::getUnqual(h[f->sym->ret->var->type->id]);
  } else
    rettype = Type::getVoidTy(c);
  FunctionType *ft = FunctionType::get(rettype, argtype, false);
  h[f->sym->id] = ft;
  f->llvm = cast<Function>(m.getOrInsertFunction(f->sym->name, ft).getCallee());
}

Type *num_type(Sym *s, LLVMContext &c) {
  switch (s->num_kind) {
    default:
      assert(!"case");
    case IF1_NUM_KIND_UINT:
    case IF1_NUM_KIND_INT:
      switch (s->num_index) {
        case IF1_INT_TYPE_1:
          return Type::getInt1Ty(c);
        case IF1_INT_TYPE_8:
          return Type::getInt8Ty(c);
        case IF1_INT_TYPE_16:
          return Type::getInt16Ty(c);
        case IF1_INT_TYPE_32:
          return Type::getInt32Ty(c);
        case IF1_INT_TYPE_64:
          return Type::getInt64Ty(c);
        default:
          assert(!"case");
      }
      break;
    case IF1_NUM_KIND_FLOAT:
      switch (s->num_index) {
        case IF1_FLOAT_TYPE_32:
          return Type::getFloatTy(c);
        case IF1_FLOAT_TYPE_64:
          return Type::getDoubleTy(c);
        default:
          assert(!"case");
          break;
      }
      break;
  }
  return 0;
}

GlobalVariable *create_global_string(LLVMContext &c, IRBuilder<> &b, Module &m, cchar *s) {
  auto v = b.CreateGlobalString(s);
  return new GlobalVariable(m, v->getType(), true, GlobalValue::InternalLinkage, v);
}

void build_llvm_globals(LLVMContext &c, IRBuilder<> &b, Module &m, Vec<Var *> &globals) {
  forv_Var(v, globals) {
    if (!v->live) continue;
    if (v->sym->is_symbol) {
      v->llvm_type = v->sym->llvm_type;
      v->llvm_value = ConstantInt::get(Type::getInt32Ty(c), v->sym->id);
    } else if (v->sym->is_constant) {
      if (v->type == sym_string) {
        v->llvm_value = create_global_string(c, b, m, v->sym->constant);
        v->llvm_type = v->llvm_value->getType();
      } else if (v->sym->imm.const_kind != IF1_NUM_KIND_NONE) {
        if (v->sym->imm.const_kind == IF1_NUM_KIND_FLOAT) {
          v->llvm_type = num_type(v->sym, c);
          if (v->sym->imm.num_index == IF1_FLOAT_TYPE_32)
            v->llvm_value = ConstantFP::get(v->llvm_type, (double)(v->sym->imm.v_float32));
          else
            v->llvm_value = ConstantFP::get(v->llvm_type, v->sym->imm.v_float64);
        } else if (v->sym->imm.const_kind == IF1_NUM_KIND_UINT || v->sym->imm.const_kind == IF1_NUM_KIND_INT) {
          v->llvm_type = v->sym->type->llvm_type;
          switch (v->sym->imm.num_index) {
            case IF1_INT_TYPE_1:
              v->llvm_value = ConstantInt::get(v->llvm_type, v->sym->imm.v_bool);
              break;
            case IF1_INT_TYPE_8:
              v->llvm_value = ConstantInt::get(v->llvm_type, v->sym->imm.v_uint8);
              break;
            case IF1_INT_TYPE_16:
              v->llvm_value = ConstantInt::get(v->llvm_type, v->sym->imm.v_uint16);
              break;
            case IF1_INT_TYPE_32:
              v->llvm_value = ConstantInt::get(v->llvm_type, v->sym->imm.v_uint32);
              break;
            case IF1_INT_TYPE_64:
              v->llvm_value = ConstantInt::get(v->llvm_type, v->sym->imm.v_uint64);
              break;
            default:
              assert(!"case");
          }
        } else if (v->sym->imm.const_kind == IF1_CONST_KIND_STRING) {
          v->llvm_value = create_global_string(c, b, m, v->sym->imm.v_string);
          v->llvm_type = v->llvm_value->getType();
        } else {
          assert(!"case");
        }
      }
    } else if (!v->sym->is_symbol && !v->sym->is_fun) {
      v->llvm_type = v->sym->llvm_type;
      v->llvm_value = new GlobalVariable(m, v->llvm_type, false, GlobalValue::ExternalLinkage, NULL, v->sym->name);
    }
  }
}

void build_llvm_types_and_globals(LLVMContext &c, IRBuilder<> &b, Module &m) {
  Vec<Sym *> typesyms;
  Vec<Var *> globals;

  collect_types_and_globals(fa, typesyms, globals);
  sym_void->type->llvm_type = Type::getVoidTy(c);
  Type **h = new (Type*);
  // setup opaque types
  forv_Sym(s, typesyms) {
    if (s->type == sym_string)
      s->llvm_type = PointerType::get(Type::getInt8Ty(c), 0);
    else if (s->num_kind)
      s->llvm_type = num_type(s, c);
    else if (s->is_symbol)
      s->llvm_type = Type::getInt32Ty(c);
    else if (!s->llvm_type)
      switch (s->type_kind) {
        default:
          s->llvm_type = PointerType::get(Type::getInt8Ty(c), 0);
        case Type_FUN:
        case Type_RECORD: {
          // h[s->id] = OpaqueType::get(c);
          break;
        }
      }
  }
  forv_Fun(f, fa->funs)
    if (f->live && !h[f->sym->id])
      // h[f->sym->id] = OpaqueType::get(c)
        ;
  // build record types
  forv_Sym(s, typesyms) {
    if (!s->llvm_type && (s->type_kind == Type_RECORD || (s->type_kind == Type_FUN && !s->fun))) {
      if (s->has.n) {
        std::vector<Type *> elements;
        forv_Sym(e, s->has) {
          Type *t = e->llvm_type;
          if (!t) t = PointerType::getUnqual(h[e->id]);
          assert(t);
          elements.push_back(t);
        }
        h[s->id] = StructType::get(c, elements);
        //cast<OpaqueType>(h[s->id].get())->refineAbstractTypeTo(new_type);
      } else
        s->llvm_type = Type::getVoidTy(c);
    }
  }
  // build function prototypes
  forv_Fun(f, fa->funs) if (f->live) build_llvm_prototype(f, c, b, m, h);
  // resolve record types
  forv_Sym(s, typesyms) {
    if (!s->llvm_type && (s->type_kind == Type_RECORD || (s->type_kind == Type_FUN && !s->fun))) {
      if (s->has.n) {
        s->llvm_type = cast<StructType>(h[s->id]);  // NOOP ?
      }
    }
  }
  // resolve function types
  forv_Fun(f, fa->funs) if (f->live) f->sym->llvm_type = cast<FunctionType>(h[f->sym->id]);
  delete h;
  // build forward function declarations
  forv_Fun(f, fa->funs) if (f->live) {
    auto ft = cast<FunctionType>(f->sym->llvm_type);
    f->llvm = Function::Create(ft, Function::ExternalLinkage, f->sym->name, m);
  }
  build_llvm_globals(c, b, m, globals);
}

void build_llvm_ir(LLVMContext &c, Module &m, Fun *main) {
  Function *main_func = cast<Function>(
      m.getOrInsertFunction("main", IntegerType::getInt32Ty(c), IntegerType::getInt32Ty(c),
                            PointerType::getUnqual(PointerType::getUnqual(IntegerType::getInt8Ty(c)))).getCallee());

  BasicBlock *bb = BasicBlock::Create(c, "main.0", main_func);
  IRBuilder<> b(bb);
  build_llvm_types_and_globals(c, b, m);
  forv_Fun(f, fa->funs) if (!f->is_external && f->live) build_llvm_fun(f, c, b, m);
  build_main_function(c, m, main_func, b, bb);
}

void llvm_codegen(FA *fa_unused, Fun *main, cchar *fn) {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  main_fn = fn;
  LLVMContext c;
  llvm_shutdown_obj shutdown;
  auto m = std::make_unique<Module>("pyc", c);
  char ver[64] = "pyc ";
  get_version(ver + strlen(ver));
  auto di = new DIBuilder(*m);
  auto di_compile_unit = di->createCompileUnit(
        DW_LANG_Python,
        di->createFile(fn, "."),
        ver,    // producer
        false,  // optimized
        "",     // flags
        0       // runtime version
        );
  (void)di_compile_unit;  // ??
  if (codegen_jit) {
    InitializeNativeTarget();
    EngineBuilder eb;
    ExecutionEngine *ee = eb.create();
    m->setDataLayout(ee->getDataLayout());
    build_llvm_ir(c, *m, main);
    ee->addModule(std::move(m));
    std::vector<GenericValue> args;
    Function *f = m->getFunction("__main__");
    ee->runFunction(f, ArrayRef(args));
  } else {
    char object_fn[512];
    strcpy(object_fn, fn);
    *strrchr(object_fn, '.') = 0;
    strcat(object_fn, ".o");
    std::error_code EC;
    raw_fd_ostream dest(object_fn, EC);
    if (EC) {
      errs() << "Could not open file: " << EC.message();
      exit(1);
    }
    auto target_triple = LLVMGetDefaultTargetTriple();
    std::string error;
    auto target = TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
      errs() << error;
      exit(1);
    }
    TargetOptions opt;
    auto RM = std::optional<Reloc::Model>();
    auto machine = target->createTargetMachine(target_triple, "generic" /* CPU */, "" /* Features */, opt, RM);
    m->setDataLayout(machine->createDataLayout());
    build_llvm_ir(c, *m, main);
    legacy::PassManager pass;
    if (machine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
      errs() << "TargetMachine can't emit a file of this type";
      exit(1);
    }
    pass.run(*m);
    dest.flush();
  }
}

int llvm_codegen_compile(cchar *filename) {
  char target[512], s[1024];
  strcpy(target, filename);
  *strrchr(target, '.') = 0;
  sprintf(s, "make --no-print-directory -f %s/Makefile.llvm_cg CG_ROOT=%s CG_TARGET=%s CG_FILES=%s.o %s %s", system_dir,
          system_dir, target, target, codegen_optimize ? "OPTIMIZE=1" : "", codegen_debug ? "DEBUG=1" : "");
  return system(s);
}
