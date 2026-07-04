import subprocess
print("Compiling...")
subprocess.run(["./pyc", "-b", "tests/for_range_from_zero.py"])
print("Executing...")
try:
    res = subprocess.run(["./tests/for_range_from_zero"], timeout=5, capture_output=True, text=True)
    print("Return code:", res.returncode)
    print("Output:", repr(res.stdout))
except subprocess.TimeoutExpired:
    print("TIMEOUT!")
