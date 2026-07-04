import sys

with open("ifa/codegen/cg_view_emit_llvm.cc", "r") as f:
    content = f.read()

# 1. declare_globals
content = content.replace(
    '''      cchar *name = cg_get_string(v);
      llvm::GlobalVariable *gv = new llvm::GlobalVariable(
          *TheModule, t, /*isConstant=*/false,
          llvm::GlobalValue::InternalLinkage, init, name);''',
    '''      cchar *name = cg_get_string(v);
      if (!name) name = "";
      char buf[256];
      snprintf(buf, sizeof(buf), "global_%s_%d", name, v->id);
      llvm::GlobalVariable *gv = new llvm::GlobalVariable(
          *TheModule, t, /*isConstant=*/false,
          llvm::GlobalValue::InternalLinkage, init, buf);'''
)

# 2. value_for_var
content = content.replace(
    '''  if (llvm::Value *cached = ctx.var_map.get(v)) return cached;
  // Constant Sym: materialize the LLVM constant directly.
  Sym *s = v->sym;
  if (s && s->is_constant && v->type) {''',
    '''  if (llvm::Value *cached = ctx.var_map.get(v)) return cached;
  // Constant Sym: materialize the LLVM constant directly.
  Sym *s = get_constant(v);
  if (!s) s = v->sym;
  if (s && s->is_constant && v->type) {'''
)

# 3. Boolean constants
content = content.replace(
    '''      case IF1_NUM_KIND_UINT:
        if (t->isIntegerTy())
          cv = llvm::ConstantInt::get(t, (uint64_t)s->imm.v_int64,
                                      s->imm.const_kind == IF1_NUM_KIND_INT);
        break;
      case IF1_NUM_KIND_FLOAT:''',
    '''      case IF1_NUM_KIND_UINT:
        if (t->isIntegerTy())
          cv = llvm::ConstantInt::get(t, (uint64_t)s->imm.v_int64,
                                      s->imm.const_kind == IF1_NUM_KIND_INT);
        break;
      case IF1_NUM_KIND_BOOL:
        if (t->isIntegerTy())
          cv = llvm::ConstantInt::get(t, (uint64_t)s->imm.v_bool, false);
        break;
      case IF1_NUM_KIND_FLOAT:'''
)

# 4. __pyc_to_str__
content = content.replace(
    '''  Fun *target = callees->v[0];
  if (!target || !target->cg_string) return;
  llvm::Function *target_fn =
      TheModule->getFunction(target->cg_string);
  if (!target_fn) return;''',
    '''  Fun *target = callees->v[0];
  if (!target || !target->cg_string) return;
  llvm::Function *target_fn =
      TheModule->getFunction(target->cg_string);
  
  if (strcmp(target->cg_string, "__pyc_to_str__") == 0) {
    if (pn->rvals.n >= 3) {
      Var *v = pn->rvals.v[2];
      char buf[256];
      if (v && v->type && v->type->is_meta_type && v->type->name) {
        snprintf(buf, sizeof(buf), "<class '%s'>", v->type->name);
      } else {
        snprintf(buf, sizeof(buf), "<instance>");
      }
      llvm::Value *cv = materialize_pyc_string(dupstr(buf));
      if (pn->lvals.n > 0 && pn->lvals.v[0]) {
        put_result(ctx, pn->lvals.v[0], cv);
      }
      return;
    }
  }

  if (!target_fn) return;'''
)

# 5. Debug prints in emit_send
content = content.replace(
    '''void emit_send(EmitCtx &ctx, PNode *pn) {
  if (!pn) return;
  if (is_const_folded_send(pn)) return;
  if (pn->prim) {''',
    '''void emit_send(EmitCtx &ctx, PNode *pn) {
  if (!pn) return;
  if (pn->lvals.n > 0 && pn->lvals.v[0] && (pn->lvals.v[0]->id == 1563 || pn->lvals.v[0]->id == 1561)) {
    fprintf(stderr, "emit_send processing id %d!\\n", pn->lvals.v[0]->id);
  }
  if (is_const_folded_send(pn)) return;
  if (pn->prim) {'''
)

# 6. Debug prints in emit_send_primitive
content = content.replace(
    '''void emit_send_primitive(EmitCtx &ctx, PNode *pn) {
  if (!pn || !pn->prim) return;
  int idx = pn->prim->index;''',
    '''void emit_send_primitive(EmitCtx &ctx, PNode *pn) {
  if (!pn || !pn->prim) return;
  if (pn->lvals.n > 0 && pn->lvals.v[0] && pn->lvals.v[0]->id == 1561) {
    fprintf(stderr, "emit_send_primitive for id 1561, prim idx %d\\n", pn->prim->index);
  }
  int idx = pn->prim->index;'''
)

# 7. Debug prints in put_result
content = content.replace(
    '''void put_result(EmitCtx &ctx, Var *v, llvm::Value *value) {
  if (!v || !value) return;
  // Phi-target Var: store to its alloca slot so subsequent''',
    '''void put_result(EmitCtx &ctx, Var *v, llvm::Value *value) {
  if (!v || !value) return;
  if (v->id == 1563 || v->id == 1561) {
    fprintf(stderr, "put_result called for id %d!\\n", v->id);
  }
  // Phi-target Var: store to its alloca slot so subsequent'''
)

# 8. Debug prints in Code_IF
content = content.replace(
    '''      if (cond && t_bb && f_bb) Builder->CreateCondBr(cond, t_bb, f_bb);
      else Builder->CreateUnreachable();''',
    '''      if (cond && t_bb && f_bb) Builder->CreateCondBr(cond, t_bb, f_bb);
      else {
        int vid = -1;
        if (closer->rvals.n > 0 && closer->rvals.v[0]) {
          vid = closer->rvals.v[0]->id;
        }
        fprintf(stderr, "Code_IF unreachable! id=%d\\n", vid);
        Builder->CreateUnreachable();
      }'''
)

# 9. Debug prints in emit_send_call
content = content.replace(
    '''    llvm::Value *val = value_for_var(ctx, actual);
    if (!val) return;
    args.push_back(val);''',
    '''    llvm::Value *val = value_for_var(ctx, actual);
    if (!val) {
      fprintf(stderr, "early 5: actual %d not found\\n", actual->id);
      return;
    }
    args.push_back(val);'''
)

with open("ifa/codegen/cg_view_emit_llvm.cc", "w") as f:
    f.write(content)
