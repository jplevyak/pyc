# pyc Backend Development Hints

When modifying or refactoring code generation (CG) in `pyc`, keep these points in mind to save time:

1. **Testing the LLVM vs C Backends:**
   - `./pyc <file.py>` (default C backend) generates `<file.py>.c` **and compiles it all
     the way to a native binary automatically** (via `Makefile.cg` → `clang++`) — no
     separate compile step needed. Run the produced binary directly.
   - `./pyc -b <file.py>` / `./pyc --emit-llvm <file.py>` triggers the LLVM backend,
     which likewise compiles and links to a native binary automatically.
   - `-o <path>` / `--output <path>` picks the output binary's path/name on either
     backend (default: the input filename with its extension stripped).
   - Manual `clang++` invocation is only useful for one specific case: iterating on a
     hand-edited `<file.py>.c` / `.ll` *without* rerunning the pyc frontend (e.g. a
     codegen experiment). For that, `clang++` (not `clang`) is required, because
     `pyc_c_runtime.h` relies on C++ implicit `struct` typedefs. Example:
     `clang++ -std=c++23 -I. -I/usr/local/include -I/opt/homebrew/include tests/file.py.c ifa/libifa_gc.a -lgc -o out`

2. **Handling `P_prim_reply` (Returns):**
   - The unified `virtual_cg_emit_send` dispatcher intentionally **skips** `P_prim_reply`. 
   - LLVM handles returns entirely inside `emit_block_terminator`.
   - If you write a new backend (like the C backend), you must manually emit the return statement either in your block termination logic or explicitly alongside the `Code_SEND` handler, otherwise `return` statements will be silently dropped.

3. **`is_const_folded_send` Elision:**
   - Any SEND operation where the lvalue is completely constant-folded must be skipped before emission.
   - Without this check, backends will attempt to assign to literals (e.g., generating `5 = _CG_prim_make()`), breaking the build.
   - Always check `virtual_cg_is_const_folded_send(pn)` before emitting SENDs.

4. **LLVM Block Terminators (`Code_IF` / `Code_GOTO`):**
   - LLVM basic blocks strictly require *exactly one* terminator.
   - When emitting branches for `Code_IF`, always check `if (Builder->GetInsertBlock()->getTerminator())` to avoid crashing `verifyModule` with double-terminated blocks, particularly in multi-predecessor loops.

5. **Never commit build artifacts:**
   - Don't `git add` compiled binaries (`ifa/ifa`, `ifa/ifa-test`, compiled test binaries under `tests/`), object files, generated LLVM IR (`.ll`), or debug-info bundles (`.dSYM/`). All of these are Makefile/test-harness output and get rebuilt on demand.
   - `.gitignore` ignores everything extensionless directly under `tests/` and `ifa/tests/` (except the `empty` fixture and the `ir/`/`synthetic/` fixture directories) plus `*.dSYM/` and `*.ll` globally — if a new test's compiled output isn't being ignored, fix the `.gitignore` pattern rather than committing the binary.
