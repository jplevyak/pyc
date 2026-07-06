#!/usr/bin/env python3
import argparse
import glob
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading

def parse_args():
    parser = argparse.ArgumentParser(description="End-to-end test suite for pyc.")
    parser.add_argument("-v", "--verbose", action="store_true", help="show all test results")
    parser.add_argument("-k", "--keep", action="store_true", help="keep tests/build/ after run")
    parser.add_argument("-b", "--bail", action="store_true", help="stop at first failure")
    parser.add_argument("--no-cpython", action="store_true", help="skip CPython cross-verify")
    parser.add_argument("-t", "--timeout", type=float, default=float(os.environ.get("TIMEOUT", "60")), help="per-step timeout in seconds")
    parser.add_argument("pattern", nargs="?", default="", help="only run tests whose name matches pattern")
    return parser.parse_args()

def diff_files(file1, file2):
    try:
        # returns (is_diff, stdout)
        # diff -q is not used because we want the first 20 lines if it fails
        res = subprocess.run(["diff", file1, file2], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if res.returncode != 0:
            lines = res.stdout.splitlines()[:20]
            return True, "\n".join(lines)
        return False, ""
    except Exception as e:
        return True, str(e)

def format_color(text, color):
    if not sys.stdout.isatty():
        return text
    colors = {
        'red': '\033[31m',
        'green': '\033[32m',
        'yellow': '\033[33m',
        'bold': '\033[1m',
        'reset': '\033[0m'
    }
    return f"{colors[color]}{text}{colors['reset']}"

def run_test(g, args, envs):
    name = os.path.basename(g)
    base = name[:-3] if name.endswith(".py") else name
    
    if args.pattern and args.pattern not in name:
        return None

    if os.path.exists(f"{g}.ignore"):
        return {"name": name, "status": "SKIP", "msg": "ignored"}

    flags = ""
    if os.path.exists(f"{g}.flags"):
        with open(f"{g}.flags") as f:
            flags = f.read().strip()

    build_dir = os.path.abspath(envs["BUILD"])
    pyc_abs = os.path.abspath(envs["PYC"])
    tests_dir_abs = os.path.abspath(envs["TESTS_DIR"])

    cmd = []
    if envs["VALGRIND"]:
        cmd.extend(envs["VALGRIND"].split())
    cmd.append(pyc_abs)
    if envs["PYC_FLAGS"]:
        cmd.extend(envs["PYC_FLAGS"].split())
    cmd.extend(["-D", "../.."])
    if flags:
        cmd.extend(flags.split())
    cmd.append(name)

    out_path = os.path.join(build_dir, f"{name}.out")
    
    # Compile
    print(f"CMD: {' '.join(cmd)}")
    try:
        with open(out_path, "w") as outf:
            res = subprocess.run(cmd, cwd=build_dir, stdout=outf, stderr=subprocess.STDOUT, timeout=args.timeout)
            rc = res.returncode
    except subprocess.TimeoutExpired:
        return {"name": name, "status": "FAIL", "stage": "COMPILE-TIMEOUT", "msg": f"log: {envs['BUILD']}/{name}.out"}

    if rc != 0:
        if os.path.exists(f"{g}.expect_fail"):
            return {"name": name, "status": "XFAIL", "stage": "compile"}
        if os.path.exists(f"{g}.check_fail"):
            return {"name": name, "status": "PASS", "msg": "(compile-fail-ok)"}
        return {"name": name, "status": "FAIL", "stage": "COMPILE", "msg": f"log: {envs['BUILD']}/{name}.out"}

    # Check compile stdout
    check_file = f"{g}.check"
    if not os.path.exists(check_file):
        check_file = os.path.join(envs["TESTS_DIR"], "empty")
    
    # resolve check_file to absolute path
    check_file_abs = os.path.abspath(check_file)
    is_diff, diff_out = diff_files(out_path, check_file_abs)
    if is_diff:
        return {"name": name, "status": "FAIL", "stage": "COMPILE-OUT", "msg": f"diff {envs['BUILD']}/{name}.out {check_file}", "diff": diff_out}

    # Execute
    exec_check = f"{g}.exec.check"
    if not os.path.exists(exec_check):
        return {"name": name, "status": "PASS", "msg": "(compile-only)"}

    exec_out_path = os.path.join(build_dir, f"{name}.exec.out")
    exec_cmd = []
    if envs["VALGRIND"]:
        exec_cmd.extend(envs["VALGRIND"].split())
    exec_cmd.append(f"./{base}")

    try:
        with open(exec_out_path, "w") as outf:
            res = subprocess.run(exec_cmd, cwd=build_dir, stdout=outf, stderr=subprocess.STDOUT, timeout=args.timeout)
            rc = res.returncode
    except subprocess.TimeoutExpired:
        return {"name": name, "status": "FAIL", "stage": f"EXEC-TIMEOUT", "msg": f"log: {envs['BUILD']}/{name}.exec.out"}

    exec_check_abs = os.path.abspath(exec_check)
    is_diff, diff_out = diff_files(exec_out_path, exec_check_abs)
    if rc != 0 or is_diff:
        if os.path.exists(f"{g}.check_fail"):
            return {"name": name, "status": "PASS", "msg": "(exec-fail-ok)"}
        diff_str = diff_out if is_diff else f"Exit code: {rc}"
        return {"name": name, "status": "FAIL", "stage": "EXEC", "msg": f"diff {envs['BUILD']}/{name}.exec.out {exec_check}", "diff": diff_str}

    # CPython verify
    if not args.no_cpython:
        python_out_path = os.path.join(build_dir, f"{name}.python.out")
        py_env = os.environ.copy()
        py_env["PYTHONPATH"] = "."
        try:
            with open(python_out_path, "w") as outf:
                subprocess.run([envs["PYTHON"], name], cwd=build_dir, env=py_env, stdout=outf, stderr=subprocess.STDOUT, timeout=args.timeout)
        except subprocess.TimeoutExpired:
            return {"name": name, "status": "FAIL", "stage": f"CPYTHON-TIMEOUT", "msg": f"log: {envs['BUILD']}/{name}.python.out"}

        is_diff, diff_out = diff_files(exec_out_path, python_out_path)
        if is_diff:
            if os.path.exists(f"{g}.python.expect_fail"):
                return {"name": name, "status": "XFAIL", "stage": "cpython"}
            return {"name": name, "status": "FAIL", "stage": "CPYTHON", "msg": f"diff {envs['BUILD']}/{name}.exec.out {envs['BUILD']}/{name}.python.out", "diff": diff_out}

    return {"name": name, "status": "PASS", "msg": ""}

def main():
    args = parse_args()

    envs = {
        "PYC": os.environ.get("PYC", "./pyc"),
        "PYTHON": os.environ.get("PYTHON", "python3"),
        "VALGRIND": os.environ.get("VALGRIND", ""),
        "PYC_FLAGS": os.environ.get("PYC_FLAGS", ""),
        "PYC_STRICT_VERIFY": os.environ.get("PYC_STRICT_VERIFY", "1"),
        "BUILD": "tests/build",
        "TESTS_DIR": "tests",
    }
    os.environ["PYC_STRICT_VERIFY"] = envs["PYC_STRICT_VERIFY"]

    if not os.path.isfile(envs["PYC"]) or not os.access(envs["PYC"], os.X_OK):
        print(f"{envs['PYC']}: not found or not executable. Run 'make' first.", file=sys.stderr)
        sys.exit(1)

    if not args.keep:
        shutil.rmtree(envs["BUILD"], ignore_errors=True)
    os.makedirs(envs["BUILD"], exist_ok=True)
    
    try:
        shutil.copy("pyc_compat.py", envs["BUILD"])
    except FileNotFoundError:
        pass

    test_files = sorted(glob.glob(os.path.join(envs["TESTS_DIR"], "*.py")))
    for f in test_files:
        dst = os.path.join(envs["BUILD"], os.path.basename(f))
        if not os.path.exists(dst):
            os.symlink(os.path.join("..", os.path.basename(f)), dst)
            
    # create empty check file if needed
    empty_file = os.path.join(envs["TESTS_DIR"], "empty")
    if not os.path.exists(empty_file):
        with open(empty_file, "w") as f:
            pass

    passed = 0
    failed = 0
    expected_failed = 0
    skipped = 0
    failures = []

    print_lock = threading.Lock()
    start_time = time.time()

    def print_result(res):
        nonlocal passed, failed, expected_failed, skipped
        if not res:
            return
        
        status = res["status"]
        name = res["name"]
        
        with print_lock:
            if status == "PASS":
                passed += 1
                if args.verbose:
                    print(f"  {format_color('PASS', 'green')} {name} {res['msg']}")
                else:
                    print(format_color('.', 'green'), end='', flush=True)
            elif status == "SKIP":
                skipped += 1
                if args.verbose:
                    print(f"  {format_color('SKIP', 'yellow')} {name} {res['msg']}")
                else:
                    print(format_color('s', 'yellow'), end='', flush=True)
            elif status == "XFAIL":
                expected_failed += 1
                if args.verbose:
                    print(f"  {format_color('XFAIL', 'yellow')} {name} ({res.get('stage', '')})")
                else:
                    print(format_color('x', 'yellow'), end='', flush=True)
            elif status == "FAIL":
                failed += 1
                failures.append(f"{name} {res['stage']}  {res['msg']}")
                if not args.verbose:
                    print()
                print(f"  {format_color('FAIL', 'red')} {name} ({res['stage']})", file=sys.stderr)
                if 'diff' in res and res['diff']:
                    print(res['diff'], file=sys.stderr)
            
            if args.bail and failed > 0:
                return True
        return False

    with ThreadPoolExecutor() as executor:
        futures = {executor.submit(run_test, g, args, envs): g for g in test_files}
        for future in as_completed(futures):
            res = future.result()
            if print_result(res):
                executor.shutdown(wait=False, cancel_futures=True)
                break

    elapsed = int(time.time() - start_time)
    
    if not args.verbose:
        print()
    print(f"\n{format_color('---- summary ----', 'bold')}")
    print(f"  {format_color('passed', 'green')}          {passed}")
    print(f"  {format_color('expected fails', 'yellow')}  {expected_failed}")
    print(f"  {format_color('failed', 'red')}          {failed}")
    print(f"  skipped         {skipped}")
    print(f"  time            {elapsed}s")
    
    if failures:
        print(f"\n{format_color('failures:', 'red')}")
        for f in failures:
            print(f"  {f}")

    if not args.keep:
        shutil.rmtree(envs["BUILD"], ignore_errors=True)
        
    sys.exit(1 if failed > 0 else 0)

if __name__ == '__main__':
    main()
