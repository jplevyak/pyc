/*
  Copyright 2003-2008 John Plevyak, All Rights Reserved
*/
#define EXTERN
#include <signal.h>
#include <sys/resource.h>
#include "defs.h"

int do_unit_tests = 0;

static void
help(ArgumentState *arg_state, char *arg_unused) {
  char ver[30];
  get_version(ver);
  fprintf(stderr, "PYC Version %s ", ver);
  fprintf(stderr, 
#include "COPYRIGHT.i"
);
  usage(arg_state, arg_unused);
}

static void
version(ArgumentState *arg_state, char *arg_unused) {
  char ver[30];
  get_version(ver);
  fprintf(stderr, "PYC Version %s ", ver);
  fprintf(stderr, 
#include "COPYRIGHT.i"
);
  exit(0);
}

static void
license(ArgumentState *arg_state, char *arg_unused) {
  fprintf(stderr,
#include "LICENSE.i"
);
  exit(0);
}

static ArgumentDescription arg_desc[] = {
  {"test", 't', "Unit Test", "F", &do_unit_tests, "AMI_TEST", NULL},
  {"verbose", 'v', "Verbosity Level", "+", &verbose_level, "PYC_VERBOSE", NULL},
  {"debug", 'd', "Debugging Level", "+", &debug_level, "PYC_DEBUG", NULL},
  {"license", ' ', "Show License", NULL, NULL, NULL, license},
  {"version", ' ', "Version", NULL, NULL, NULL, version},
  {"help", 'h', "Help", NULL, NULL, NULL, help},
  {0}
};

static ArgumentState arg_state("pyc", arg_desc);

static void init_system() {
  struct rlimit nfiles;
  assert(!getrlimit(RLIMIT_NOFILE, &nfiles));
  nfiles.rlim_cur = nfiles.rlim_max;
  assert(!setrlimit(RLIMIT_NOFILE, &nfiles));
  assert(!getrlimit(RLIMIT_NOFILE, &nfiles));
}

void
analyze(char *fn) {
  if (ifa_analyze(fn) < 0)
    fail("program does not type");
  if (fgraph)
    ifa_graph(fn);
  if (fdump_html) {
    char mktree_dir[512];
    strcpy(mktree_dir, system_dir);
    strcat(mktree_dir, "/etc/www");
    ifa_html(fn, mktree_dir);
  }
  if (fcg) {
    ifa_cg(fn);
    ifa_compile(fn);
  }
  return;
}

int main(int argc, char *argv[]) {
  init_config();
  INIT_RAND(0x1234567);
  process_args(&arg_state, argc, argv);
  if (arg_state.nfile_arguments != 1)
    usage(&arg_state, 0);
  char *filename = arg_state.file_argument[0];
  init_system();
  Service::start_all();
  if (do_unit_tests) {
    int r = UnitTest::run_all();
    Service::stop_all();
    _exit(r);
  }
  Py_Initialize();
  PyEval_InitThreads();
  PyArena *arena = PyArena_New();
  FILE *fp = fopen(filename, "r");
  if (!fp)
    fail("unable to read file '%s'", filename);
  mod_ty mod = PyParser_ASTFromFile(fp, filename, Py_file_input, 0, 0, 0, 0, arena);
  if (!mod)
    error("unable to parse file '%s'", filename);
  else {
    PyFutureFeatures *pyc_future = PyFuture_FromAST(mod, filename);
    if (!pyc_future)
      error("unable to parse futures for file '%s'", filename);
    else {
      ast_to_if1(mod);
      analyze(filename);
    }
  }
  PyArena_Free(arena);
  Py_Finalize();
  Service::stop_all();
  _exit(0);
  return 0;
}
