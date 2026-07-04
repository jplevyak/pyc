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

with open("ifa/codegen/cg_view_emit_llvm.cc", "w") as f:
    f.write(content)
