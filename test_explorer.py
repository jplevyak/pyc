#!/usr/bin/env python3
"""Smoke test for the compilation explorer (pyc --html / --dump-json).

Checks what the explorer promises and what has broken before:

  1. Telemetry is observational. A compile with --html --dump-json must emit
     byte-identical C (and LLVM IR) to a plain compile. It once did not:
     simple_inlining took its fixed point from the inline-event sidecar, so
     an explorer run inlined in up to four passes and a plain compile in one
     (ifa/optimize/inline.cc, inline_n_applied).
  2. The JSON parses, and its delta-encoded passes rebuild consistently:
     every AVar, type and flow-edge id resolves (ifa/common/html.cc, the
     phase-3 writer; explorer.js materializePass is the reader).
  3. The flow-analysis hooks fired: stage events and a split ledger with a
     reason, products and the partition it acted on.
  4. pyc -x: the value-taking explorer options are skipped by the script
     pre-scan, and the source view holds the program, not the cache copy.

Usage: ./test_explorer.py   (PYC=./pyc by default; run from the repo root)
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.abspath(__file__))
PYC = os.path.abspath(os.environ.get("PYC", os.path.join(ROOT, "pyc")))
# A program with demand splits (two list comprehensions over different
# element types) and the five-line inliner reproducer.
PROGRAMS = ["splitter_mark_type.py", "inline_rewritten_body.py"]
failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print("  FAIL", msg)
    return cond


def pyc(args, cwd):
    return subprocess.run([PYC, "-D", ROOT] + args, cwd=cwd, capture_output=True, text=True, timeout=600)


def materialize(p3):
    """Apply the passes in order, as explorer.js does; check every id."""
    cs_ids = {r[0] for r in p3["cs_info"]}
    av_ids = {r[0] for r in p3["avar_info"]}
    es, cs, av = {}, {}, {}
    for p in p3["passes"]:
        d = p.get("data")
        if p["summary_only"]:
            check(d is None and "summary" in p, f"pass {p['pass']}: summary-only pass has no summary")
            continue
        if d["keyframe"]:
            es.clear(), cs.clear(), av.clear()
        for key, state in (("es_gone", es), ("cs_gone", cs), ("av_gone", av)):
            for i in d.get(key, []):
                check(i in state, f"pass {p['pass']}: {key} names {i}, which is not in the state")
                state.pop(i, None)
        for e in d["entry_sets"]:
            es[e["id"]] = e
        for c in d["creation_sets"]:
            cs[c["id"]] = c
        for a in d["avars"]:
            av[a[0]] = a
        for a in av.values():
            if not (a[0] in av_ids and set(a[1]) <= cs_ids and set(a[2]) <= av_ids and set(a[3]) <= av_ids):
                check(False, f"pass {p['pass']}: avar {a[0]} names an id missing from avar_info/cs_info")
                break
    return es, cs, av


def explorer_json_from_html(path):
    s = open(path).read()
    m = re.search(r'<script id="pyc-compiler-data" type="application/json">\n(.*?)\n</script>', s, re.S)
    return json.loads(m.group(1)) if m else None


def test_program(name, backend_flags, tmp):
    label = name + (" " + " ".join(backend_flags) if backend_flags else "")
    print(f"-- {label}")
    work = os.path.join(tmp, re.sub(r"\W", "_", label))
    os.makedirs(work)
    shutil.copy(os.path.join(ROOT, "tests", name), work)
    stem = name[:-3]
    out = stem + (".ll" if "-b" in backend_flags else ".py.c")

    r = pyc(backend_flags + [name], work)
    if not check(r.returncode == 0, f"{label}: plain compile failed: {r.stderr[-400:]}"):
        return
    plain = open(os.path.join(work, out), "rb").read()

    r = pyc(backend_flags + ["--html", "--dump-json", name], work)
    if not check(r.returncode == 0, f"{label}: --html --dump-json compile failed: {r.stderr[-400:]}"):
        return
    check(open(os.path.join(work, out), "rb").read() == plain,
          f"{label}: {out} differs with --html --dump-json (telemetry changed codegen)")

    d = json.load(open(os.path.join(work, name + ".json")))
    h = explorer_json_from_html(os.path.join(work, name + ".html"))
    check(h == d, f"{label}: the data embedded in the .html differs from the .json")
    p3 = d["phase3_ifa"]
    check(p3.get("encoding") == "delta-v1", f"{label}: phase3 encoding is {p3.get('encoding')}")
    check(len(p3["passes"]) == d["meta"]["total_passes"] and p3["passes"], f"{label}: pass count")
    es, cs, av = materialize(p3)
    check(es and cs and av, f"{label}: the final pass rebuilds to an empty state")
    check(d["phase1_source"]["lines"] == open(os.path.join(work, name)).read().splitlines(),
          f"{label}: the source view is not the program")
    check(d["phase6_codegen"].get("lines"), f"{label}: no target code embedded")
    if name == "splitter_mark_type.py":
        # FA's own per-stage events: fa_events_enable() must reach the FA
        # (it used to run before the FA existed and do nothing). Events the
        # exporter builds from the splits alone carry "synthesized".
        events = [e for p in p3["passes"] for e in (p.get("data") or p)["summary"]["events"]]
        check(any(not e.get("synthesized") for e in events),
              f"{label}: every stage event is synthesized; FA recorded none")
        splits = [s for p in p3["passes"] for s in (p.get("data") or p)["summary"]["splits"]]
        check(splits, f"{label}: no splits recorded")
        for s in splits:
            if not (s["reason"] and s["stage"] and s["kind"]):
                check(False, f"{label}: split {s['id']} lacks a reason/stage/kind")
                break
            if s["kind"] == "EntrySet" and not (s["products"] and s["affected_edges"]):
                check(False, f"{label}: ES split {s['id']} records no products or moved edges")
                break


def test_exec_mode(tmp):
    """pyc -x with a value-taking explorer option before the script."""
    print("-- -x --html-max-size 50 --dump-json")
    work = os.path.join(tmp, "exec")
    os.makedirs(work)
    name = "inline_rewritten_body.py"
    shutil.copy(os.path.join(ROOT, "tests", name), work)
    r = pyc(["-x", "--html-max-size", "50", "--dump-json", name], work)
    if not check(r.returncode == 0, f"-x: failed: {r.stderr[-400:]}"):
        return
    check(r.stdout.split() == ["3.0", "3.5"], f"-x: the program's output was {r.stdout!r}")
    path = os.path.join(work, name + ".json")
    if not check(os.path.exists(path), "-x: the JSON is not beside the program"):
        return
    d = json.load(open(path))
    check(d["phase1_source"]["lines"] == open(os.path.join(work, name)).read().splitlines(),
          "-x: the source view is not the program (it read the cache copy)")


def main():
    if not os.path.exists(PYC):
        sys.exit(f"no compiler at {PYC}")
    with tempfile.TemporaryDirectory(prefix="pyc-explorer-") as tmp:
        for name in PROGRAMS:
            test_program(name, [], tmp)
            test_program(name, ["-b"], tmp)
        test_exec_mode(tmp)
    if failures:
        print(f"explorer smoke: {len(failures)} failure(s)")
        sys.exit(1)
    print("explorer smoke: OK")


if __name__ == "__main__":
    main()
