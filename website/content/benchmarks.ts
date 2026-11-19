// Wall-clock times, one run each, stdout to /dev/null, measured 2026-10-19
// at commit 0aa9fcb1 on an AMD Ryzen 9 3950X. pyc used its default C backend
// (-O2). The programs are unmodified from shedskin_examples/. Re-measure
// and update together.

export interface Bench {
  name: string;
  what: string;
  cpython: number; // ms
  pyc: number; // ms
}

export const BENCHMARKS: Bench[] = [
  { name: 'fib(35)', what: 'Naive recursive Fibonacci', cpython: 1155, pyc: 24 },
  { name: 'go', what: 'Monte Carlo tree search Go player', cpython: 62279, pyc: 7506 },
  { name: 'sudoku1', what: 'Constraint-propagation Sudoku solver', cpython: 9912, pyc: 1527 },
  { name: 'nbody', what: 'Planetary orbit simulation', cpython: 73260, pyc: 13030 },
  { name: 'pystone', what: 'The classic Dhrystone port', cpython: 18038, pyc: 4252 },
  { name: 'mandelbrot2', what: 'Mandelbrot set renderer', cpython: 1990, pyc: 716 },
];

export const BENCH_SETUP =
  'Wall-clock time of a single run, CPython 3.12 vs. pyc with its default C backend, on an AMD ' +
  'Ryzen 9 3950X. Programs are unmodified from the shedskin example corpus, except fib(35), ' +
  'which is the four-line program on the install page. Speedups depend heavily on the program: ' +
  'code dominated by calls and integer arithmetic gains the most, and code that mostly ' +
  'allocates gains less.';
