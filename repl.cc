// SPDX-License-Identifier: BSD-3-Clause
//
// repl.cc — interactive REPL for pyc (Phase 1).
//
// Strategy: accumulate statements in a session buffer; on each complete
// input, fork a child that runs the full compile+JIT pipeline on the
// accumulated session.  The fork gives us error recovery: fail() in
// the child exits nonzero; the parent discards the bad input and keeps
// the committed buffer intact.
//
// Block/bracket detection is heuristic: brackets are counted, block
// openers (lines ending with ':') require a blank line to terminate.
// Single-line bare expressions are auto-printed via a _repl_ wrapper.
//
// Limitations (Phase 1):
//   • exit(N) from Python code looks like a compile error (fork exits N)
//   • Expression detection misses some augmented assignments on complex lhs
//   • No persistent globals between iterations (each fork re-runs from scratch)

#include "defs.h"
#include "python_parse.h"
#include "python_ifa.h"

#include <cctype>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const size_t SESSION_MAX = 4 * 1024 * 1024;  // 4 MB session buffer
static const size_t LINE_MAX_   = 4096;

// ---- Bracket-depth tracking (skips strings and comments) ----------------

static int update_depth(const char *line, int depth) {
  bool in_str = false, triple = false;
  char sc = 0;
  for (const char *p = line; *p && *p != '\n'; p++) {
    if (in_str) {
      if (*p == '\\' && !triple) { p++; continue; }
      if (*p == sc) {
        if (triple && p[1] == sc && p[2] == sc) { triple = false; in_str = false; p += 2; }
        else if (!triple) in_str = false;
      }
      continue;
    }
    if (*p == '#') break;
    if (*p == '\'' || *p == '"') {
      sc = *p;
      if (p[1] == sc && p[2] == sc) { triple = true; p += 2; }
      in_str = true;
      continue;
    }
    if (*p == '(' || *p == '[' || *p == '{') depth++;
    else if (*p == ')' || *p == ']' || *p == '}') { if (depth > 0) depth--; }
  }
  return depth;
}

// True if a line at bracket-depth 0 ends with ':', signalling a block opener.
static bool ends_with_colon(const char *line, int depth_before) {
  if (depth_before != 0) return false;
  const char *p = line + strlen(line) - 1;
  while (p >= line && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) p--;
  return p >= line && *p == ':';
}

static bool is_blank(const char *s) {
  for (; *s; s++)
    if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return false;
  return true;
}

// ---- Definition vs side-effect classification ---------------------------
//
// For session replay we only need to re-run definitions (def, class, @,
// assignments) — not side effects (print, bare expressions, for/while).
// is_definition_block returns true when the complete input segment is a
// pure definition that should be replayed on the next compile.

static bool is_definition_block(const char *buf) {
  const char *s = buf;
  while (*s == ' ' || *s == '\t') s++;

  if (*s == '@') return true;
  if (strncmp(s, "def ",   4) == 0) return true;
  if (strncmp(s, "class ", 6) == 0) return true;
  if (strncmp(s, "async def ", 10) == 0) return true;

  // Multi-line that isn't def/class is a control-flow block (for/while/if)
  // — treat as side effect so it doesn't replay.
  const char *nl = strchr(buf, '\n');
  if (nl && *(nl + 1) != '\0') return false;

  // Single-line: a plain or augmented assignment is a definition.
  // Augmented: first token followed by op=
  {
    const char *p = s;
    while (isalnum(*p) || *p == '_' || *p == '.') p++;
    while (*p == ' ' || *p == '\t') p++;
    static const char *aug[] = {
      "**=","//=","<<=",">>=","+=","-=","*=","/=","%=","&=","|=","^=", nullptr
    };
    for (int i = 0; aug[i]; i++)
      if (strncmp(p, aug[i], strlen(aug[i])) == 0) return true;
  }
  // Plain assignment: top-level '=' not part of a compound operator.
  {
    int depth = 0; bool in_str = false; char sc = 0;
    for (const char *p = s; *p && *p != '\n' && *p != '#'; p++) {
      if (in_str) {
        if (*p == '\\') { p++; continue; }
        if (*p == sc) in_str = false;
        continue;
      }
      if (*p == '\'' || *p == '"') { in_str = true; sc = *p; continue; }
      if (*p == '(' || *p == '[' || *p == '{') { depth++; continue; }
      if (*p == ')' || *p == ']' || *p == '}') { if (depth > 0) depth--; continue; }
      if (*p == '=' && depth == 0) {
        static const char *ops = "!<>=+-*/%&|^:~";
        if (!(p > s && strchr(ops, p[-1])) && p[1] != '=') return true;
      }
    }
  }
  return false;
}

// ---- Expression detection (heuristic) -----------------------------------
//
// Returns true when 'buf' looks like a bare expression that should be
// auto-printed.  Single-line only; statement keywords and assignments
// return false.  Not perfect — see Phase 1 limitations above.

static bool is_bare_expression(const char *buf) {
  // Multi-line input (def / class / if / for / …) is never auto-printed.
  const char *nl = strchr(buf, '\n');
  if (nl && *(nl + 1) != '\0') return false;

  const char *s = buf;
  while (*s == ' ' || *s == '\t') s++;
  if (!*s || *s == '\n') return false;

  static const char *kws[] = {
    "def ", "class ", "if ", "elif ", "else", "for ", "while ",
    "return", "import ", "from ", "pass", "break", "continue",
    "raise", "try", "except", "finally", "with ", "del ",
    "assert ", "yield", "global ", "nonlocal ", "async ",
    // Common statement-like builtins that return None — don't auto-print.
    "print(", "print (", "exit(", "exit (", "quit(", "quit (",
    "@", "#", nullptr
  };
  for (int i = 0; kws[i]; i++)
    if (strncmp(s, kws[i], strlen(kws[i])) == 0) return false;

  // Augmented assignment: first token followed by op= (+=, -=, **=, …)
  {
    const char *p = s;
    while (isalnum(*p) || *p == '_') p++;
    while (*p == ' ' || *p == '\t') p++;
    static const char *aug[] = {
      "**=", "//=", "<<=", ">>=",
      "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", nullptr
    };
    for (int i = 0; aug[i]; i++)
      if (strncmp(p, aug[i], strlen(aug[i])) == 0) return false;
  }

  // Plain assignment: top-level '=' not part of ==, !=, <=, >=, :=, etc.
  int depth = 0;
  bool in_str = false; char sc = 0;
  for (const char *p = s; *p && *p != '\n' && *p != '#'; p++) {
    if (in_str) {
      if (*p == '\\') { p++; continue; }
      if (*p == sc) in_str = false;
      continue;
    }
    if (*p == '\'' || *p == '"') { in_str = true; sc = *p; continue; }
    if (*p == '(' || *p == '[' || *p == '{') { depth++; continue; }
    if (*p == ')' || *p == ']' || *p == '}') { if (depth > 0) depth--; continue; }
    if (*p == '=' && depth == 0) {
      static const char *non_assign = "!<>=+-*/%&|^:~";
      if (!(p > s && strchr(non_assign, p[-1])) && p[1] != '=') return false;
    }
  }
  return true;
}

// ---- Stage-2 bitcode cache ----------------------------------------------
//
// Cache key: FNV-64 of session file content (+ runtime_errors flag so
// different flag values don't share compiled output).
// Cache location: $HOME/.cache/pyc_repl/<16hex>.bc
//   Falls back to /tmp/pyc_repl_<uid>/ if HOME is unavailable.
// Cache eviction: not automatic — the directory accumulates .bc files
//   that can be removed manually or by OS tmp-cleaner.

static char s_cache_dir[512] = {};

static uint64_t fnv64(const char *buf, size_t len) {
  uint64_t h = 14695981039346656037ULL;
  const auto *p = reinterpret_cast<const unsigned char *>(buf);
  for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

static void setup_cache_dir() {
  if (s_cache_dir[0]) return;
  const char *home = getenv("HOME");
  if (!home || !*home) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) home = pw->pw_dir;
  }
  if (home && *home) {
    snprintf(s_cache_dir, sizeof(s_cache_dir), "%s/.cache/pyc_repl", home);
  } else {
    snprintf(s_cache_dir, sizeof(s_cache_dir), "/tmp/pyc_repl_%d", (int)getuid());
  }
  // Create the directory (and parent) if needed.
  if (home && *home) {
    char parent[512];
    snprintf(parent, sizeof(parent), "%s/.cache", home);
    mkdir(parent, 0700);
  }
  mkdir(s_cache_dir, 0700);
}

// ---- Child pipeline helpers ---------------------------------------------

static void write_session_file(const char *path, const char *buf, size_t len) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) { perror("pyc repl: open"); return; }
  if (write(fd, buf, len) != (ssize_t)len) perror("pyc repl: write");
  close(fd);
}

static PycModule *load_builtin_mod() {
  char dir[512], pyfile[512];
  snprintf(dir,    sizeof(dir),    "%s/__pyc__",    system_dir);
  snprintf(pyfile, sizeof(pyfile), "%s/__pyc__.py", system_dir);
  PyDAST *ast;
  cchar *fname = dupstr(pyfile);
  if (is_directory(dir))
    ast = dparse_builtin_dir(dir);
  else
    ast = dparse_python_to_ast(fname);
  if (!ast) return nullptr;
  PycModule *m = new PycModule(fname, true);
  m->pymod = ast;
  return m;
}

// Stage-3: lazy IF1 baseline.
// Built in the REPL parent on the first cache miss; zero cost for sessions
// where every input hits the Stage-2 bitcode cache.
// s_builtin_mods must be a persistent static so ctx->modules never dangles.
static PycModule          *s_builtin_mod  = nullptr;
static Vec<PycModule *>    s_builtin_mods;
static BaselineIF1State    s_baseline     = {};
static bool                s_baseline_built = false;

// Runs in the fork child: parses, analyses, JIT-executes tmpfile.
// Stage-2 cache path: if cache_path is non-empty, try bitcode first.
// Stage-3 path (cache miss): ast_to_if1_extend() instead of ast_to_if1().
static void run_compile_jit(const char *tmpfile, const char *cache_path) {
  // Stage-2 cache-hit: skip FA + codegen entirely.
  if (cache_path[0] && llvm_jit_read_cache(cache_path)) {
    if (llvm_jit_execute() != 0) exit(1);
    exit(0);
  }
  // Cache miss: tell jit.cc to write bitcode on success.
  if (cache_path[0])
    strncpy(llvm_jit_cache_path, cache_path, sizeof(llvm_jit_cache_path) - 1);

  // Stage-3: extend the pre-built baseline with the user module only.
  PyDAST *user_ast = dparse_python_to_ast(tmpfile);
  if (!user_ast) exit(1);
  PycModule *user = new PycModule(tmpfile, false);
  user->pymod = user_ast;
  Vec<PycModule *> mods;
  mods.add(s_builtin_mod);
  mods.add(user);
  fruntime_errors = runtime_errors;
  if (ast_to_if1_extend(mods, s_baseline) < 0) exit(1);
  compile(tmpfile);  // FA → codegen → llvm_jit_execute (writes cache, forks for exit() safety)
  exit(0);
}

// Fork + waitpid wrapper.
// candidate/clen: the session file content (already written to tmpfile)
//   used to compute the Stage-2 bitcode cache key.
// Returns 0 on success, nonzero on failure.
//
// Stage-3 lazy baseline: if the bitcode cache is absent (miss), build the
// IF1 baseline in the PARENT before forking so the child inherits it via
// CoW.  Cache hits skip the baseline entirely — zero startup cost.
static int try_compile_jit(const char *tmpfile, const char *candidate, size_t clen) {
  // Compute a content-addressed cache key.
  char cache_path[512] = {};
  if (s_cache_dir[0]) {
    uint64_t h = fnv64(candidate, clen);
    h ^= (uint64_t)(unsigned)runtime_errors * 0xc96c5795d7870f42ULL;
    snprintf(cache_path, sizeof(cache_path), "%s/%016llx.bc",
             s_cache_dir, (unsigned long long)h);
  }

  // Stage-3: if this will be a cache miss, ensure the baseline is ready in
  // the parent before we fork — the child inherits it via CoW.
  bool cache_hit = cache_path[0] && access(cache_path, F_OK) == 0;
  if (!cache_hit && !s_baseline_built) {
    s_builtin_mod = load_builtin_mod();
    if (!s_builtin_mod) { fprintf(stderr, "repl: failed to load __pyc__\n"); return 1; }
    s_builtin_mods.add(s_builtin_mod);
    s_baseline = ast_to_if1_baseline(s_builtin_mods);
    s_baseline_built = true;
  }

  fflush(stdout); fflush(stderr);
  pid_t pid = fork();
  if (pid < 0) { perror("pyc repl: fork"); return 1; }
  if (pid == 0) run_compile_jit(tmpfile, cache_path);
  int st = 0;
  while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
  return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

// ---- SIGINT handling ----------------------------------------------------

static volatile sig_atomic_t repl_got_sigint = 0;
static void repl_sigint_handler(int) { repl_got_sigint = 1; }

// ---- Main REPL loop ------------------------------------------------------

void pyc_repl() {
  codegen_llvm = 1;
  codegen_jit  = 1;

  // Ignore SIGINT in the parent; each fork child inherits the default and
  // can be killed by Ctrl-C without taking the REPL process with it.
  struct sigaction sa = {}, old_sa = {};
  sa.sa_handler = repl_sigint_handler;
  sigaction(SIGINT, &sa, &old_sa);

  char tmpfile[256];
  snprintf(tmpfile, sizeof(tmpfile), "/tmp/pyc_repl_%d.py", (int)getpid());

  char *committed  = (char *)malloc(SESSION_MAX);  // full history (for dedup)
  char *defs       = (char *)malloc(SESSION_MAX);  // definitions only (replayed each iter)
  char *new_buf    = (char *)malloc(SESSION_MAX);
  char *candidate  = (char *)malloc(SESSION_MAX + 512);
  if (!committed || !defs || !new_buf || !candidate) { perror("pyc repl: malloc"); return; }
  committed[0] = defs[0] = new_buf[0] = '\0';
  size_t committed_len = 0, defs_len = 0, new_len = 0;

  setup_cache_dir();
  fprintf(stderr, "pyc REPL  (Ctrl-D or exit() to quit)\n");

  char line[LINE_MAX_];
  int  depth = 0;
  bool block_pending = false;

  while (true) {
    if (repl_got_sigint) {
      // Ctrl-C: discard pending input and start fresh line.
      repl_got_sigint = 0;
      new_buf[0] = '\0'; new_len = 0;
      depth = 0; block_pending = false;
      fputs("\n", stdout);
    }

    fputs(depth > 0 || block_pending ? "... " : ">>> ", stdout);
    fflush(stdout);

    if (!fgets(line, sizeof(line), stdin)) {
      // EOF (Ctrl-D)
      fputs("\n", stdout);
      break;
    }
    if (!strcmp(line, "exit()\n") || !strcmp(line, "quit()\n")) break;

    bool blank      = is_blank(line);
    int  new_depth  = update_depth(line, depth);
    bool colon      = !blank && ends_with_colon(line, depth);

    // Append to the pending buffer.
    size_t ll = strlen(line);
    if (new_len + ll + 1 < SESSION_MAX) {
      memcpy(new_buf + new_len, line, ll + 1);
      new_len += ll;
    } else {
      fprintf(stderr, "repl: input too large, discarding\n");
      new_buf[0] = '\0'; new_len = 0;
      depth = 0; block_pending = false;
      continue;
    }
    depth = new_depth;

    // Determine if the statement is complete.
    if (depth > 0) continue;                      // unmatched open bracket
    if (!blank && block_pending) continue;         // still inside an indented block
    if (colon) { block_pending = true; continue; } // block opener — wait for body
    if (blank && block_pending) block_pending = false;  // blank line ends block

    if (new_len == 0 || is_blank(new_buf)) {
      new_buf[0] = '\0'; new_len = 0;
      continue;
    }

    // Session buffer overflow guard.
    if (committed_len + new_len + 256 >= SESSION_MAX) {
      fprintf(stderr, "repl: session buffer full\n");
      new_buf[0] = '\0'; new_len = 0;
      continue;
    }

    bool committed_ok = false;
    bool is_def = is_definition_block(new_buf);

    // Attempt 1: wrap as expression for auto-print.
    // Session file = defs (replayed definitions) + expression wrapper.
    if (is_bare_expression(new_buf)) {
      char trimmed[LINE_MAX_];
      memcpy(trimmed, new_buf, new_len + 1);
      size_t tl = new_len;
      while (tl > 0 && (trimmed[tl-1] == '\n' || trimmed[tl-1] == '\r')) trimmed[--tl] = '\0';

      size_t clen = (size_t)snprintf(
          candidate, SESSION_MAX + 511,
          "%.*s_repl_ = %s\nprint(_repl_)\n",
          (int)defs_len, defs, trimmed);
      write_session_file(tmpfile, candidate, clen);

      if (try_compile_jit(tmpfile, candidate, clen) == 0) {
        // Expression succeeded — add to committed (for dedup), NOT to defs
        // (side effects don't replay).
        memcpy(committed + committed_len, new_buf, new_len + 1);
        committed_len += new_len;
        committed_ok = true;
      }
    }

    // Attempt 2: compile as-is (statement, failed expression, or definition).
    // Session file = defs + new_buf.
    if (!committed_ok) {
      size_t clen = (size_t)snprintf(candidate, SESSION_MAX + 511,
                                     "%.*s%s", (int)defs_len, defs, new_buf);
      write_session_file(tmpfile, candidate, clen);

      if (try_compile_jit(tmpfile, candidate, clen) == 0) {
        memcpy(committed + committed_len, new_buf, new_len + 1);
        committed_len += new_len;
        if (is_def && defs_len + new_len < SESSION_MAX) {
          // Replay only definitions on subsequent compiles.
          memcpy(defs + defs_len, new_buf, new_len + 1);
          defs_len += new_len;
        }
      }
      // On failure the child already printed the error; just discard new_buf.
    }

    new_buf[0] = '\0'; new_len = 0;
  }

  sigaction(SIGINT, &old_sa, nullptr);
  unlink(tmpfile);
  free(committed); free(defs); free(new_buf); free(candidate);
}
