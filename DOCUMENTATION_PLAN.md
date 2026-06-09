# Documentation Plan

A plan for filling out the documentation set so an LLM (or human) can navigate
the pyc + IFA codebase efficiently. Use the checkboxes to track progress.

---

## Design principles

These shape every decision below.

1. **One coherent unit per doc.** A doc covers one phase or one concept. Cross
   between docs by name+section pointer, never by re-explanation. (Exception:
   the top-level pipeline overview deliberately duplicates a few sentences from
   deep docs because those sentences answer "do I need to descend?")
2. **Self-contained reads.** Each deep doc should be readable cold without
   first reading another deep doc. Goal size: ~300-500 lines (one full Read
   call's worth of context). Existing `IFA.md` (~430 lines) and `CLONE.md`
   (~460 lines) hit this target.
3. **Two-tier hierarchy.** Tier 1 = thin indices + pipeline maps (cheap to
   maintain). Tier 2 = deep per-phase docs (high cost to write, then stable).
   Avoid a third tier — sub-sections of deep docs are already indexed by
   their own headings.
4. **Summaries only where the cost/benefit is positive.** A summary that
   duplicates content needs maintenance every time the underlying code
   moves. Add a summary only when:
   - The summarized fact is *stable* (e.g., "ast_to_if1 is the V-language
     handoff to IF1"), OR
   - The summary saves the LLM a Read on the *most common* task in that area.
   For volatile content, link instead of summarise.
5. **Symptom → file tables.** Each deep doc ends with a small table mapping
   common symptoms to start-here files+functions. Cheap, high value, low
   churn (symptoms outlast implementations).
6. **Cite line numbers sparingly.** Line numbers rot. Cite a function or
   class name; let the reader grep. Use line numbers only when pointing to
   a specific subtle hunk that has no good name.

---

## Current state

- [x] `CLAUDE.md` (root) — flat index, currently 3 lines
- [x] `README.md` (root) — user-facing intro
- [x] `ROADMAP.md` (root) — Python 3 feature roadmap
- [x] `ifa/CLAUDE.md` — `@AGENTS.md` redirect
- [x] `ifa/AGENTS.md` — build/test/architecture overview for ifa
- [x] `ifa/README.md` — build instructions for ifa
- [x] `ifa/IFA.md` — flow analysis reference
- [x] `ifa/CLONE.md` — cloning reference
- [x] `ifa/LLVM.md` — rewritten as a stub redirecting to CODEGEN_LLVM.md (the old IR-serialization content was stale and the referenced files don't exist)

---

## Tier 1 — indices & pipeline maps (write first, maintain forever)

These are the documents an LLM should read whenever it starts work in an
unfamiliar area. They are deliberately short so they fit in one Read and
their maintenance cost is bounded.

- [x] **`PIPELINE.md`** (root, ~150 lines) — the master compilation flow.
  Contents:
  - One-paragraph elevator pitch ("pyc is a whole-program type-inferring
    Python-to-C compiler built on the IFA library").
  - End-to-end flow diagram (text-only, one screen) from `main()` in `pyc.cc`
    through DParser → PyDAST → IF1 → FA → clone → CFG/SSU → optimize →
    codegen, with the entry function for each phase named and a one-line
    "what changes during this phase" annotation.
  - Per-phase pointer table: phase → driver file → deep doc.
  - "Where do I start?" table: common goals (add a Python feature, fix a
    codegen bug, change dispatch, etc.) → first doc to read.
  - **Maintenance:** updates only when the phase ordering itself changes,
    which is rare. The phase boundaries are encoded in `pyc.cc:compile`
    and `ifa/ifa.cc` — small surface area to keep in sync.

- [x] **`ifa/ARCHITECTURE.md`** (~120 lines) — bird's-eye view of the IFA
  library. Contents:
  - File-tree map of `ifa/` annotated by what each subdirectory owns
    (frontend, if1, analysis, optimize, codegen, common).
  - The `IFACallbacks` boundary: what the analysis demands from a frontend,
    what cloning demands, what codegen demands. Pointer to where each
    callback is implemented in pyc and in the V frontend.
  - `ifa_init` / `ifa_analyze` / `ifa_optimize` / `ifa_cg` call sequence
    from `ifa/ifa.cc`, with each function pointing into the deep doc that
    owns it.
  - Three or four cross-cutting invariants worth knowing before touching
    anything (e.g., "All Syms are owned by IF1 and interned; never new one
    directly — always via `new_Sym()`"; "PNode IDs are stable across passes;
    Sym IDs may change during cloning").
  - **Maintenance:** stable. The callback boundary and ifa.cc are about as
    foundational as anything gets.

- [ ] **Update `CLAUDE.md`** (root) — once each Tier-2 doc lands, add it to
  the index with a one-line hook. Keep the index alphabetical by topic, not
  chronological by addition.

---

## Tier 2 — deep per-phase / per-component docs

Sized like `IFA.md` / `CLONE.md`: paper synthesis (where applicable),
vocabulary, data structures, algorithm walk, gotchas, symptom table,
references. ~300-500 lines each.

### Python frontend (active feature area; highest churn → highest doc value)

- [x] **`PYTHON_FRONTEND.md`** (root) — the pyc-specific frontend.
  Scope:
  - `pyc.cc` — CLI / orchestration.
  - `python_parse.cc` + `python.g` — DParser grammar, `dparse_python_to_ast`,
    `dparse_builtin_dir`. PyDAST construction.
  - `python_ast.h` — `PyDAST`, `PyASTKind`, `PyOp`, `PyCtx`.
  - `python_ifa_int.h` — `PycCompiler`, `PycScope`, globals.
  - `python_ifa_util.cc` — `PycSymbol` / `PycAST` methods, string tables.
  - `python_ifa_sym.cc` — symbol creation, callbacks, `make_PycSymbol`.
  - `python_ifa_build_syms.cc` — symbol-table-building pass (`build_syms`,
    `gen_fun_pyda`, `gen_class_pyda`, `import_file`).
  - `python_ifa_build_if1.cc` — IF1 codegen pass (`build_if1_pyda`,
    `build_builtin_call_pyda`).
  - `python_ifa_main.cc` — `build_environment`, primitive registration,
    top-level `ast_to_if1`.
  - The two-pass build (symbols → IF1) and why it exists.
  - Scope sentinels (`GLOBAL_USE`/`NONLOCAL_USE`/`GLOBAL_DEF`/`NONLOCAL_DEF`)
    and the saved-scope machinery used for module imports.
  - PycCompiler.run() entry sequence; the OOP conversion (PycContext ↔
    PycCompiler ↔ PycCallbacks ↔ IFACallbacks); what each layer owns.
  - Hooks: which `IFACallbacks` virtuals are overridden where.

- [ ] **`RUNTIME.md`** (root, ~120 lines) — the pyc runtime layer.
  Scope:
  - `pyc_c_runtime.h` — generated C runtime helpers
    (boxing, type tags, GC integration).
  - `__pyc__/` directory (`00_runtime.py` … `06_bytearray.py`) — the
    builtin module; how concatenation works; ordering constraints (forward
    references).
  - `pyc_compat.py` — the `from pyc_compat import __pyc_declare__` shim;
    how to add a new directive.
  - `pyc_symbols.h` — the `S(...)` macro and what each builtin symbol means.
  - `defs.h` — common includes.

### IFA library — IR & supporting machinery

- [x] **`ifa/IR.md`** (~430 lines) — IF1 intermediate form. This one is
  load-bearing for *every* other deep doc.
  Scope:
  - `if1.cc` / `if1.h` — `IF1` struct (allsyms, allclosures, constants,
    primitives), the `if1_*` builders (`if1_send`, `if1_move`, `if1_if`,
    `if1_loop`, `if1_closure`, `if1_finalize`).
  - `sym.cc` / `sym.h` — `BasicSym` / `Sym`, `Type_kind`, the rich flag bag
    (`is_constant`, `is_fun`, `is_lvalue`, ...), `has` / `creators` /
    `implementors` / `specializers` / `specializes`, type aliases.
  - `code.h` — `Code` (rvals/lvals/labels/sub/ast), `Code_kind` enum,
    `Partial_kind`.
  - `pnode.cc` / `pnode.h` — `PNode`, CFG fields, `phi` / `phy`, temporaries.
  - `var.cc` / `var.h` — `Var` (per-use), `avars`.
  - `fun.cc` / `fun.h` — `Fun`, the `Fun(Sym*)` constructor that auto-builds
    CFG+SSU, `collect_Vars` / `collect_PNodes` / `collect_PNodes`, `fmap` /
    `nmap`, `calls` / `called`.
  - `num.cc` / `num.h` — `Immediate`, `IF1_num_kind`, the int/float/complex
    type tables, `imm_constant`, constant folding.
  - `ast.cc` / `ast.h` — low-level AST plumbing.
  - `builtin.cc` / `builtin.h` / `builtin_symbols.h` — global `sym_*`
    pointers and registration.
  - Lifecycle: who creates a Sym, when it's interned, when `live` flips,
    when it gets `type` assigned (during cloning).
  - The big invariant: pointer equality on interned strings, on canonicalized
    immediates, on canonicalized ATypes.

- [x] **`ifa/DISPATCH.md`** (~400 lines) — pattern matching & dispatch
  resolution. `pattern.cc` is 1568 lines of dense, opaque code that powers
  every method call; it deserves a dedicated doc.
  Scope:
  - `pattern.cc` / `pattern.h` — `Pattern`, `Match`, `MPosition`,
    `MPositionHashFuns`, `build_arg_positions`, `build_patterns`,
    `pattern_match`, `Match::merge`, `cannonicalize_mposition`.
  - How `MPosition` encodes positional + nested + keyword arguments,
    including the `int2Position` / `Position2int` sentinel encoding.
  - The shape of `Match::formal_filters` and how it interacts with
    `EntrySet::filters` (cross-ref to `IFA.md` §5.3).
  - `Pattern_kind` cases (NAME, TYPE, has-pattern, etc.).
  - Cache fields and what gets cached vs recomputed.
  - Symptom table: "method not found", "wrong overload picked",
    "ambiguous dispatch".

- [x] **`ifa/PRIMITIVES.md`** (~430 lines) — the primitive system.
  Scope:
  - `prim.cc` / `prim.h` / `prim_data.cc` / `prim_data.h` — `Prim`,
    `Primitives`, `RegisteredPrim`, `PrimType` enum.
  - The `prim_data.dat` file format and how it's compiled into
    `prim_data.cc`.
  - Transfer-function registration via `FA::register_primitive`.
  - How `Prim::find` maps a `PNode` send to a `Prim*`.
  - The relationship: declared `Prim` (in tables) vs registered
    `RegisteredPrim` (with a tfn) vs the `P_prim_*` indices used in
    `fa.cc`'s big switch.
  - The full list of `P_prim_*` constants with one-line semantics each
    (this is the part that earns its keep — saves grepping `prim_data.dat`).
  - Codegen counterpart: `PrimitiveCGPtr` and where it's invoked.

### IFA library — analysis pipeline

- [x] `ifa/IFA.md` — done.
- [x] `ifa/CLONE.md` — done.

- [x] **`ifa/CFG_SSU.md`** (~350 lines) — control flow, SSU form, dominators,
  loops. All four are tightly coupled and are produced as one block by
  `Fun::Fun(Sym*)` in `fun.cc:65-66`.
  Scope:
  - `cfg.cc` (102 lines) — `Fun::build_cfg`, `build_pn_cfg`,
    `resolve_labels`, `finalize_cfg`, `remove_unreachable`. The Code →
    PNode lowering with cfg_succ/cfg_pred wiring.
  - `ssu.cc` (205 lines) — `Fun::build_ssu`, phi/phy node insertion,
    `live_vars`, the rename pass.
  - `dom.cc` / `dom.h` — `Dom`, `build_dominators` (the algorithm — Cooper
    / Harvey / Kennedy?), `build_cfg_dominators`, `build_call_dominators`.
  - `loop.cc` / `loop.h` — `LoopGraph`, `LoopNode`, Sreedhar-Gao-Lee
    modification used here, the tree-of-loops structure.
  - Why SSU and not SSA — the symmetric "single static use" treatment of
    sources and sinks.
  - PNode → CFG invariants (e.g., `cont` chain, `pn` back-pointer).

- [x] **`ifa/OPTIMIZE.md`** (~340 lines) — post-analysis transforms.
  Scope:
  - `dead.cc` (318 lines) — `mark_live_code`, `mark_live_types`,
    `mark_live_funs`, the live-from-AVar reachability.
  - `inline.cc` (330 lines) — `simple_inlining`, `frequency_estimation`,
    `local_frequency_estimation`, `global_frequency_estimation`,
    `LOOP_FREQUENCY` constant.
  - Order: `ifa_optimize` does inline + retag live. Pointer to where
    `ifa.cc` orchestrates.
  - What can be safely added here (a new optimization pass) and what
    breaks if you reorder.
  - **Note on summary cost:** this doc summarizes the order/dependencies
    among optimization passes. That order is encoded in `ifa.cc:ifa_optimize`
    — 5 lines of code. The doc and the code can drift; keep the
    summary one short paragraph that quotes the actual function body.

### IFA library — V-language frontend (lower priority but useful)

- [x] **`ifa/FRONTEND.md`** (~440 lines) — the V-language frontend (legacy
  but still the IFA-tree's authoritative test bed).
  Scope:
  - `parse.cc` / `parse.h` / `parse_structs.h` — the dparser scaffolding,
    `FrontEnd` table, whitespace handlers.
  - `make_ast.cc` / `make_ast.h` — `ParseAST` construction.
  - `ast_to_if1.cc` (1876 lines) — the V → IF1 lowering. The big one.
    Will be the longest section of this doc.
  - `scope.cc` / `scope.h` — `Scope` chain, `get_dynamic`, `module()` walk.
  - `ast_kinds.h` — the `AST_*` enum.
  - `v.g` grammar references; how grammar nonterminals map to AST_* kinds.
  - **De-prioritize relative to pyc-side docs:** if time is short, do
    `PYTHON_FRONTEND.md` first since pyc is the active driver.

### IFA library — code generation

- [x] **`ifa/CODEGEN_C.md`** (~400 lines) — the C backend.
  Scope:
  - `cg.cc` (995 lines) — `c_codegen_write_c`, `c_codegen_compile`,
    per-PNode emission, `write_c_pnode`, `write_c_fun_proto`,
    type-string generation (`Sym::cg_string`), `rebuild_cfg_pred_index`.
  - The "LABEL extra_goto" bug noted in the project memory under "Critical
    IFA Code Generator Bug Fixed" — worth a callout so future maintainers
    don't reintroduce it.
  - How `Fun::cg_string` / `Sym::cg_string` get assigned and what they
    look like in emitted C.
  - The `Makefile.cg` interaction (system C compiler invocation).

- [x] **`ifa/CODEGEN_LLVM.md`** (~400 lines) — the LLVM backend.
  Scope:
  - `llvm.cc` (1589 lines) — top-level driver, `llvm_codegen`,
    `llvm_codegen_compile`, `llvm_codegen_initialize`, global state
    (`TheContext`, `TheModule`, `Builder`, `DBuilder`, `CU`, `UnitFile`).
  - `llvm_codegen.cc` (867) — per-Fun / per-PNode LLVM IR construction,
    `createFunction`, `translatePNode`, `getLLVMBasicBlock`,
    `getLLVMType`, `label_to_bb_map`.
  - `llvm_primitives.cc` (642) — per-primitive LLVM implementation.
  - `make_prims.cc` (175) — primitive table generator (build-time).
  - `llvm_internal.h` — globals & helpers.
  - The `IFA_LLVM=1` toggle; the JIT path.
  - **Decision:** keep `LLVM.md` separate (it's about the on-disk IR
    format) once it's updated — see the next item.

- [x] **Replace or rewrite `ifa/LLVM.md`** — currently documents
  `ir_serialize.cc` / `ir_deserialize.cc` and a `.ir` text format. Those
  files don't exist in the tree. Options:
  - (a) Delete it if the serialization layer is genuinely gone.
  - (b) Rewrite it to describe whatever serialization layer is current
    (if any).
  Decide which after a 10-minute grep for what produces/consumes `.ir`
  files today.

### IFA library — supporting subsystems

- [x] **`ifa/CAST.md`** (~360 lines) — type coercion & numeric promotion.
  Scope:
  - `cast_code.cc` (827 lines) — generated coercion code.
  - `make_cast_code.cc` (109) — the generator.
  - `check_cast.cc` (828) — coercion-table validator.
  - `num.cc` (931) — `Immediate`, the int/float/complex tables, conversion
    helpers. Already partly covered in `IR.md` — move the numeric-tables
    details here instead, and have `IR.md` just point.
  - `coerce_num` in `fa.cc` (the analysis-time C-style usual arithmetic
    conversions); cross-ref `IFA.md` §7.
  - The `prim_data.dat` ↔ `prim_data.cc` regeneration.

- [x] **`ifa/COMMON.md`** (~450 lines) — the plib-style utility layer.
  Scope:
  - `vec.cc` / `vec.h` — `Vec<T>`, `set_add`, `set_in`, `set_union`,
    `set_intersection`, `some_disjunction`, etc. (Used pervasively.)
  - `map.h` — `Map`, `ChainHash`, `HashMap`, `BlockHash`. Hash-cons
    patterns.
  - `arg.cc` / `arg.h` — `ArgumentState`, `ArgumentDescription`,
    `process_args`.
  - `config.cc` / `config.h` — `init_config`, `get_int_config`.
  - `log.cc` / `log.h` — `log()`, `LOG_*` channels, log-flag arg parsing.
  - `fail.cc` / `fail.h` — `fail()`, `assert` wrappers.
  - `html.cc` — HTML dumping.
  - `service.cc` — `Service::start_all` / `stop_all` lifecycle.
  - `unit.cc` — the `UnitTest` framework (and how to write a new one).
  - `timer.h` — `Timer`.
  - `defalloc.h` — GC allocator hookup.
  - `misc.cc` — `dupstr`, etc.
  - **Why bother with a "utilities" doc:** the LLM grovels through these
    constantly. One doc explaining the idioms (especially `Vec::set_*`
    sentinel semantics) saves a lot of re-discovery.

---

## Strategic summaries (in-doc, not new files)

Add these short summaries to *existing* files where the maintenance cost is
clearly positive. Each is two-to-five lines.

- [ ] **`pyc.cc`** top-of-file comment block — 4-line summary of CLI flags
  → phase ordering, pointing at `PIPELINE.md`. (`compile()` is the body;
  this just orients a reader.)
- [ ] **`ifa/ifa.cc`** top-of-file comment block — 4-line summary of the
  `ifa_*` exports and call order, pointing at `ifa/ARCHITECTURE.md`.
- [ ] **`ifa/analysis/fa.cc`** top-of-file comment block — 1 line pointing
  at `ifa/IFA.md`. (Already implicitly there via the codebase memory; make
  it explicit.)
- [ ] **`ifa/analysis/clone.cc`** top-of-file comment block — 1 line
  pointing at `ifa/CLONE.md`.
- [ ] **`ifa/if1/pattern.cc`** top-of-file comment block — 1 line pointing
  at `ifa/DISPATCH.md` (once written).
- [ ] **`python_ifa_int.h`** top-of-file comment block — short note about
  the OOP layering (`PycCompiler : PycCallbacks : IFACallbacks`) and a
  link to `PYTHON_FRONTEND.md`.

**Maintenance discipline:** an in-file pointer comment is allowed to say
"see X.md" but must NOT duplicate factual content from X.md. If you're
tempted to summarize the algorithm in the source, write a sentence about
*which* algorithm and link instead.

---

## Indices to update as each doc lands

- [ ] Update `CLAUDE.md` (root) when each new top-level doc lands.
- [ ] Update `ifa/CLAUDE.md` to point at `ARCHITECTURE.md` and any new
  ifa-side docs (currently it only `@AGENTS.md`s; consider adding a
  short index above the `@AGENTS.md` line).
- [ ] Once the Tier-2 docs exist, audit `ifa/AGENTS.md` for content that
  now duplicates Tier-2 docs; trim and point.

---

## Suggested ordering (highest value first)

If documentation work is rate-limited, do them in this order:

1. `PIPELINE.md` + `ifa/ARCHITECTURE.md` (the maps; immediately useful).
2. `ifa/IR.md` (almost every other doc references its concepts).
3. `PYTHON_FRONTEND.md` (active development area; biggest LLM payoff).
4. `ifa/DISPATCH.md` (`pattern.cc` is the most opaque area).
5. `ifa/CFG_SSU.md` (touched whenever CFG bugs happen).
6. `ifa/PRIMITIVES.md` (referenced by IFA.md and CODEGEN docs).
7. `ifa/OPTIMIZE.md` (small, finish for completeness).
8. `ifa/CODEGEN_C.md` (active backend).
9. `ifa/CODEGEN_LLVM.md` (less mature but documented separately).
10. `ifa/CAST.md` (numerics; useful but rarer entry point).
11. Update / replace / delete `ifa/LLVM.md`.
12. `ifa/FRONTEND.md` (V-language; lowest priority for the pyc work).
13. `ifa/COMMON.md` (utilities; high readership but stable, can land later).
14. `RUNTIME.md` (small, easy, finish-up).

---

## What we deliberately DON'T document (and why)

To save maintenance cost, the plan intentionally omits:

- Per-grammar-rule documentation of `python.g` / `v.g`. The grammars are
  self-documenting (a comment header per rule is enough) and they change
  whenever syntax does.
- Generated files (`*.d_parser.cc`, `prim_data.cc`). Document the *generator*
  (DParser usage, `make_cast_code`) instead.
- `tests/*.py` / `tests/*.v`. Tests are read directly; a doc would lag.
- The hand-rolled hash-function magic constants. Documenting them invites
  drift; the patterns are self-similar across `fa.cc` and obvious in context.
- Project memory / `MEMORY.md` content. That's the LLM's own scratchpad,
  not source documentation.
