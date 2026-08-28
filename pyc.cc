// SPDX-License-Identifier: BSD-3-Clause
#define EXTERN
#include "defs.h"
#include "exc_check_fold.h"
#include "python_parse.h"
#include <signal.h>
#include <sys/resource.h>
#include <math.h>
#include <limits.h>

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
// shedskin-style typed default. The underlying ifa library itself
// defaults to strict (see ifa/if1/if1.cc); pyc's default is permissive
// for Python ergonomics -- these flags make that choice explicit and
// overridable in one step instead of two oddly-named ones.
static void strict_mode_arg(ArgumentState *arg_state, char *arg_unused) {
  runtime_errors = false;
  ifa_no_implicit_none = 1;
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
    {"optimize", 'O', "Optimize", "F", &codegen_optimize, "PYC_OPTIMIZE", NULL},
    {"output", 'o', "Output File", "S511", codegen_output, "PYC_OUTPUT", NULL},
#ifdef USE_LLVM
    {"emit-llvm", 'b', "LLVM Codegen (the only LLVM backend — internally v2 via cg_normalize_v2 + cg_v2_emit_llvm_module)",
     "F", &codegen_llvm, "PYC_LLVM", NULL},
    {"jit", 'j', "JIT", "F", &codegen_jit, "PYC_JIT", NULL},
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
    {"fa-inline", ' ', "Run simple_inlining between FA passes (0/1, default 0)", "I",
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
  if (fgraph) ifa_graph(fn);
  if (fdump_html) {
    char mktree_dir[512];
    strcpy(mktree_dir, system_dir);
    ifa_html(fn, mktree_dir);
  }
  if (fcg) {
#ifdef USE_LLVM
        if (codegen_llvm) {
      // The only LLVM path.  Despite the name `llvm.cc`, the
      // emission internally routes through cg_normalize_v2 +
      // cg_v2_emit_llvm_module (see llvm.cc:155 — old direct
      // emitter was retired in issue 014).
      llvm_codegen_write_ir(pdb->fa, if1->top->fun, fn);
      if (codegen_jit) {
        if (llvm_jit_execute()) fail("JIT execution failed");
      } else {
        if (llvm_codegen_compile(fn)) fail("compilation failure");
      }
    } else
#endif
    {
      c_codegen_write_c(pdb->fa, if1->top->fun, fn);
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
  process_args(&arg_state, argc, argv);
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
  if (cchar *v = getenv("PYC_NO_IMPLICIT_NONE")) ifa_no_implicit_none = atoi(v);
  if (mods.n > 1) {
    ast_to_if1(mods);
    compile(first_filename);
  }
  Service::stop_all();
  exit(0);
  return 0;
}
