/*
  Copyright 2003-2009 John Plevyak, All Rights Reserved
*/
#define EXTERN
#include "defs.h"
#include <signal.h>
#include <sys/resource.h>

int do_unit_tests = 0;
static char pyc_ifa_log[256];

static void help(ArgumentState *arg_state, char *arg_unused) {
  char ver[30];
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
    {"debug_info", 'g', "Produce Debugging Information", "F", &codegen_debug, "PYC_DEBUG_INFO", NULL},
    {"optimize", 'O', "Optimize", "F", &codegen_optimize, "PYC_OPTIMIZE", NULL},
#ifdef USE_LLVM
    {"llvm", 'b', "LLVM Codegen", "F", &codegen_llvm, "PYC_LLVM", NULL},
    {"jit", 'j', "JIT", "F", &codegen_jit, "PYC_JIT", NULL},
#endif
#ifdef DEBUG
    {"test", 't', "Unit Test", "F", &do_unit_tests, "PYC_TEST", NULL},
    {"test_scoping", ' ', "Test Scoping", "F", &test_scoping, "PYC_TEST_SCOPING", NULL},
#endif
#ifdef USE_SS
    {"ss", 's', "Shedskin Codegen", "F", &codegen_shedskin, "PYC_SS", NULL},
#endif
    {"runtime_errors", 'r', "Use runtime type checks", "f", &runtime_errors, "PYC_RUNTIME_ERRORS", NULL},
    {"html", ' ', "Output as HTML", "F", &fdump_html, "PYC_HTML", NULL},
    {"ifalog", 'l', "IFA Log", "S256", pyc_ifa_log, "PYC_IFA_LOG", log_flags_arg},
    {"system_directory", 'D', "System Directory", "S511", system_dir, "PYC_SYSTEM_DIRECTORY", NULL},
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
      llvm_codegen(pdb->fa, if1->top->fun, fn);
      if (!codegen_jit && llvm_codegen_compile(fn)) fail("compilation failure");
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
  INIT_RAND64(0x1234567);
  process_args(&arg_state, argc, argv);
  ifa_verbose = verbose_level;
  ifa_debug = debug_level;
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
  Py_Initialize();
  PyEval_InitThreads();
  PyArena *arena = PyArena_New();
  cchar *first_filename = 0;
  Vec<PycModule *> mods;
  for (int i = -1; i < arg_state.nfile_arguments; i++) {
    cchar *filename = 0;
    if (i < 0) {
      char fn[256];
      strcpy(fn, system_dir);
      strcat(fn, "/__pyc__.py");
      filename = dupstr(fn);
    } else
      filename = arg_state.file_argument[i];
    if (!i) first_filename = filename;
    mod_ty mod = file_to_mod(filename, arena);
    if (mod) mods.add(new PycModule(mod, filename, i < 0));
  }
  fruntime_errors = runtime_errors;
  if (mods.n > 1) {
    ast_to_if1(mods, arena);
    compile(first_filename);
  }
  PyArena_Free(arena);
  Py_Finalize();
  Service::stop_all();
  exit(0);
  return 0;
}
