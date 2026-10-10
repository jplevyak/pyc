#!/bin/bash
# pyc check driver for webserver (added, not an upstream file: see
# ../PYC_CHANGES.md). webserver serves port 50000 forever, so it never ends
# and prints nothing until a client connects. This starts the server with
# the command it is given (`./webserver`, or `python3 webserver.py`), sends a
# fixed script of requests, prints the responses, stops the server, and
# prints what the server printed. corpus_sweep.sh runs it for both arms.
#
# Output is deterministic except the client sockets' file descriptor
# numbers, which depend on how many files the process has open (CPython opens
# more), so `got client: N` is normalized.
PORT=50000
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
# Both arms unbuffered, or the server's output is lost when it is stopped:
# PYTHONUNBUFFERED for CPython, stdbuf for a pyc binary's C stdio (each is a
# no-op for the other).
#
# The server does not set SO_REUSEADDR, so right after a previous run (the
# sweep samples CPython twice in a row) the port can still be in TIME_WAIT
# and bind fails with EADDRINUSE. Retry until it binds; TIME_WAIT lasts
# about 60 s.
#
# Readiness is a connect. The probe that succeeds is itself a client
# (connect, then close without sending), so it appears in the server's log
# the same way in both arms.
up=0
deadline=$(( $(date +%s) + 90 ))
while [ $up = 0 ] && [ $(date +%s) -lt $deadline ]; do
  PYTHONUNBUFFERED=1 stdbuf -o0 -e0 "$@" > "$tmp/server.out" 2> "$tmp/server.err" &
  server=$!
  for i in $(seq 1 50); do
    if (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null; then up=1; break; fi
    kill -0 $server 2>/dev/null || break
    sleep 0.1
  done
  if [ $up = 0 ]; then
    kill $server 2>/dev/null; wait $server 2>/dev/null
    sleep 2
  fi
done
if [ $up = 0 ]; then
  echo "server did not start"
  cat "$tmp/server.err"
  exit 1
fi

# The server logs every client close. Waiting for the Nth one before the next
# step makes the order of events fixed: otherwise the next client can
# connect before the server has handled the previous close, and the two lines
# swap depending on timing (seen: CPython vs a pyc binary, whose select polls
# each fd in turn).
closes() { grep -c "close from client detected" "$tmp/server.out"; }
wait_closes() {  # wait_closes N
  for i in $(seq 1 100); do
    [ "$(closes)" -ge "$1" ] && return 0
    sleep 0.05
  done
  return 0
}

# One request per connection, sent with a single sendall: the server reads a
# request with ONE recv, so a request split across writes (bash's printf to
# /dev/tcp did that) is half-read, and the run is not repeatable. `\r` and
# `\n` in the argument are CR and LF.
request() {  # request LABEL RAW-REQUEST
  echo "== $1"
  python3 -I -c '
import socket, sys
req = sys.argv[2].replace("\\r", "\r").replace("\\n", "\n")
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
s.sendall(req.encode())
out = b""
while True:
    b = s.recv(4096)
    if not b:
        break
    out += b
sys.stdout.write(out.decode())
' "$PORT" "$2"
  echo
}

wait_closes 1   # the readiness probe
request "GET /" 'GET / HTTP/1.0\r\n\r\n'
request "GET with query and headers" \
  'GET /search?q=hello+world&lang=%41%42c&empty= HTTP/1.0\r\nHost: localhost\r\nUser-Agent: check\r\n\r\n'
echo "== connect, close without sending"
(exec 3<>/dev/tcp/127.0.0.1/$PORT)
wait_closes 2

kill $server 2>/dev/null
wait $server 2>/dev/null
echo "== server stdout"
sed -E 's/got client: [0-9]+/got client: N/' "$tmp/server.out"
