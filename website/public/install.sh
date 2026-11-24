#!/bin/sh
# Install pyc, the Python-to-native compiler, from a GitHub release.
#
#   curl -fsSL https://pyc-lang.org/install.sh | sh
#
# Picks the tarball for this OS and CPU, checks it against the release's
# SHA256SUMS, unpacks it to $PYC_PREFIX/share/pyc/<version>, and links
# $PYC_PREFIX/bin/pyc to it. No root needed. Re-running upgrades; the
# previous version stays in share/pyc/ until you delete it.
#
# Settings (environment, or `sh -s -- --flag` when piped):
#   PYC_VERSION=0.2.0    a specific release        (--version 0.2.0)
#                        default: the latest
#   PYC_PREFIX=DIR       install under DIR         (--prefix DIR)
#                        default: ~/.local
#   PYC_FORCE=1          skip the OS version check (--force)
#
# Supported: Linux x86-64 and arm64 with glibc 2.39+, macOS 15+ on Apple
# silicon or Intel. Anything else builds from source:
# https://pyc-lang.org/install/
#
# The script is one function called on its last line, so a download cut
# off part way runs nothing.

set -eu

pyc_install() {
  REPO=https://github.com/jplevyak/pyc
  VERSION=${PYC_VERSION:-}
  PREFIX=${PYC_PREFIX:-${HOME:?HOME is not set}/.local}
  FORCE=${PYC_FORCE:-}

  while [ $# -gt 0 ]; do
    case $1 in
      --version) VERSION=${2:?--version needs a value}; shift 2 ;;
      --version=*) VERSION=${1#*=}; shift ;;
      --prefix) PREFIX=${2:?--prefix needs a value}; shift 2 ;;
      --prefix=*) PREFIX=${1#*=}; shift ;;
      --force) FORCE=1; shift ;;
      -h | --help) usage; return 0 ;;
      *) die "unknown option: $1 (try --help)" ;;
    esac
  done
  VERSION=${VERSION#v}

  # --- what to fetch -------------------------------------------------------
  os=$(uname -s)
  arch=$(uname -m)
  case $os in
    Linux) os=linux ;;
    Darwin) os=darwin ;;
    *) die "no pyc release for $os; build from source: https://pyc-lang.org/install/" ;;
  esac
  case $arch in
    x86_64 | amd64) arch=x86_64 ;;
    aarch64 | arm64) arch=arm64 ;;
    *) die "no pyc release for $os on $arch; build from source: https://pyc-lang.org/install/" ;;
  esac
  if [ "$os" = darwin ] && [ "$arch" = x86_64 ] &&
    [ "$(sysctl -n sysctl.proc_translated 2>/dev/null || true)" = 1 ]; then
    arch=arm64 # a shell under Rosetta on Apple silicon
  fi
  if [ "$os" = linux ] && [ "$arch" = arm64 ]; then arch=aarch64; fi

  # --- can this machine run it ---------------------------------------------
  if [ "$os" = linux ]; then
    glibc=$(getconf GNU_LIBC_VERSION 2>/dev/null | sed -n 's/^glibc //p' || true)
    if [ -z "$glibc" ]; then
      too_old "this Linux is not glibc-based (musl?); the release needs glibc 2.39 or newer"
    elif version_lt "$glibc" 2.39; then
      too_old "glibc $glibc is too old; the release needs glibc 2.39 or newer (Ubuntu 24.04, Debian 13, Fedora 40)"
    fi
  else
    macos=$(sw_vers -productVersion 2>/dev/null || echo 0)
    if version_lt "$macos" 15; then
      too_old "macOS $macos is too old; the release needs macOS 15 (Sequoia) or newer"
    fi
  fi

  if command -v curl >/dev/null 2>&1; then
    fetch() { curl -fsSL --retry 3 -o "$2" "$1"; }
    resolve() { curl -fsSLI -o /dev/null -w '%{url_effective}' "$1"; }
  elif command -v wget >/dev/null 2>&1; then
    fetch() { wget -q -O "$2" "$1"; }
    resolve() { wget -q -S --spider "$1" 2>&1 | sed -n 's/^ *[Ll]ocation: *//p' | tail -n 1 | tr -d '\r'; }
  else
    die "need curl or wget"
  fi
  if command -v sha256sum >/dev/null 2>&1; then
    sha256() { sha256sum "$1" | cut -d' ' -f1; }
  elif command -v shasum >/dev/null 2>&1; then
    sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }
  else
    die "need sha256sum or shasum to check the download"
  fi

  if [ -z "$VERSION" ]; then
    tag=$(resolve "$REPO/releases/latest") || die "could not reach $REPO/releases/latest"
    VERSION=${tag##*/tag/v}
    case $VERSION in
      '' | */* | *:*) die "could not find the latest release (got '$tag'); set PYC_VERSION" ;;
    esac
  fi

  name=pyc-$VERSION-$os-$arch
  url=$REPO/releases/download/v$VERSION
  dest=$PREFIX/share/pyc/$VERSION

  # --- download, check, unpack ---------------------------------------------
  tmp=$(mktemp -d 2>/dev/null || mktemp -d -t pyc)
  trap 'rm -rf "$tmp"' EXIT INT TERM
  say "downloading pyc $VERSION for $os $arch"
  fetch "$url/$name.tar.gz" "$tmp/$name.tar.gz" ||
    die "no $name.tar.gz in release v$VERSION ($url)"
  fetch "$url/SHA256SUMS" "$tmp/SHA256SUMS" || die "release v$VERSION has no SHA256SUMS"
  want=$(awk -v f="$name.tar.gz" '$2 == f || $2 == "*" f { print $1 }' "$tmp/SHA256SUMS")
  [ -n "$want" ] || die "SHA256SUMS does not list $name.tar.gz"
  got=$(sha256 "$tmp/$name.tar.gz")
  [ "$got" = "$want" ] || die "checksum mismatch for $name.tar.gz (expected $want, got $got)"

  tar -xzf "$tmp/$name.tar.gz" -C "$tmp"
  [ -x "$tmp/$name/bin/pyc" ] || die "$name.tar.gz has no bin/pyc"

  mkdir -p "$PREFIX/share/pyc" "$PREFIX/bin"
  rm -rf "$dest"
  mv "$tmp/$name" "$dest"
  ln -sf "$dest/bin/pyc" "$PREFIX/bin/pyc"
  if [ -f "$dest/share/man/man1/pyc.1" ]; then
    mkdir -p "$PREFIX/share/man/man1"
    ln -sf "$dest/share/man/man1/pyc.1" "$PREFIX/share/man/man1/pyc.1"
  fi

  "$PREFIX/bin/pyc" --version >/dev/null 2>&1 ||
    die "installed $PREFIX/bin/pyc, but it does not run on this machine"
  say "installed pyc $VERSION: $PREFIX/bin/pyc -> $dest"

  # --- what is still missing -----------------------------------------------
  missing=
  command -v clang++ >/dev/null 2>&1 || missing="clang++"
  command -v make >/dev/null 2>&1 || missing="${missing:+$missing }make"
  if [ -n "$missing" ]; then
    if [ "$os" = darwin ]; then hint="xcode-select --install"; else hint="sudo apt-get install clang make (or your distribution's equivalent)"; fi
    say "pyc compiles with clang++ and make; not found: $missing. Install with: $hint"
  fi
  case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) say "$PREFIX/bin is not on your PATH; add it with:"
       say "  export PATH=\"$PREFIX/bin:\$PATH\"" ;;
  esac
  say "try it: echo 'print(\"hello\")' > hello.py && pyc hello.py && ./hello"
}

usage() {
  cat <<'USAGE'
usage: curl -fsSL https://pyc-lang.org/install.sh | sh -s -- [options]

  --version V   install release V (default: the latest)    PYC_VERSION=V
  --prefix DIR  install under DIR (default: ~/.local)      PYC_PREFIX=DIR
  --force       install even if the OS looks too old       PYC_FORCE=1

pyc goes to DIR/share/pyc/V, linked from DIR/bin/pyc.
USAGE
}
say() { printf 'pyc-install: %s\n' "$*"; }
die() { printf 'pyc-install: error: %s\n' "$*" >&2; exit 1; }
too_old() {
  if [ -n "$FORCE" ]; then
    say "warning: $1 (continuing: --force)"
  else
    die "$1. Build from source instead: https://pyc-lang.org/install/ (or --force to try anyway)"
  fi
}
# version_lt A B: is dotted version A older than B?
version_lt() {
  if [ "$1" = "$2" ]; then return 1; fi
  [ "$(printf '%s\n%s\n' "$1" "$2" | sort -t. -k1,1n -k2,2n -k3,3n | head -n 1)" = "$1" ]
}

pyc_install "$@"
