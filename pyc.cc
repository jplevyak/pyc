// SPDX-License-Identifier: BSD-3-Clause
#define EXTERN
#include "defs.h"
#include "exc_check_fold.h"
#include "python_parse.h"
#include <signal.h>
#include <sys/resource.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>
#include <set>
#include <string>

extern int write_code_exit;

int do_unit_tests = 0;
int do_repl = 0;
static int dparse_only = 0;
static int dparse_ast = 0;
static int codegen_verify_each = 0;         // --verify-each
static char pyc_ifa_log[256];
// Backing storage for --strict / --permissive so both have a
// well-formed table entry (env-var support); the real work happens in
// the pfn callbacks below, which set runtime_errors and
// ifa_no_implicit_none together. See PIPELINE.md / README.md for what
// each mode means.
static bool pyc_strict_mode = false;
static bool pyc_permissive_mode = false;
static bool pyc_safe_mode = false;

// ---- pyc -x: compile once, cache, run ---------------------------------
//
// `#!/usr/bin/env -S pyc -x` as a script's first line makes it directly
// executable: `pyc -x script.py args...` compiles the script (if needed),
// caches the executable, and execs it with `args`. Everything after the
// script on the command line belongs to the program.
//
// The cache is $XDG_CACHE_HOME/pyc (else ~/.cache/pyc), one directory per
// script, `<name>-<hash>`. The hash covers the script's content and real
// path, the pyc binary's identity (size and mtime), and the pyc options
// given. The directory holds the executable and a manifest of every source
// file the compile parsed -- the script, its imports, pyc_lib modules and
// the builtin library -- each with its content hash. A run whose executable
// exists and whose manifest still matches execs it without parsing
// anything; otherwise pyc compiles into a temporary directory, writes the
// manifest, and renames it into place (so two concurrent first runs cannot
// leave a half-written entry), then execs.
static int exec_mode = 0;              // -x / --exec
static cchar *exec_out_fn = nullptr;   // codegen's output path in the cache
static std::string exec_tmp_dir;       // removed if the compile fails

static unsigned long long fnv64(const void *data, size_t n, unsigned long long h = 1469598103934665603ull) {
  const unsigned char *p = (const unsigned char *)data;
  for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

// Content hash of a file; false if it cannot be read.
static bool hash_file(cchar *path, unsigned long long *out) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return false;
  unsigned long long h = 1469598103934665603ull;
  char buf[65536];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) h = fnv64(buf, (size_t)n, h);
  close(fd);
  if (n < 0) return false;
  *out = h;
  return true;
}

static int rm_entry(const char *p, const struct stat *, int, struct FTW *) { return remove(p); }
static void rm_tree(const std::string &d) { if (!d.empty()) nftw(d.c_str(), rm_entry, 16, FTW_DEPTH | FTW_PHYS); }
static void exec_cleanup() { rm_tree(exec_tmp_dir); }

static bool mkdir_p(const std::string &d) {
  for (size_t i = 1; i <= d.size(); i++)
    if (i == d.size() || d[i] == '/') {
      std::string sub = d.substr(0, i);
      if (mkdir(sub.c_str(), 0755) < 0 && errno != EEXIST) return false;
    }
  return true;
}

// Does every "<hex hash> <path>" line of the manifest still hold?
static bool manifest_valid(const std::string &manifest) {
  FILE *fp = fopen(manifest.c_str(), "r");
  if (!fp) return false;
  char line[PATH_MAX + 64];
  bool ok = true, any = false;
  while (ok && fgets(line, sizeof(line), fp)) {
    unsigned long long want = 0, have = 0;
    char path[PATH_MAX + 1];
    if (sscanf(line, "%llx %4096[^\n]", &want, path) != 2) { ok = false; break; }
    any = true;
    if (!hash_file(path, &have) || have != want) ok = false;
  }
  fclose(fp);
  return ok && any;
}

static void exec_program(const std::string &exe, char **prog_argv) {
  fflush(stdout);
  fflush(stderr);
  execv(exe.c_str(), prog_argv);
  fprintf(stderr, "pyc -x: cannot execute '%s': %s\n", exe.c_str(), strerror(errno));
  exit(127);
}

// Where -x keeps `script`'s executable: fills *dir, *exe, *manifest.
static void exec_cache_paths(cchar *script, int argc_opts, char **argv, std::string *dir, std::string *exe,
                             std::string *manifest, std::string *name) {
  cchar *base = strrchr(script, '/');
  base = base ? base + 1 : script;
  std::string nm(base);
  if (nm.size() > 3 && nm.compare(nm.size() - 3, 3, ".py") == 0) nm.resize(nm.size() - 3);
  unsigned long long h = 0;
  if (!hash_file(script, &h)) {
    fprintf(stderr, "pyc -x: cannot read '%s'\n", script);
    exit(1);
  }
  char real[PATH_MAX];
  if (realpath(script, real)) h = fnv64(real, strlen(real), h);
  struct stat st;
  if (stat("/proc/self/exe", &st) == 0) {  // the compiler's identity
    h = fnv64(&st.st_size, sizeof(st.st_size), h);
    h = fnv64(&st.st_mtim, sizeof(st.st_mtim), h);
  }
  for (int i = 1; i < argc_opts; i++) h = fnv64(argv[i], strlen(argv[i]) + 1, h);  // the pyc options
  std::string root;
  if (cchar *x = getenv("XDG_CACHE_HOME"); x && *x) root = std::string(x) + "/pyc";
  else if (cchar *home = getenv("HOME"); home && *home) root = std::string(home) + "/.cache/pyc";
  else root = "/tmp/pyc-cache";
  char hex[17];
  snprintf(hex, sizeof(hex), "%016llx", h);
  *name = nm;
  *dir = root + "/" + nm + "-" + hex;
  *exe = *dir + "/" + nm;
  *manifest = *dir + "/sources";
}

static void help(ArgumentState *arg_state, char *arg_unused) {
  char ver[1000];
  get_version(ver);
  fprintf(stderr, "PYC Version %s ", ver);
  fprintf(stderr,
#include "COPYRIGHT.i"
  );
  usage(arg_state, arg_unused);
}

static void version(ArgumentState *arg_state, char *arg_unused) {
  char ver[30];
  get_version(ver);
  fprintf(stderr, "PYC Version %s ", ver);
  fprintf(stderr,
#include "COPYRIGHT.i"
  );
  exit(0);
}

static void license(ArgumentState *arg_state, char *arg_unused) {
  fprintf(stderr,
#include "LICENSE.i"
  );
  exit(0);
}

// --strict / --permissive: the one blessed toggle for Python's
// permissive-typing behavior. Bundles the two knobs that actually gate
// it: runtime_errors (defs.h) turns type violations from hard compile
// errors into warnings + inserted runtime checks; ifa_no_implicit_none
// (ifa/common/fail.h) controls whether a function whose fall-off path
// would otherwise need an implicit None union instead gets a
// shedskin-style typed default -- an accommodation, so it is off in BOTH
// modes and only PYC_NO_IMPLICIT_NONE (permissive only) turns it on
// (issues/171). The underlying ifa library itself
// defaults to strict (see ifa/if1/if1.cc); pyc's default is permissive
// for Python ergonomics -- these flags make that choice explicit and
// overridable in one step instead of two oddly-named ones.
// issues/171: --strict is CPython semantics or a compile error, never an
// accommodation. It used to turn ifa_no_implicit_none ON -- the
// shedskin-style typed default for a function that falls off the end,
// which returns e.g. False where CPython returns None. That is a semantic
// deviation, so it is now a permissive-only opt-in (PYC_NO_IMPLICIT_NONE,
// below) and strict keeps CPython's None, refusing a {T, None} union
// that has no representation.
static void strict_mode_arg(ArgumentState *arg_state, char *arg_unused) {
  runtime_errors = false;
  ifa_no_implicit_none = 0;
}

static void permissive_mode_arg(ArgumentState *arg_state, char *arg_unused) {
  runtime_errors = true;
  ifa_no_implicit_none = 0;
}

// --safe: the third environment of ifa/issues/039. Permissive about
// types, but no local is ever read holding garbage -- one the analysis
// reports as POSSIBLY used before assignment is zero-initialized
// instead of checked. (DEFINITELY used before assignment stays a
// compile error here, as in every mode: no path makes such a program
// right, so there is nothing to be safe about.)
static void safe_mode_arg(ArgumentState *arg_state, char *arg_unused) {
  runtime_errors = true;
  ifa_no_implicit_none = 0;
  auto_init_unbound = true;
}

static ArgumentDescription arg_desc[] = {
    {"repl", ' ', "Interactive REPL (requires -b; implies -b -j)", "F", &do_repl, "PYC_REPL", NULL},
    {"debug-info", 'g', "Produce Debugging Information", "F", &codegen_debug, "PYC_DEBUG_INFO", NULL},
    {"refuse-imprecise", ' ',
     "Refuse a program whose inference left a container element or parameter untyped, instead of guessing its layout "
     "(ifa/issues/124)",
     "F", &frefuse_imprecise, "PYC_REFUSE_IMPRECISE", NULL},
    {"optimize", 'O', "Optimize", "F", &codegen_optimize, "PYC_OPTIMIZE", NULL},
    {"no-optimize", ' ', "Compile the generated C without optimization (-O0, for debugging)", "f", &codegen_optimize,
     "PYC_NO_OPTIMIZE", NULL},
    {"output", 'o', "Output File", "S511", codegen_output, "PYC_OUTPUT", NULL},
    {"exec", 'x',
     "Compile (cached in ~/.cache/pyc by name and source hash) and run; for `#!/usr/bin/env -S pyc -x`. "
     "Arguments after the source file go to the program",
     "F", &exec_mode, "PYC_EXEC", NULL},
#ifdef USE_LLVM
    {"emit-llvm", 'b', "LLVM Codegen (the only LLVM backend — internally v2 via cg_normalize_v2 + cg_v2_emit_llvm_module)",
     "F", &codegen_llvm, "PYC_LLVM", NULL},
    {"jit", 'j', "JIT: compile with the LLVM backend and run in-process (implies -b)", "F", &codegen_jit, "PYC_JIT",
     NULL},
#endif
    {"strict", ' ', "Strict mode: hard compile errors on type violations, no permissive-Python fallbacks",
     "F", &pyc_strict_mode, "PYC_STRICT", strict_mode_arg},
    {"permissive", ' ', "Permissive mode (default): type violations warn + insert runtime checks, CPython-faithful implicit None",
     "F", &pyc_permissive_mode, "PYC_PERMISSIVE", permissive_mode_arg},
    {"safe", ' ', "Safe mode: permissive, plus zero-initialize locals that may be read before assignment",
     "F", &pyc_safe_mode, "PYC_SAFE", safe_mode_arg},
    {"html", ' ', "Output as HTML", "F", &fdump_html, "PYC_HTML", NULL},
    {"system-directory", 'D', "System Directory", "S511", system_dir, "PYC_SYSTEM_DIRECTORY", NULL},
    {"verbose", 'v', "Verbosity Level", "+", &verbose_level, "PYC_VERBOSE", NULL},
    {"debug", 'd', "Debugging Level", "+", &debug_level, "PYC_DEBUG", NULL},
    {"license", ' ', "Show License", NULL, NULL, NULL, license},
    {"version", ' ', "Version", NULL, NULL, NULL, version},
    {"help", 'h', "Help", NULL, NULL, NULL, help},

    {"", ' ', "-- Internal / development options (not a stable CLI contract) --", NULL, NULL, NULL, NULL},
#ifdef USE_LLVM
    {"verify-each", ' ', "Strict LLVM verifier: verify after every function, emit .ll on failure", "F",
     &codegen_verify_each, "PYC_VERIFY_EACH", NULL},
#endif
#ifdef DEBUG
    {"test", 't', "Unit Test", "F", &do_unit_tests, "PYC_TEST", NULL},
    {"test-scoping", ' ', "Test Scoping", "F", &test_scoping, "PYC_TEST_SCOPING", NULL},
#endif
    {"dparse-only", ' ', "Validate DParser parse only (no compilation)", "F", &dparse_only, "PYC_DPARSE_ONLY", NULL},
    {"dparse-ast", ' ', "Parse with DParser and print AST", "F", &dparse_ast, "PYC_DPARSE_AST", NULL},
    {"escape-in-fa", ' ', "Integrate escape analysis into IFA (Phase 1+, see ESCAPE_PLAN.md)", "F",
     &ifa_escape_in_fa, "IFA_ESCAPE_IN_FA", NULL},
    {"fa-inline", ' ', "Run simple_inlining between FA passes (0/1, default 1)", "I",
     &ifa_fa_inline, "IFA_FA_INLINE", NULL},
    {"narrow", ' ', "Enable issue-025 per-branch type narrowing recognizer (0/1, default 1)", "I",
     &ifa_narrow, "IFA_NARROW", NULL},
    {"dump-ir-after", ' ', "Write IF1 after pass N and exit; useful for bisecting", "I", &write_code_exit,
     "PYC_DUMP_IR_AFTER", NULL},
    {"log", 'l', "Debug Logging Flags", "S256", pyc_ifa_log, "PYC_LOG", log_flags_arg},
    {0}};

static ArgumentState arg_state("pyc", arg_desc);

static void init_system() {
  struct rlimit nfiles;
  assert(!getrlimit(RLIMIT_NOFILE, &nfiles));
#ifdef __APPLE__
  nfiles.rlim_cur = fmin(OPEN_MAX, nfiles.rlim_max);
#else
  nfiles.rlim_cur = nfiles.rlim_max;
#endif
  assert(!setrlimit(RLIMIT_NOFILE, &nfiles));
  assert(!getrlimit(RLIMIT_NOFILE, &nfiles));
}

void compile(cchar *fn) {
  if (ifa_analyze(fn) < 0) fail("program does not type");
  // issue 011: Fun::calls (built by clone(), inside ifa_analyze())
  // now exists -- compute the precise, post-FA can-raise bit before
  // any codegen consumes it.
  compute_fun_can_raise();
  // issue 011 (Tier 2 continued): fold each provably-safe exception
  // check's condition to FA's own canonical true_type constant --
  // both backends' existing Code_IF codegen already elides an
  // unreachable arm entirely for that exact Sym identity (the same
  // path ordinary FA-folded isinstance checks use), so this also
  // removes the dead exception-dispatch code the check used to guard,
  // not just the check's own cost. Must run after
  // compute_fun_can_raise() and before codegen.
  //
  // issue 050 Tier 3a (2026-07-18) added a SEPARATE, earlier fold --
  // IFACallbacks::provably_constant_isinstance, consulted natively by
  // FA's own P_prim_isinstance transfer function during
  // ifa_analyze() -- but Tier 2 stays, unconditionally, as more than
  // a fallback: mark_live_code (inside ifa_analyze) treats
  // constness and liveness as deliberately orthogonal (a
  // constant-folded SEND's own inputs can still be marked live, even
  // though codegen will separately elide the SEND's emission via
  // virtual_cg_is_const_folded_send) -- confirmed empirically by
  // disabling Tier 2 alone: Tier 3a's fold still removes the
  // check/branch on its own, but the __pyc_exc__ slot-read MOVE's
  // residual comes back. reclaim_dead_producer_chain's cleanup is
  // NOT redundant with native FA integration; it addresses a
  // different, general property of mark_live_code's design.
  mark_exc_checks_constant(fa);
  if (ifa_optimize() < 0) fail("unable to optimize program");
  ifa_dbg_bodies("after-optimize");  // ifa/issues/112
  { extern void sum_audit_check(cchar *); sum_audit_check("after-optimize"); }  // ifa/120
  { extern void compute_clone_equivalence(FA *); compute_clone_equivalence(pdb->fa); }  // ifa/121
  if (fgraph) ifa_graph(fn);
  if (fdump_html) {
    char mktree_dir[512];
    strcpy(mktree_dir, system_dir);
    ifa_html(fn, mktree_dir);
  }
  // pyc -x: codegen's .c/.ll/.o and the executable go in the cache entry
  // being built, not beside the source (which may not be writable).
  if (exec_out_fn) fn = exec_out_fn;
  if (fcg) {
#ifdef USE_LLVM
        if (codegen_llvm) {
      // The only LLVM path.  Despite the name `llvm.cc`, the
      // emission internally routes through cg_normalize_v2 +
      // cg_v2_emit_llvm_module (see llvm.cc:155 — old direct
      // emitter was retired in issue 014).
      llvm_codegen_write_ir(pdb->fa, if1->top->fun, fn);
      { extern void sum_audit_check(cchar *); sum_audit_check("after-cg"); }  // ifa/120
      if (codegen_jit) {
        if (llvm_jit_execute()) fail("JIT execution failed");
      } else {
        if (llvm_codegen_compile(fn)) fail("compilation failure");
      }
    } else
#endif
    {
      c_codegen_write_c(pdb->fa, if1->top->fun, fn);
      { extern void sum_audit_check(cchar *); sum_audit_check("after-cg"); }  // ifa/120
      if (c_codegen_compile(fn)) fail("compilation failure");
    }
  }
  return;
}

cchar *mod_name_from_filename(cchar *n) {
  cchar *start = strrchr(n, '/');
  if (!start)
    start = n;
  else
    start++;
  cchar *end = strrchr(n, '.');
  assert(end);
  return dupstr(start, end);
}

int main(int argc, char *argv[]) {
  // Raise the stack soft limit toward the hard limit before any deep
  // work. FA's matcher/dispatch (if1/pattern.cc, analysis/fa.cc) and
  // the recursive frontend lowering recurse proportionally to program
  // structure; a large machine-generated input (shedskin othello3:
  // 23k lines, hundreds of dispatch classes) overflows the default 8MB
  // stack, and the fault surfaces as a hard segfault deep inside the
  // GC's stack scrubber. On Linux the main-thread stack grows on
  // demand up to the current soft limit, so raising it here (best
  // effort; ignored if already unlimited or the hard cap is lower)
  // lets those inputs run to a normal outcome instead of crashing.
  {
    struct rlimit rl;
    if (!getrlimit(RLIMIT_STACK, &rl)) {
      rlim_t want = 1024ull * 1024 * 1024;  // 1 GiB
      rlim_t target = (rl.rlim_max == RLIM_INFINITY) ? want : (rl.rlim_max < want ? rl.rlim_max : want);
      if (rl.rlim_cur != RLIM_INFINITY && rl.rlim_cur < target) {
        rl.rlim_cur = target;
        (void)setrlimit(RLIMIT_STACK, &rl);
      }
    }
  }
  MEM_INIT();
  // The C backend optimizes by default, as the LLVM backend always has
  // (clang -O2): at -O0 the generated C ran 2-4x slower (brainfuck 100s vs
  // 26s, slower than CPython) and every C-vs-LLVM timing compared optimized
  // against unoptimized code. --no-optimize turns it off for debugging.
  codegen_optimize = 1;
  // pyc -x: everything after the script is the program's, so only the part
  // up to and including it is pyc's to parse. Found before process_args,
  // which would read the program's arguments as more inputs or options.
  int exec_script = -1;
  {
    bool saw_x = false;
    static cchar *takes_value[] = {"-o", "-D", "-l", "--output", "--system-directory", "--log", "--fa-inline",
                                   "--narrow", "--dump-ir-after", nullptr};
    for (int i = 1; i < argc; i++) {
      cchar *a = argv[i];
      if (!strcmp(a, "--")) { if (i + 1 < argc) exec_script = i + 1; break; }
      if (a[0] != '-' || !a[1]) { exec_script = i; break; }
      if (!strcmp(a, "--exec") || (a[1] != '-' && strchr(a + 1, 'x'))) saw_x = true;
      for (int k = 0; takes_value[k]; k++)
        if (!strcmp(a, takes_value[k])) { i++; break; }
    }
    if (!saw_x) exec_script = -1;
  }
  char **exec_argv = nullptr;
  char **pyc_argv = argv;
  if (exec_script > 0) {
    exec_argv = argv + exec_script;  // argv[script], its arguments, NULL
    // process_args reads argv up to its NULL, not up to argc: hand it a copy
    // that ends at the script.
    argc = exec_script + 1;
    pyc_argv = (char **)MALLOC(sizeof(char *) * (argc + 1));
    for (int i = 0; i < argc; i++) pyc_argv[i] = argv[i];
    pyc_argv[argc] = nullptr;
  }
  process_args(&arg_state, argc, pyc_argv);
  // The library directory (-D / PYC_SYSTEM_DIRECTORY) defaults to ".", which
  // only works from the source tree. Left at that default with no __pyc__
  // here, use the directory of the pyc executable itself (through any
  // symlink), so `pyc` on PATH -- and a `#!/usr/bin/env -S pyc -x` script run
  // from anywhere -- finds its library.
  if (!strcmp(system_dir, ".") && !is_directory("./__pyc__")) {
    char self[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
      self[n] = 0;
      if (char *slash = strrchr(self, '/')) {
        *slash = 0;
        std::string lib = std::string(self) + "/__pyc__";
        if (is_directory(lib.c_str()) && strlen(self) < sizeof(system_dir)) strcpy(system_dir, self);
      }
    }
  }
#ifdef USE_LLVM
  // The JIT runs only on the LLVM backend (compile() consults codegen_jit
  // inside the codegen_llvm branch), so `-j` alone used to be ignored
  // silently: the C backend built a binary and nothing ran. The REPL
  // already sets both.
  if (codegen_jit) codegen_llvm = 1;
#endif
  ifa_verbose = verbose_level;
  ifa_debug = debug_level;
  // Propagate the --verify-each flag down to the ifa lib's llvm.cc,
  // which reads PYC_VERIFY_EACH (the lib doesn't see pyc's defs.h
  // globals). Setting it from either the CLI flag or
  // PYC_VERIFY_EACH=1 is now equivalent.
  if (codegen_verify_each) setenv("PYC_VERIFY_EACH", "1", 1);
  if (do_repl) {
    init_system();
    init_config();
    if (pyc_ifa_log[0]) init_logs();
    Service::start_all();
    pyc_repl();
    Service::stop_all();
    exit(0);
  }
  if (arg_state.nfile_arguments < 1) usage(&arg_state, 0);
  std::string exec_dir, exec_exe, exec_manifest, exec_name;
  if (exec_mode) {
    if (arg_state.nfile_arguments != 1 || !exec_argv) {
      fprintf(stderr, "pyc -x: expected one source file, then the program's arguments\n");
      exit(2);
    }
    cchar *script = arg_state.file_argument[0];
    exec_cache_paths(script, exec_script, argv, &exec_dir, &exec_exe, &exec_manifest, &exec_name);
    if (access(exec_exe.c_str(), X_OK) == 0 && manifest_valid(exec_manifest)) exec_program(exec_exe, exec_argv);
    // Miss: build in a private temporary entry, renamed into place below.
    exec_tmp_dir = exec_dir + ".tmp." + std::to_string((long)getpid());
    rm_tree(exec_tmp_dir);
    if (!mkdir_p(exec_tmp_dir)) {
      fprintf(stderr, "pyc -x: cannot create '%s': %s\n", exec_tmp_dir.c_str(), strerror(errno));
      exit(1);
    }
    atexit(exec_cleanup);
    exec_out_fn = dupstr((exec_tmp_dir + "/" + exec_name + ".py").c_str());
    codegen_output[0] = 0;  // -o names the binary; -x decides where it goes
  }
  init_system();
  init_config();
  if (pyc_ifa_log[0]) init_logs();
  Service::start_all();
  if (do_unit_tests) {
    int r = UnitTest::run_all();
    Service::stop_all();
    _exit(r);
  }
  if (dparse_only) {
    int errors = 0;
    for (int i = 0; i < arg_state.nfile_arguments; i++)
      if (dparse_python_file(arg_state.file_argument[i]) < 0) errors++;
    Service::stop_all();
    exit(errors ? 1 : 0);
  }
  if (dparse_ast) {
    int errors = 0;
    for (int i = 0; i < arg_state.nfile_arguments; i++) {
      PyDAST *ast = dparse_python_to_ast(arg_state.file_argument[i]);
      if (!ast) errors++;
      else pyast_print(ast, 0);
    }
    Service::stop_all();
    exit(errors ? 1 : 0);
  }
  cchar *first_filename = 0;
  Vec<PycModule *> mods;
  int parse_errors = 0;
  for (int i = -1; i < arg_state.nfile_arguments; i++) {
    cchar *filename = 0;
    PyDAST *pymod = nullptr;
    if (i < 0) {
      char fn[256], fn2[256];
      strcpy(fn, system_dir);
      strcat(fn, "/__pyc__");
      if (is_directory(fn)) {
        // Load as one concatenated module; use the .py name for path resolution
        strcpy(fn2, system_dir);
        strcat(fn2, "/__pyc__.py");
        filename = dupstr(fn2);
        pymod = dparse_builtin_dir(fn);
      } else {
        strcat(fn, ".py");
        filename = dupstr(fn);
        pymod = dparse_python_to_ast(filename);
      }
    } else {
      filename = arg_state.file_argument[i];
      pymod = dparse_python_to_ast(filename);
    }
    if (!i) first_filename = filename;
    if (pymod) {
      PycModule *m = new PycModule(filename, i < 0);
      m->pymod = pymod;
      mods.add(m);
    } else
      parse_errors++;
  }
  // A file that does not parse used to just not be added to `mods`, and
  // with only the builtin module left the `mods.n > 1` guard below
  // skipped compilation entirely -- so pyc printed "dparse: parse error"
  // and then exited 0. Any script or harness reading the exit code saw a
  // successful build that produced no binary. (Found while delta-debugging
  // ifa/118: ast.unparse emits no trailing newline, pyc's grammar requires
  // one, and every candidate therefore "compiled clean".)
  if (parse_errors) {
    Service::stop_all();
    exit(1);
  }
  fruntime_errors = runtime_errors;
  fauto_init_unbound = auto_init_unbound;
  // ifa/issues/118: --strict bundles two knobs that are orthogonal.
  // runtime_errors turns type violations into warnings + inserted
  // checks; ifa_no_implicit_none gives a function whose fall-off path
  // would need an implicit None a shedskin-style typed default instead
  // (shedskin does exactly this -- it compiles the issue's 7-line repro
  // by returning False where CPython returns None). The second is what
  // clears the {bool, None} BOXING refusal, and wanting it does not
  // imply wanting the first. PYC_NO_IMPLICIT_NONE sets it on its own.
  //
  // issues/171: it is an ACCOMMODATION (a CPython deviation), so it is
  // honoured only in permissive mode and announces itself. Under --strict
  // the request is ignored with a warning rather than silently changing
  // what the program returns.
  if (cchar *v = getenv("PYC_NO_IMPLICIT_NONE")) {
    int want = atoi(v);
    if (want && !runtime_errors)
      fprintf(stderr, "warning: PYC_NO_IMPLICIT_NONE is a permissive-only accommodation; ignored under --strict\n");
    else
      ifa_no_implicit_none = want;
  }
  if (ifa_no_implicit_none)
    fprintf(stderr,
            "warning: PYC_NO_IMPLICIT_NONE: a function with a value 'return' that falls off the end returns a "
            "typed default, not None (a CPython deviation, issues/171)\n");
  if (mods.n > 1) {
    ast_to_if1(mods);
    compile(first_filename);
  }
  Service::stop_all();
  if (exec_mode) {
    // Manifest: every distinct source file parsed, with its content hash.
    std::string mpath = exec_tmp_dir + "/sources";
    FILE *fp = fopen(mpath.c_str(), "w");
    if (!fp) {
      fprintf(stderr, "pyc -x: cannot write '%s'\n", mpath.c_str());
      exit(1);
    }
    std::set<std::string> seen;
    for (cchar *f : pyc_source_files) {
      if (!f) continue;
      char real[PATH_MAX];
      std::string key = realpath(f, real) ? real : f;
      if (!seen.insert(key).second) continue;
      unsigned long long h = 0;
      if (hash_file(key.c_str(), &h)) fprintf(fp, "%016llx %s\n", h, key.c_str());
    }
    fclose(fp);
    // Into place. A stale entry (a source changed) is replaced; if another
    // run renamed its own entry in first, use that one.
    rm_tree(exec_dir);
    if (rename(exec_tmp_dir.c_str(), exec_dir.c_str()) == 0) exec_tmp_dir.clear();
    if (access(exec_exe.c_str(), X_OK) != 0) {
      fprintf(stderr, "pyc -x: no executable at '%s'\n", exec_exe.c_str());
      exit(1);
    }
    exec_program(exec_exe, exec_argv);
  }
  exit(0);
  return 0;
}
