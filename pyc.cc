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
static int codegen_strict_verify = 0;       // --strict-verify
static char pyc_ifa_log[256];

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

static ArgumentDescription arg_desc[] = {
    {"repl", ' ', "Interactive REPL (requires -b; implies -b -j)", "F", &do_repl, "PYC_REPL", NULL},
    {"debug_info", 'g', "Produce Debugging Information", "F", &codegen_debug, "PYC_DEBUG_INFO", NULL},
    {"optimize", 'O', "Optimize", "F", &codegen_optimize, "PYC_OPTIMIZE", NULL},
#ifdef USE_LLVM
    {"llvm", 'b', "LLVM Codegen (the only LLVM backend — internally v2 via cg_normalize_v2 + cg_v2_emit_llvm_module)",
     "F", &codegen_llvm, "PYC_LLVM", NULL},
    {"jit", 'j', "JIT", "F", &codegen_jit, "PYC_JIT", NULL},
    {"strict_verify", ' ', "Strict LLVM verifier (per-fn + emit .ll on failure)", "F",
     &codegen_strict_verify, "PYC_STRICT_VERIFY", NULL},
#endif
#ifdef DEBUG
    {"test", 't', "Unit Test", "F", &do_unit_tests, "PYC_TEST", NULL},
    {"test_scoping", ' ', "Test Scoping", "F", &test_scoping, "PYC_TEST_SCOPING", NULL},
#endif
    {"dparse_only", ' ', "Validate DParser parse only (no compilation)", "F", &dparse_only, "PYC_DPARSE_ONLY", NULL},
    {"dparse_ast", ' ', "Parse with DParser and print AST", "F", &dparse_ast, "PYC_DPARSE_AST", NULL},
#ifdef USE_SS
    {"ss", 's', "Shedskin Codegen", "F", &codegen_shedskin, "PYC_SS", NULL},
#endif
    {"escape_in_fa", ' ', "Integrate escape analysis into IFA (Phase 1+, see ESCAPE_PLAN.md)", "F",
     &ifa_escape_in_fa, "IFA_ESCAPE_IN_FA", NULL},
    {"fa_inline", ' ', "Run simple_inlining between FA passes (0/1, default 0)", "I",
     &ifa_fa_inline, "IFA_FA_INLINE", NULL},
    {"narrow", ' ', "Enable issue-025 per-branch type narrowing recognizer (0/1, default 1)", "I",
     &ifa_narrow, "IFA_NARROW", NULL},
    {"runtime_errors", 'r', "Use runtime type checks", "f", &runtime_errors, "PYC_RUNTIME_ERRORS", NULL},
    {"html", ' ', "Output as HTML", "F", &fdump_html, "PYC_HTML", NULL},
    {"ifalog", 'l', "IFA Log", "S256", pyc_ifa_log, "PYC_IFA_LOG", log_flags_arg},
    {"system_directory", 'D', "System Directory", "S511", system_dir, "PYC_SYSTEM_DIRECTORY", NULL},
    {"write_code_exit", 'x', "Write Code and Exit Pass", "I", &write_code_exit, "PYC_WRITE_CODE_EXIT", NULL},
    {"verbose", 'v', "Verbosity Level", "+", &verbose_level, "PYC_VERBOSE", NULL},
    {"debug", 'd', "Debugging Level", "+", &debug_level, "PYC_DEBUG", NULL},
    {"license", ' ', "Show License", NULL, NULL, NULL, license},
    {"version", ' ', "Version", NULL, NULL, NULL, version},
    {"help", 'h', "Help", NULL, NULL, NULL, help},
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
#ifdef USE_SS
    if (codegen_shedskin) {
      if (shedskin_codegen(pdb->fa, if1->top->fun, fn)) fail("compilation failure");
    } else
#endif
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
  MEM_INIT();
  process_args(&arg_state, argc, argv);
  ifa_verbose = verbose_level;
  ifa_debug = debug_level;
  // Propagate the --strict-verify flag down to the ifa lib's
  // llvm.cc, which reads PYC_STRICT_VERIFY (the lib doesn't see
  // pyc's defs.h globals).  Setting it from either the CLI flag
  // or PYC_STRICT_VERIFY=1 is now equivalent.
  if (codegen_strict_verify) setenv("PYC_STRICT_VERIFY", "1", 1);
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
    }
  }
  fruntime_errors = runtime_errors;
  if (mods.n > 1) {
    ast_to_if1(mods);
    compile(first_filename);
  }
  Service::stop_all();
  exit(0);
  return 0;
}
