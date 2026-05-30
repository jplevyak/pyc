# PIPELINE — pyc Compilation Flow

A map of how `pyc <file.py>` becomes `<file>.c` (or LLVM `.o`). This is the
"start here" doc — pick the phase you care about, then descend into the deep
doc for that phase.

For the doc layout itself, see [DOCUMENTATION_PLAN.md](DOCUMENTATION_PLAN.md).

---

## In one paragraph

**pyc** is a whole-program, type-inferring Python-to-C (or -LLVM) compiler
built on the **IFA** library. The front end (`pyc.cc` + `python_*.cc`)
parses Python with a DParser-generated grammar into a `PyDAST` tree, then
lowers it into IFA's intermediate form (**IF1**) — first a symbol-table
pass, then an IF1-code pass. From there the IFA library takes over:
construct one `Fun` per closure (which builds CFG + SSU on the fly), run
**iterative flow analysis** to infer all types, **clone** functions and
types for monomorphisation, build call-graph dominators, mark dead code,
estimate execution frequency, do a round of inlining, and finally hand
to one of two backends: a C-emitter (`cg.cc`) that produces a `.c` file
which is then compiled by the system C compiler, or an LLVM backend that
produces `.o`/`.ll` directly.

---

## Top-level flow diagram

```
pyc <file.py>
│
├── main()                                                   pyc.cc:138
│   ├── process_args / init_system / init_config
│   ├── (optional)  --dparse_only       → dparse_python_file        ─── parse-only validation
│   ├── (optional)  --dparse_ast        → dparse_python_to_ast + pyast_print
│   │
│   ├── For each input file (and the builtin __pyc__ first):
│   │     pymod = dparse_python_to_ast(filename)                    [python_parse.cc + python.g]
│   │     mods += new PycModule(filename, is_builtin)
│   │
│   ├── ast_to_if1(mods)                                            [python_ifa_main.cc]
│   │   ├── ifa_init(PycCompiler)            → IF1 + PDB + ast init [ifa/ifa.cc:17]
│   │   ├── build_builtin_symbols / register transfer-fns
│   │   ├── build_environment(mods[0])        → scope_sym for every builtin
│   │   ├── build_search_path                 → PYTHONPATH walk
│   │   ├── build_syms(each mod)              → symbol-table pass   [python_ifa_build_syms.cc]
│   │   ├── finalize_types(if1)
│   │   ├── build_module_attributes_if1 +
│   │   │   build_if1_module_pyda(each mod)   → IF1-code pass       [python_ifa_build_if1.cc]
│   │   ├── build_init(code)                  → wrap __main__
│   │   ├── build_type_hierarchy
│   │   └── fixup_aspect                      → resolve super dispatch
│   │
│   └── compile(file)                                                pyc.cc:81
│       ├── ifa_analyze(file)                                       [ifa/ifa.cc:23]
│       │   ├── if1_finalize(if1)             → close types         [ifa/if1/if1.cc]
│       │   ├── for each closure in if1->allclosures:
│       │   │     new Fun(closure)            → build_cfg+build_ssu [ifa/optimize/cfg.cc, ssu.cc]
│       │   │     pdb->add(f)
│       │   ├── FA::analyze(top)              → iterative flow      [ifa/analysis/fa.cc]   ← IFA.md
│       │   ├── clone(fa)                     → monomorphise        [ifa/analysis/clone.cc] ← CLONE.md
│       │   ├── build_cfg_dominators(f)       → per-Fun dominators  [ifa/optimize/dom.cc]
│       │   ├── mark_live_code(fa)            → DCE                 [ifa/optimize/dead.cc]
│       │   └── frequency_estimation(fa)      → static freq         [ifa/optimize/inline.cc]
│       │
│       ├── ifa_optimize()                                           [ifa/ifa.cc:53]
│       │   ├── mark_live_funs(fa)
│       │   ├── simple_inlining(fa)           → inline hot calls    [ifa/optimize/inline.cc]
│       │   ├── mark_live_types(fa)
│       │   └── mark_live_funs(fa)
│       │
│       ├── (optional)  fgraph        → ifa_graph(fn)               [ifa/analysis/graph.cc]
│       ├── (optional)  fdump_html    → ifa_html(fn)                [ifa/common/html.cc]
│       │
│       └── backend:
│           ├── (default)   c_codegen_write_c → c_codegen_compile  [ifa/codegen/cg.cc]
│           ├── --llvm      llvm_codegen      → llvm_codegen_compile [ifa/codegen/llvm*.cc]
│           └── --ss        shedskin_codegen                       [shedskin.cc — vestigial]
```

Phase ordering is encoded in two places:
- `pyc.cc:compile` — backend selection.
- `ifa/ifa.cc` — `ifa_analyze` then `ifa_optimize` then the backend exports
  (`ifa_cg`, `ifa_compile`, `llvm_codegen`).

When the ordering changes, this doc and `ifa/ARCHITECTURE.md` are the two
files to update.

---

## What changes during each phase

| Phase | Input | Output | Key invariant established |
|---|---|---|---|
| Parse | `.py` source | `PyDAST` tree | Lexical + syntactic structure only; no symbols yet. |
| `build_syms` | `PyDAST` + builtin syms | `Sym`s for every binding | Every `def`/`class`/local has a `Sym`; scopes are populated; imports are resolved by walking saved scopes. |
| `build_if1` | scoped `PyDAST` | `Code` trees attached to `Sym`s | Each function/closure has an IF1 `Code` body of `SEND`/`MOVE`/`IF`/`LOOP` etc. Types are not yet inferred. |
| `if1_finalize` + `new Fun` | IF1 closures | `Fun` objects with PNodes | One `PNode` per `Code`; CFG (`cfg_succ`/`cfg_pred`) wired; SSU phi/phy inserted; live-vars known per node. |
| `FA::analyze` | `PDB` of `Fun`s | per-`AVar` `AType`s, `EntrySet`s, `CreationSet`s, `AEdge`s, `type_violations` | Every reachable value has an inferred type-set; call edges resolved; type-violation imprecisions split until fixed-point. |
| `clone` | analysed graph | concrete `Sym`s (`cs->type`, `av->type`) + new `Fun` clones | Every contour has a concrete type; `Fun::calls` / `Fun::called` reflect the cloned call graph. |
| `build_cfg_dominators` | cloned `Fun`s | `Dom` trees | Dominator info available for SSA-style transforms. |
| `mark_live_code` | analysed types | `Var::live`, `PNode::live` bits | Dead nodes/vars flagged. |
| `frequency_estimation` | loop structure | `PNode::execution_frequency`, `Fun::execution_frequency` | Static estimate (10× per loop level). |
| `simple_inlining` | freq + live | smaller call graph | Hot statically-bound calls inlined. |
| `c_codegen` / `llvm_codegen` | cloned + optimised `Fun`s | `.c` / `.o` / `.ll` | Concrete-typed code in the chosen target. |

---

## Data structures by phase

This is the cheat-sheet for "what type holds the truth at this point in
the pipeline":

```
PyDAST            ← parse, build_syms reads, build_if1 reads
Sym               ← build_syms creates, FA reads/extends, clone rewrites (cs->type→s->type)
Code              ← build_if1 creates, CFG lowers to PNode
PNode             ← Fun::build_cfg creates, FA propagates types over
Var               ← build_if1 creates (per Sym usage), FA per-contour AVars
AVar              ← FA only (Var × contour); becomes Var->type after clone
AType             ← FA only (lattice element)
CreationSet       ← FA creates, clone uses to mint concrete types
EntrySet          ← FA creates (one per Fun specialisation), clone collapses
AEdge             ← FA call edges; clone uses for call graph
Fun               ← built once per closure; cloned during clone
Match / MPosition ← pattern matching (dispatch resolution)  [see ifa/DISPATCH.md when written]
```

For details on which fields are populated when, see the per-phase deep doc.

---

## Where do I start?

Pick the goal closest to your task. Each row points at the doc you should
read first; if the doc doesn't exist yet, the "fallback" column tells you
which sources to read instead.

| If you want to… | Read first | Fallback (until that doc exists) |
|---|---|---|
| Add a Python language feature | `PYTHON_FRONTEND.md` | `python_ifa_build_syms.cc`, `python_ifa_build_if1.cc`, `python.g`, `ROADMAP.md` |
| Fix a parser/AST issue | `PYTHON_FRONTEND.md` | `python.g`, `python_parse.cc`, `python_ast.h` |
| Change how types flow | [`ifa/IFA.md`](ifa/IFA.md) | — |
| Fix wrong dispatch / overload | `ifa/DISPATCH.md` | `ifa/if1/pattern.cc`, `ifa/IFA.md` §5.5 |
| Fix a cloning bug | [`ifa/CLONE.md`](ifa/CLONE.md) | — |
| Add a new primitive | `ifa/PRIMITIVES.md` | `ifa/if1/prim_data.cc`, `python_ifa_main.cc` `add_primitive_transfer_functions` |
| Fix a CFG/SSU/dom/loop bug | `ifa/CFG_SSU.md` | `ifa/optimize/{cfg,ssu,dom,loop}.cc` |
| Change/add an optimisation pass | `ifa/OPTIMIZE.md` | `ifa/optimize/{dead,inline}.cc`, `ifa/ifa.cc:ifa_optimize` |
| Fix wrong C output | `ifa/CODEGEN_C.md` | `ifa/codegen/cg.cc` |
| Work on the LLVM backend | `ifa/CODEGEN_LLVM.md` | `ifa/codegen/llvm*.cc` |
| Add a runtime helper | `RUNTIME.md` | `pyc_c_runtime.h`, `__pyc__/*.py` |
| Understand the IF1 IR | `ifa/IR.md` | `ifa/if1/{if1,sym,code,pnode,fun,var,num}.{cc,h}` |
| Understand the architecture | `ifa/ARCHITECTURE.md` | `ifa/ifa.cc`, `ifa/ifa.h`, `ifa/AGENTS.md` |

---

## Build / run cheat-sheet

```bash
make                       # build pyc binary (root Makefile)
make -C ifa                # rebuild ifa library only

./pyc hello_world.py       # default C backend → hello_world.c + hello_world binary
./pyc -O hello_world.py    # with optimisation
./pyc -b hello_world.py    # LLVM backend (USE_LLVM build)
./pyc --dparse_only x.py   # parse-validation only
./pyc --dparse_ast x.py    # parse + print AST

./test_pyc                 # full test suite (see tests/)
```

Useful environment / flag knobs:
- `IFA_LOG_FLAGS=splitting` (or `-l splitting`) — turn on the splitter log
  to debug analysis convergence (see `ifa/IFA.md` §6).
- `--write_code_exit N` — dump IF1 after pass N and stop. Useful for bisecting.
- `PYTHONPATH` — searched by `build_search_path` for imports.
- `IFA_SYSTEM_DIRECTORY` / `PYC_SYSTEM_DIRECTORY` — where `__pyc__/` lives.

---

## Things that surprise newcomers

A few one-liner gotchas worth knowing before reading any deep doc:

- **`new Fun(closure)` builds the CFG and SSU.** It looks like a constructor
  but does serious work (`fun.cc:65-66`). If you `new Fun` outside the
  intended flow, you will silently re-build a CFG.
- **There are two Plib `Vec` mental models.** `Vec` is sometimes a dense
  array, sometimes a sparse set (with `set_add`/`set_in`/`set_to_vec`).
  Many fields use the set mode without comment; check the call site.
- **`Sym` pointer identity is the type identity.** Strings, Immediates,
  and ATypes are also interned. Pointer equality means semantic equality
  almost everywhere. Don't construct a new `Sym` when you mean to look one
  up.
- **`PNode` IDs are stable across passes; `Sym` IDs may change during
  cloning.** If you cache anything by ID, prefer PNode IDs.
- **Imports are inlined at compile time.** `import foo` runs `build_syms`
  on `foo.py` with a saved/restored scope stack. There is no runtime
  import.
- **The CDB persistence layer is dormant.** Mentions of "compilation
  database" in code/docs refer to a planned cache (`ifa/analysis/cdb.cc`)
  that currently no-ops. Don't rely on it.

---

## Map of existing documentation

| Doc | Scope |
|---|---|
| `CLAUDE.md` (root) | Document index |
| `PIPELINE.md` (this) | Top-level compilation flow |
| `README.md` | User-facing intro |
| `ROADMAP.md` | Python 3 feature plan |
| `DOCUMENTATION_PLAN.md` | What docs to write next |
| `ifa/AGENTS.md` | IFA build/test/architecture for agents |
| `ifa/CLAUDE.md` | `@AGENTS.md` redirect |
| [`ifa/IFA.md`](ifa/IFA.md) | Flow analysis (algorithm + code) |
| [`ifa/CLONE.md`](ifa/CLONE.md) | Cloning (algorithm + code) |
| `ifa/LLVM.md` | (Stub redirecting to `CODEGEN_LLVM.md`; old IR-serialization content was stale.) |

Future docs are listed in `DOCUMENTATION_PLAN.md`.
