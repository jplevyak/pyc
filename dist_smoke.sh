#!/bin/bash
# dist_smoke.sh TARBALL [TESTS_DIR]
#
# Does a `make dist` tarball work where nothing else of pyc is? Unpacks it
# into a fresh temporary directory, then compiles and runs a fixed set of
# suite programs from another fresh directory, on both backends, and
# compares each program's output with its `.exec.check`. The JIT gets one
# program. Exit 0 only if every check passes.
#
# Run it in a clean container to prove the tarball is self-contained, e.g.
#   docker run --rm -v "$PWD:/src:ro" ubuntu:24.04 bash -c \
#     'apt-get update -qq && apt-get install -y -qq clang lld make >/dev/null &&
#      /src/dist_smoke.sh /src/dist/pyc-*.tar.gz /src/tests'
# The only things the image provides are a C++ compiler, lld and make: no
# LLVM development package, no Boehm GC, no dparser.
#
# Every binary it checks -- pyc, and each program pyc builds -- must load
# only the operating system's libraries (/lib, /usr/lib, /System on macOS).
# That is the self-containment test where a clean machine is not available
# (a macOS runner keeps Homebrew's libraries installed).
set -u
TARBALL=$(realpath "$1")
TESTS=$(realpath "${2:-$(dirname "$0")/tests}")
PROGRAMS="call_unpacking_into_named list_eq_other_type narrowing_not_isinstance kwargs_parameter
          generator_send_typed decorator_method property_setter"

ROOT=$(mktemp -d)
echo "unpacked in $ROOT"
tar -xzf "$TARBALL" -C "$ROOT"
PYC=$(echo "$ROOT"/*/bin/pyc)
[ -x "$PYC" ] || { echo "no bin/pyc in $TARBALL"; exit 1; }
WORK="$ROOT/work"
mkdir -p "$WORK"
cd "$WORK" || exit 1
echo "pyc: $PYC"
"$PYC" --version | head -1

# The shared libraries a binary loads, one path per line.
deps() {
  if [ "$(uname -s)" = Darwin ]; then otool -L "$1" | tail -n +2 | awk '{print $1}'
  else ldd "$1" | awk '/=>/ {print $3} !/=>/ {print $1}'; fi
}
# Fail (print) any that is not the system's.
check_deps() {
  local bad
  bad=$(deps "$1" | grep -vE '^(/lib|/usr/lib|/System/|linux-vdso|/lib64)' | grep -v '^$')
  if [ -n "$bad" ]; then echo "FAIL [deps] $2 loads non-system libraries:"; echo "$bad" | sed 's/^/    /'; return 1; fi
}

fail=0
check_deps "$PYC" pyc && echo "ok   [deps] pyc" || fail=1
for flags in "" "-b"; do
  for p in $PROGRAMS; do
    cp "$TESTS/$p.py" .
    if ! "$PYC" $flags "$p.py" >"$p.compile.out" 2>&1; then
      echo "FAIL [${flags:-C}] $p: compile"; sed 's/^/    /' "$p.compile.out" | head -10; fail=1; continue
    fi
    if ./"$p" 2>&1 | cmp -s - "$TESTS/$p.py.exec.check"; then
      echo "ok   [${flags:-C}] $p"
    else
      echo "FAIL [${flags:-C}] $p: output differs from $p.py.exec.check"; fail=1
    fi
    check_deps "./$p" "[${flags:-C}] $p" || fail=1
    rm -f "$p" "$p.py.c" "$p.ll" "$p.o"
  done
done
printf 'd = {"a": [1, 2]}\nprint(1 + 2, sorted(d), "x".join(["p", "q"]))\n' > jit.py
if [ "$("$PYC" -j jit.py 2>&1)" = "3 ['a'] pxq" ]; then echo "ok   [JIT] jit"; else echo "FAIL [JIT] jit"; fail=1; fi
[ $fail = 0 ] && echo "dist smoke: PASS" || echo "dist smoke: FAIL"
exit $fail
