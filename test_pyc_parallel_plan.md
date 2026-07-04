# Speed up testing through parallelization

## Goal
The `test_pyc` suite currently evaluates ~130 tests sequentially. On an M1/M2/M3 CPU, this means we waste a lot of cores, taking ~2 minutes per backend when it could theoretically finish in 10-20 seconds. 

## Approach
1. We will rewrite `test_pyc` in Python (`test_pyc.py`). Python's `concurrent.futures.ThreadPoolExecutor` provides exactly what we need to run these independent test checks in parallel while reliably grouping terminal output so it isn't interleaved.
2. The Python runner will respect the same CLI args (e.g. `--bail`, `-v`, pattern, `PYC_FLAGS` env var).
3. The build outputs are naturally sandboxed by their base filename (`tests/build/$name.out`), so they won't step on each other's toes as long as we make sure they can run concurrently.
4. We can update `Makefile` to invoke the new parallelized python harness instead of the bash script.

## Verification
- Run `make test` and confirm it succeeds much faster (parallel execution).
- Check that `test_pyc` still properly handles failures when tests are intentionally broken.
