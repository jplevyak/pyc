#include <ctype.h>
#include "ifadefs.h"
#include "pattern.h"
#include "cg.h"
#include "prim.h"
#include "if1.h"
#include "builtin.h"
#include "pdb.h"
#include "fun.h"
#include "pnode.h"
#include "fa.h"
#include "var.h"
#include "fail.h"

static inline cchar *
c_type(Var *v) {
  if (!v->type->cg_string)
    return "_CG_void";
  return v->type->cg_string;
}

static inline cchar *
c_type(Sym *s) {
  if (!s->type->cg_string)
    return "_CG_void";
  return s->type->cg_string;
}

static void
write_c_fun_proto(FILE *fp, Fun *f, int type = 0) {
  assert(f->rets.n == 1);
  fputs(f->rets[0]->type->cg_string, fp);
  if (type)
    fputs(" (*", fp);
  else
    fputs(" ", fp);
  if (type)
    fputs(f->cg_structural_string, fp);
  else
    fputs(f->cg_string, fp);
  if (type)
    fputs(")(", fp);
  else
    fputs("(", fp);
  MPosition p;
  p.push(1);
  for (int i = 0; i < f->sym->has.n; i++) {
    MPosition *cp = cannonicalize_mposition(p);
    Var *v = f->args.get(cp);
    if (i)
      fputs(", ", fp);
    fputs(c_type(v), fp);
    fprintf(fp, " a%d", i);
    p.inc();
  }
  fputs(")", fp);
}

static void
write_c_type(FILE *fp, Var *v) {
  fprintf(fp, c_type(v));
}

static int
application_depth(PNode *n) {
  if (n->rvals[0]->type->fun)
    return 1;
  else
    return 1 + application_depth(n->rvals[0]->def);
}

static void
write_c_apply_arg(FILE *fp, cchar *base, int n, int i) {
  fputs(base, fp);
  for (int j = i; j < n - 2; j++)
    fputs("->e0", fp);
  if (!i)
    fputs("->e0", fp);
  else
    fputs("->e1", fp);
}

static int
cg_writeln(FILE *fp, Vec<Var *> vars, int ln) {
  //fprintf(fp, "/* write/writeln */\n");
  for (int i = 2; i < vars.n; i++) {
    if (vars[i]->type == sym_int8 ||
        vars[i]->type == sym_int16 ||
        vars[i]->type == sym_int32)
      fprintf(fp, "printf(\"%%d\", %s);\n", vars[i]->cg_string);
    else if (vars[i]->type == sym_bool ||
             vars[i]->type == sym_uint8 ||
             vars[i]->type == sym_uint16 ||
             vars[i]->type == sym_uint32)
      fprintf(fp, "printf(\"%%u\", %s);\n", vars[i]->cg_string);
    else if (vars[i]->type == sym_int64)
      fprintf(fp, "printf(\"%%lld\", %s);\n", vars[i]->cg_string);
    else if (vars[i]->type == sym_uint64)
      fprintf(fp, "printf(\"%%llu\", %s);\n", vars[i]->cg_string);
    else if (vars[i]->type == sym_float32 ||
             vars[i]->type == sym_float64 ||
             vars[i]->type == sym_float128)
      fprintf(fp, "printf(\"%%g\", %s);\n", vars[i]->cg_string);
    else if (vars[i]->type == sym_string) {
      if (strcmp("\"\"", vars[i]->cg_string))
        fprintf(fp, "printf(\"%%s\", %s);\n", vars[i]->cg_string);
    } else
      fprintf(fp, "printf(\"<unsupported type>\");\n");
  }
  if (ln)
    fprintf(fp, "printf(\"\\n\");\n");
  return 0;
}

static cchar *
num_string(Sym *s) {
  switch (s->num_kind) {
    default: assert(!"case");
    case IF1_NUM_KIND_UINT:
      switch (s->num_index) {
        case IF1_INT_TYPE_1:  return "_CG_bool";
        case IF1_INT_TYPE_8:  return "_CG_uint8";
        case IF1_INT_TYPE_16: return "_CG_uint16";
        case IF1_INT_TYPE_32: return "_CG_uint32";
        case IF1_INT_TYPE_64: return "_CG_uint64";
        default: assert(!"case");
      }
      break;
    case IF1_NUM_KIND_INT:
      switch (s->num_index) {
        case IF1_INT_TYPE_1:  return "_CG_bool";
        case IF1_INT_TYPE_8:  return "_CG_int8";
        case IF1_INT_TYPE_16: return "_CG_int16";
        case IF1_INT_TYPE_32: return "_CG_int32";
        case IF1_INT_TYPE_64: return "_CG_int64";
        default: assert(!"case");
      }
      break;
    case IF1_NUM_KIND_FLOAT:
      switch (s->num_index) {
        case IF1_FLOAT_TYPE_32:  return "_CG_float32";
        case IF1_FLOAT_TYPE_64:  return "_CG_float64";
        case IF1_FLOAT_TYPE_128: return "_CG_float128";
        default: assert(!"case");
          break;
      }
      break;
  }
  return 0;
}

static cchar *
c_rhs(Var *v) {
  if (!v->sym->is_fun)
    return v->cg_string;
  else {
    char s[100];
    sprintf(s, "((_CG_function*)%s)", v->cg_string);
    return dupstr(s);
  }
}

static int
write_c_prim(FILE *fp, FA *fa, Fun *f, PNode *n) {
  switch (n->prim->index) {
    default: return 0;
    case P_prim_tuple: {
      cchar *t = c_type(n->lvals[0]);
      fprintf(fp, "%s = _CG_prim_tuple(%s);\n", n->lvals[0]->cg_string, t);
      for (int i = 1; i < n->rvals.n; i++)
        fprintf(fp, "%s->e%d = %s;\n", n->lvals[0]->cg_string, i-1, n->rvals.v[i]->cg_string);
      break;
    }
    case P_prim_vector: {
      if (n->rvals.n > 2) {
        int rank = 0;
        n->rvals[1]->sym->imm_int(&rank);
        fprintf(fp, "%s = (void*)%s /* prim_vector */ ;\n", n->lvals[0]->cg_string,
                n->rvals[2]->cg_string);
      } else {
        fprintf(fp, "%s = (void*)0; /* prim_vector */ ;\n", n->lvals[0]->cg_string);
      }
      break;
    }
    case P_prim_period: {
      cchar *t = c_type(n->lvals[0]);
      Vec<Sym *> symbols;
      symbol_info(n->rvals[3], symbols);
      assert(symbols.n == 1);
      cchar *symbol = symbols[0]->name;
      Sym *obj = n->rvals[1]->type;
      if (obj->type_kind == Type_LUB)
        obj = obj->has[0];
      if (n->lvals[0]->type->type_kind == Type_FUN && n->creates) {
        fprintf(fp, "%s = _CG_prim_closure(%s);\n", n->lvals[0]->cg_string, t);
        fprintf(fp, "%s->e%d = %s;\n", n->lvals[0]->cg_string, 0, n->rvals.v[3]->cg_string);
        fprintf(fp, "%s->e%d = %s;\n", n->lvals[0]->cg_string, 1, n->rvals.v[1]->cg_string);
      } else {
        for (int i = 0; i < obj->has.n; i++) {
          if (symbol == obj->has[i]->name) {
            assert(n->lvals[0]->cg_string);
            fprintf(fp, "%s = ((%s)%s)->e%d;\n", n->lvals[0]->cg_string, 
                    obj->cg_string, n->rvals[1]->cg_string, i);
            goto Lgetter_found;
          }
        }
        fail("getter not resolved");
      Lgetter_found:;
      }
      break;
    }
    case P_prim_setter: {
      Vec<Sym *> symbols;
      symbol_info(n->rvals[3], symbols);
      assert(symbols.n == 1);
      cchar *symbol = symbols[0]->name;
      Sym *obj = n->rvals[1]->type;
      if (obj->type_kind == Type_LUB)
        obj = obj->has[0];
      for (int i = 0; i < obj->has.n; i++) {
        if (symbol == obj->has[i]->name) {
          fprintf(fp, "((%s)%s)->e%d = %s;\n", 
                  obj->cg_string, n->rvals[1]->cg_string, i, c_rhs(n->rvals.v[4]));
          if (n->lvals[0]->live()) {
            assert(n->lvals[0]->cg_string);
            fprintf(fp, "%s = ((%s)%s)->e%d;\n", n->lvals[0]->cg_string, 
                    obj->cg_string, n->rvals[1]->cg_string, i);
          }
          goto Lsetter_found;
        }
      }
      fail("setter not resolved");
    Lsetter_found:
      break;
    }
    case P_prim_apply: {
      assert(0);
      int incomplete = 0; //n->lvals.n && n->lvals[0]->type->var && n->lvals.v[0]->type->var->def == n;
      if (incomplete) {
        fprintf(fp, "Fmake_apply(%s, ", n->lvals[0]->cg_string);
        write_c_type(fp, n->lvals[0]);
        fprintf(fp, ", %s, %s);\n", n->rvals[0]->cg_string, n->rvals.v[2]->cg_string);
      } else {
        if (n->lvals.n) {
          assert(n->lvals.n == 1);
          fprintf(fp, "%s = ", n->lvals[0]->cg_string);
        }
        int depth = application_depth(n);
        write_c_apply_arg(fp, n->rvals[0]->cg_string, depth, 0);
        fputs("((T__symbol) 0", fp);
        for (int i = 1; i < depth; i++) {
          fputs(", ", fp);
          write_c_apply_arg(fp, n->rvals[0]->cg_string, depth, i);
        }
        fputs(", ", fp);
        fputs(n->rvals[2]->cg_string, fp);
        fputs(");\n", fp);
      }
      break;
    }
    case P_prim_index_object: {
      assert(n->lvals.n == 1);
      int o = (n->rvals.v[0]->sym == sym_primitive) ? 1 : 0;
      Sym *t = n->rvals[1+o]->type, *e = t->element->type;
      e = e ? e : sym_void_type;
      fprintf(fp, "%s = ", n->lvals[0]->cg_string);
      fprintf(fp, "((list<%s>*)(%s))->__getitem__(", e->cg_string, n->rvals[1+o]->cg_string);
      for (int i = 2+o; i < n->rvals.n; i++) {
        if (i != 2+o) fputs(", ", fp);
        fprintf(fp, "%s-%d", n->rvals[i]->cg_string, fa->tuple_index_base);
      }
      fprintf(fp, ");\n");
      break;
    }
    case P_prim_set_index_object: {
      int o = (n->rvals.v[0]->sym == sym_primitive) ? 1 : 0;
      fprintf(fp, "((%s", n->lvals[0]->type->cg_string);
      for (int i = 2+o; i < n->rvals.n-1; i++) fprintf(fp, "*");
      fprintf(fp, ")(%s))", n->rvals[1]->cg_string);
      for (int i = 2+o; i < n->rvals.n-1; i++) 
        fprintf(fp, "[%s-%d]", n->rvals[i]->cg_string, fa->tuple_index_base);
      fprintf(fp, " = ");
      fprintf(fp, "%s", n->rvals[n->rvals.n-1]->cg_string);
      fprintf(fp, " /* prim_set_index */ ;\n");
      break;
    }
    case P_prim_new: {
      assert(n->lvals.n == 1);
      fprintf(fp, "%s = ", n->lvals[0]->cg_string);
      assert(n->rvals[n->rvals.n-1]->type->is_meta_type);
      fprintf(fp, "_CG_prim_new(%s);\n", n->lvals[0]->type->cg_string);
      break;
    }
    case P_prim_assign: {
      fprintf(fp, "%s = (%s)", n->lvals[0]->cg_string, num_string(n->lvals.v[0]->type));
      fprintf(fp, "%s;\n", n->rvals[3]->cg_string);
      break;
    }
    case P_prim_clone: {
      assert(n->lvals.n == 1);
      if (n->lvals[0]->cg_string)
        fprintf(fp, "%s = ", n->lvals[0]->cg_string);
      fprintf(fp, "(%s)_CG_prim_clone(", n->lvals[0]->type->cg_string);
      for (int i = 2; i < n->rvals.n; i++) {
        if (i > 2) fprintf(fp, ", ");
        fputs(n->rvals[i]->cg_string, fp);
      }
      fputs(");\n", fp);
      break;
    }
    case P_prim_primitive: {
      if (n->lvals.n) {
        assert(n->lvals.n == 1);
        if (n->lvals[0]->cg_string)
          fprintf(fp, "%s = ", n->lvals[0]->cg_string);
      }
      cchar *name = n->rvals[1]->sym->name;
      if (!name) 
        name = n->rvals[1]->sym->constant;
      if (!strcmp("write", name))
        cg_writeln(fp, n->rvals, 0);
      else if (!strcmp("writeln", name))
        cg_writeln(fp, n->rvals, 1);
      else if (!strcmp("print", name))
        cg_writeln(fp, n->rvals, 1);
      else {
        fprintf(fp, "_CG_%s_%s(", n->prim->name, name);
        for (int i = 2; i < n->rvals.n; i++) {
          if (i > 2) fprintf(fp, ", ");
          fputs(n->rvals[i]->cg_string, fp);
        }
        fputs(");\n", fp);
      }
      break;
    }
    case P_prim_list: {
      assert(n->lvals.n == 1);
      Sym *t = n->lvals.v[0]->type, *e = t->element->type;
      e = e ? e : sym_void_type;
      if (n->lvals[0]->cg_string)
        fprintf(fp, "%s = ", n->lvals[0]->cg_string);
      fprintf(fp, "(void*)new list<%s>(%d", e->cg_string, n->rvals.n-2);
      for (int i = 2; i < n->rvals.n; i++) {
        fprintf(fp, ", ");
        fputs(n->rvals[i]->cg_string, fp);
      }
      fputs(");\n", fp);
      break;
    }
  }
  return 1;
}

static Fun *
get_target_fun(PNode *n, Fun *f) {
  Vec<Fun *> *fns = f->calls.get(n);
  if (!fns || fns->n != 1)
   fail("unable to resolve to a single function at call site");
  return fns->v[0];
}

static void
write_c_fun_arg(FILE *fp, char *s, char *e, Sym *sym, int i) {
  if (i) fprintf(fp, ", ");
  if (!i && sym->type_kind == Type_FUN && !sym->fun && sym->has.n) {
    assert(0);
    for (int i = 0; i < sym->type->has.n; i++) {
      if (i) fprintf(fp, ", ");
      sprintf(e, "->e%d", i);
      write_c_fun_arg(fp, s, s + strlen(s), sym->has[i], i);
      fputs(s, fp);
    }
  } else {
    sprintf(e, "->e%d", i);
    fputs(s, fp);
  }
}

static void
simple_move(FILE *fp, Var *lhs, Var *rhs) {
  if (lhs->sym->type_kind || rhs->sym->type_kind)
    return;
  if (!lhs->cg_string || !rhs->cg_string)
    return;
  if (!rhs->sym->fun) {
    ASSERT(rhs->cg_string);
    fprintf(fp, "%s = (%s)%s;\n", lhs->cg_string, c_type(lhs), rhs->cg_string);
  } else if (rhs->cg_string)
    fprintf(fp, "%s = (_CG_fun)&%s;\n", lhs->cg_string, rhs->cg_string);
  else
    fprintf(fp, "%s = NULL;\n", lhs->cg_string);
}

static void
write_send(FILE *fp, FA *fa, Fun *f, PNode *n) {
  if (n->lvals.n) {
    assert(n->lvals.n == 1);
    if (n->lvals[0]->cg_string)
      fprintf(fp, "%s = ", n->lvals[0]->cg_string);
  }
  if (n->prim) {
    fprintf(fp, "_CG_%s(", n->prim->name);
    int comma = 0;
    for (int i = 1; i < n->rvals.n; i++) {
      if (n->rvals[i]->cg_string) {
        if (comma) fprintf(fp, ", ");
        comma = 1;
        fputs(n->rvals[i]->cg_string, fp);
      } else
        assert(0);
    }
    fputs(");\n", fp);
  } else {
    Fun *target = get_target_fun(n, f);
    fputs(target->cg_string, fp);
    fputs("(", fp);
    for (int i = 0; i < n->rvals.n; i++) {
      if (i) fprintf(fp, ", ");
      Sym *t = n->rvals[i]->type;
      if (!i && t->type_kind == Type_FUN && !t->fun && t->has.n) {
        char ss[4096];
        sprintf(ss, "%s", n->rvals[0]->cg_string);
        char *ee = ss + strlen(ss);
        for (int i = 0; i < t->has.n; i++)
          write_c_fun_arg(fp, ss, ee, t->has[i], i);
      } else if (t->fun)
        fprintf(fp, "((_CG_function)&%s)", n->rvals[i]->cg_string);
      else
        fputs(n->rvals[i]->cg_string, fp);
    }
    fputs(");\n", fp);
  }
}

static void
do_phy_nodes(FILE *fp, PNode *n, int isucc) {
  forv_PNode(p, n->phy)
    simple_move(fp, p->lvals[isucc], p->rvals.v[0]);
}

static void do_phi_nodes(FILE *fp, PNode *n, int isucc) {
  if (n->cfg_succ.n) {
    PNode *succ = n->cfg_succ[isucc];
    if (succ->phi.n) {
      int i = succ->cfg_pred_index.get(n);
      forv_PNode(pp, succ->phi)
        simple_move(fp, pp->lvals[0], pp->rvals.v[i]);
    }
  }
}

static void
write_c_pnode(FILE *fp, FA *fa, Fun *f, PNode *n, Vec<PNode *> &done) {
  if (n->code->kind == Code_LABEL)
    fprintf(fp, "L%d:;\n", n->code->label[0]->id);
  switch (n->code->kind) {
    case Code_LABEL: break;
    case Code_MOVE: 
      for (int i = 0; i < n->lvals.n; i++)
        simple_move(fp, n->lvals[i], n->rvals.v[i]); 
      break;
    case Code_SEND:
      if (n->prim)
        if (write_c_prim(fp, fa, f, n))
          break;
      write_send(fp, fa, f, n);
      break;
    case Code_IF:
    case Code_GOTO:
      break;
    default: assert(!"case");
  }
  switch (n->code->kind) {
    case Code_IF:
      fprintf(fp, "if (%s) {\n", n->rvals[0]->cg_string);
      do_phy_nodes(fp, n, 0);
      do_phi_nodes(fp, n, 0);
      fprintf(fp, "goto L%d;\n}\n", n->code->label[0]->id);
      fprintf(fp, "else {\n");
      do_phy_nodes(fp, n, 1);
      do_phi_nodes(fp, n, 1);
      fprintf(fp, "goto L%d;\n}\n", n->code->label[1]->id);
      break;
    case Code_GOTO:
      do_phi_nodes(fp, n, 0);
      fprintf(fp, "goto L%d;\n", n->code->label[0]->id);
      break;
    default:
      do_phi_nodes(fp, n, 0);
      break;
  }
  int extra_goto = n->cfg_succ.n == 1 && n->code->kind != Code_GOTO && n->code->kind != Code_LABEL;
  forv_PNode(p, n->cfg_succ)
    if (done.set_add(p)) {
      write_c_pnode(fp, fa, f, p, done);
      extra_goto = 0;
    }
  if (extra_goto) {
    assert(n->cfg_succ[0]->code->kind == Code_LABEL);
    fprintf(fp, "goto L%d;\n", n->cfg_succ[0]->code->label[0]->id);
  }
}

static void
write_arg_position(FILE *fp, MPosition *p) {
  for (int i = 0; i < p->pos.n; i++) {
    if (is_intPosition(p->pos[i])) {
      if (!i)
        fprintf(fp, "a%d", (int)Position2int(p->pos[i])-1);
      else
        fprintf(fp, "->e%d", (int)Position2int(p->pos[i])-1);
    }
  }
}

static void
write_c_args(FILE *fp, Fun *f) {
  forv_MPosition(p, f->positional_arg_positions) {
    Var *v = f->args.get(p);
    Sym *s = v->sym;
    if (v->cg_string && !s->is_symbol && !s->is_fun) {
      fprintf(fp, "%s = ", v->cg_string);
      write_arg_position(fp, p);
      fprintf(fp, ";\n");
    }
  }
}

static void
write_c(FILE *fp, FA *fa, Fun *f, Vec<Var *> *globals = 0) {
  fputs("\n", fp);
  write_c_fun_proto(fp, f);
  if (!f->entry) {
    fputs(" { }\n", fp);
    return;
  }
  fputs(" {\n", fp);
  int index = 0;
  Vec<Var *> vars, defs;
  f->collect_Vars(vars, 0, FUN_COLLECT_VARS_NO_TVALS);
  forv_Var(v, vars)
    if (v->sym->is_local)
      v->cg_string = 0;
  forv_Var(v, vars) if (!v->is_internal) {
    if (!v->cg_string && v->live() && !v->sym->is_symbol) {
      char s[100];
      sprintf(s, "t%d", index++);
      v->cg_string = dupstr(s);
      write_c_type(fp, v);
      fprintf(fp, " %s; (void)%s;\n", v->cg_string, v->cg_string);
    }
  }
  if (defs.n)
    fputs("\n", fp);
  if (globals)
    forv_Var(v, *globals)
      if (!v->sym->is_fun && v->sym->fun && !v->sym->type_kind && v->cg_string)
        fprintf(fp, "%s = %s;\n", v->cg_string, v->sym->fun->cg_string);
  write_c_args(fp, f);
  // rebuild cfg_pred_index
  forv_PNode(n, f->fa_all_PNodes) {
    n->cfg_pred_index.clear();
    for (int i = 0; i < n->cfg_pred.n; i++)
      n->cfg_pred_index.put(n->cfg_pred[i], i);
  }
  Vec<PNode *> done;
  done.set_add(f->entry);
  write_c_pnode(fp, fa, f, f->entry, done);
  fputs("}\n", fp);
}

static int
build_type_strings(FILE *fp, FA *fa, Vec<Var *> &globals) {
  // build builtin map
#define S(_n) if1_get_builtin(fa->pdb->if1, #_n)->cg_string = "_CG_" #_n;
#include "builtin_symbols.h"
#undef S
  // assign functions a C type string
  int f_index = 0;
  forv_Fun(f, fa->funs) {
    char s[100];
    if (f->sym->name)
      sprintf(s, "_CG_f_%d_%d/*%s*/", f->sym->id, f_index, f->sym->name);
    else
      sprintf(s, "_CG_f_%d_%d", f->sym->id, f_index);
    f->cg_string = dupstr(s);
    sprintf(s, "_CG_pf%d", f_index);
    f->cg_structural_string = dupstr(s);
    f->sym->cg_string = f->cg_structural_string;
    f_index++;
    if (f->sym->var)
      globals.set_add(f->sym->var);
  }
  // collect all syms
  Vec<Sym *> allsyms;
  forv_Fun(f, fa->funs) {
    Vec<Var *> vars;
    f->collect_Vars(vars);
    forv_Var(v, vars) {
      if ((!v->sym->is_local && v->sym->nesting_depth != f->sym->nesting_depth + 1) || v->sym->is_symbol || v->sym->is_fun)
        globals.set_add(v);
      allsyms.set_add(v->type);
    }
  }
  // collect type has syms
  int again = 1;
  while (again) {
    again = 0;
    Vec<Sym *> loopsyms;
    loopsyms.copy(allsyms);
    for (int i = 0; i < loopsyms.n; i++) 
      if (loopsyms[i] && !loopsyms.v[i]->type_kind) {
        forv_Sym(s, loopsyms[i]->has) {
          again = allsyms.set_add(s) || again;
        }
      }
  }
  globals.set_to_vec();
  // assign creation sets C type strings
  forv_Sym(s, allsyms) if (s) {
    if (s->num_kind)
      s->cg_string = num_string(s);
    else if (s->is_symbol) {
      s->cg_string = "_CG_symbol";
    } else {
      if (s->cg_string) {
        // skip
      } else {
        switch (s->type_kind) {
          default: 
            s->cg_string = dupstr("_CG_any");
            break;
          case Type_FUN:
            if (s->fun) break;
            // fall through
          case Type_RECORD: {
            if (s->has.n) {
              char ss[100];
              fprintf(fp, "struct _CG_s%d;\n", s->id);
              fprintf(fp, "typedef struct _CG_s%d *_CG_ps%d;\n", s->id, s->id);
              sprintf(ss, "_CG_ps%d", s->id);
              s->cg_string = dupstr(ss);
            } else
              s->cg_string = "_CG_void";
            break;
          }
        }
      }
    }
  }
  // resolve types
  forv_Sym(s, allsyms) if (s) {
    if (s->fun)
      s->cg_string = s->fun->cg_structural_string;
    else 
      if (s->is_symbol)
        s->cg_string = sym_symbol->cg_string;
  }
  // define function types and prototypes
  forv_Fun(f, fa->funs) {
    fprintf(fp, "typedef _CG_function %s;\n", f->cg_structural_string);
    write_c_fun_proto(fp, f);
    fputs(";\n", fp);
  }
  // define structs
  forv_Sym(s, allsyms) if (s) {
    switch (s->type_kind) {
      default: 
        break;
      case Type_FUN:
        if (s->fun) break;
        // fall through
      case Type_RECORD: {
        if (s->has.n) {
          fprintf(fp, "struct _CG_s%d {\n", s->id);
          for (int i = 0; i <  s->has.n; i++) {
            fputs("  ", fp);
            fputs(c_type(s->has[i]), fp);
            fprintf(fp, " e%d;\n", i);
          }
          fprintf(fp, "};\n");
        }
      }
    }
  }
  return 0;
}

void
c_codegen_print_c(FILE *fp, FA *fa, Fun *init) {
  Vec<Var *> globals;
  int index = 0;
  fprintf(fp, "#include \"c_runtime.h\"\n\n");
  if (build_type_strings(fp, fa, globals) < 0)
    fail("unable to generate C code: no unique typing");
  if (globals.n)
    fputs("\n", fp);
  forv_Var(v, globals)
    if (v->sym->is_fun)
      v->cg_string = v->sym->fun->cg_string;
  forv_Var(v, globals) {
    if (v->sym->imm.const_kind != IF1_NUM_KIND_NONE && v->sym->imm.const_kind != IF1_CONST_KIND_STRING) {
      char s[100];
      sprint_imm(s, v->sym->imm);
      v->cg_string = dupstr(s);
    } else if (v->sym->constant) {
      if (v->type == sym_string)
        v->cg_string = escape_string(v->sym->constant);
      else
        v->cg_string = v->sym->constant;
    } else if (v->sym->is_symbol) {
      char s[100];
      sprintf(s, "_CG_Symbol(%d, \"%s\")", v->sym->id, v->sym->name);
      v->cg_string = dupstr(s);
    } else if (v->sym->is_fun) {
    } else if (!v->sym->type_kind || v->sym->type_kind == Type_RECORD) {
      char s[100];
      if (v->sym->name)
        sprintf(s, "/* %s %d */ g%d", v->sym->name, v->sym->id, index++);
      else
        sprintf(s, "/* %d */ g%d", v->sym->id, index++);
      v->cg_string = dupstr(s);
      write_c_type(fp, v);
      fputs(" ", fp);
      fputs(v->cg_string, fp);
      fputs(";\n", fp);
    } else {
      index++;
      v->cg_string = dupstr(v->sym->name);
    }
  }
  forv_Fun(f, fa->funs) if (f != init && !f->is_external)
    write_c(fp, fa, f);
  write_c(fp, fa, init, &globals);
  fprintf(fp, "\nint main(int argc, char *argv[]) {"
          "(void)argc; (void) argv;\n"
          "%s(0);\n"
          "return 0;\n"
          "}\n", init->cg_string);
}

void
c_codegen_write_c(FA *fa, Fun *main, cchar *filename) {
  char fn[512];
  strcpy(fn, filename);
  strcat(fn, ".c");
  FILE *fp = fopen(fn, "w");
  c_codegen_print_c(fp, fa, main);
  fclose(fp);
}

int
c_codegen_compile(cchar *filename) {
  char target[512], s[1024];
  strcpy(target, filename);
  *strrchr(target, '.') = 0;
  sprintf(s, "make --no-print-directory -f %s/Makefile.cg CG_ROOT=%s CG_TARGET=%s CG_FILES=%s.c",
          system_dir, system_dir, target, filename);
  return system(s);
}
