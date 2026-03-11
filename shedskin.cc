// SPDX-License-Identifier: BSD-3-Clause
#include "defs.h"

int shedskin_codegen(FA *fa, Fun *fun, cchar *fn) {
  Vec<Sym *> modules;
  forv_Sym(s, if1->allsyms) if (s->is_module && !s->is_builtin) modules.add(s);
  forv_Sym(s, modules) printf("%s\n", s->name);
  return 0;
}
