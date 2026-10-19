export type State = 'done' | 'active' | 'planned';

export interface Milestone {
  title: string;
  state: State;
  when?: string;
  items: string[];
}

export interface Track {
  id: string;
  title: string;
  summary: string;
  milestones: Milestone[];
}

export const TRACKS: Track[] = [
  {
    id: 'foundation',
    title: 'Foundation',
    summary: 'What is already in place: a Python 3 compiler that matches CPython on a real benchmark corpus.',
    milestones: [
      {
        title: 'Python 3, on its own parser',
        state: 'done',
        items: [
          'Python 2.7 → 3 migration of grammar, AST and builtin library',
          'DParser grammar replaces the dependency on CPython’s parser',
        ],
      },
      {
        title: 'The modern language',
        state: 'done',
        items: [
          'Exceptions with CPython’s built-in hierarchy',
          'Generators, yield from, send(); async/await',
          'match/case, every PEP 634 pattern kind',
          'Packages and relative imports',
        ],
      },
      {
        title: 'Two backends and a corpus',
        state: 'done',
        items: [
          'LLVM backend with JIT and REPL alongside the C backend',
          '76 of 77 shedskin programs compile and run; every comparable one but one matches CPython',
          'Demand-driven contour splitting, from minimum contours',
          'pyc -x: #! scripts with a content-hashed compile cache',
        ],
      },
    ],
  },
  {
    id: 'language',
    title: 'Language completeness',
    summary: 'Close the remaining gaps in the Python 3 subset that can be typed statically.',
    milestones: [
      {
        title: 'Calls and properties',
        state: 'active',
        items: [
          '**kwargs and keyword-only parameters (issues/182)',
          'Property setters and deleters (issues/007)',
          'Attribute access through cls in @classmethod (issues/184)',
        ],
      },
      {
        title: 'Numbers',
        state: 'planned',
        items: [
          'complex',
          'Integer overflow handled explicitly: trapped, or widened where the program demands it',
        ],
      },
      {
        title: 'The whole corpus, statically resolved',
        state: 'planned',
        items: [
          'othello3 within the compile budget',
          'Zero run-time dispatch: every call resolved statically',
        ],
      },
    ],
  },
  {
    id: 'libraries',
    title: 'Libraries',
    summary: 'Grow pyc_lib from 22 modules toward the part of the standard library real programs use.',
    milestones: [
      {
        title: 'Finish the modules that exist',
        state: 'active',
        items: [
          'itertools: chain, islice, permutations, combinations, groupby, accumulate',
          'functools: partial, lru_cache, cmp_to_key, total_ordering',
          'collections: Counter, OrderedDict, namedtuple',
          're: findall, finditer, sub, split, named groups',
        ],
      },
      {
        title: 'The modern idioms',
        state: 'planned',
        items: ['dataclasses', 'enum', 'typing (accepted, and checked under --strict)', 'bisect'],
      },
      {
        title: 'Applications',
        state: 'planned',
        items: [
          'json, pathlib',
          'asyncio front end over pyc’s event loop',
          'statistics, fractions, decimal',
          'unittest, so the same tests run compiled and under CPython',
        ],
      },
    ],
  },
  {
    id: 'strict',
    title: 'Strict dialect',
    summary: 'Turn --strict from a compiler mode into a specified, stable language.',
    milestones: [
      {
        title: 'The guarantee',
        state: 'done',
        items: [
          'CPython semantics or a compile error',
          'Every accommodation flagged, warned and off under --strict (issues/171)',
          'Strict suite and strict corpus sweep checked against CPython',
        ],
      },
      {
        title: 'The definition',
        state: 'active',
        items: [
          'A written specification of representable types and refusal rules',
          'Diagnostics that point at the confluence and suggest the fix',
          'The test suite and corpus published as a conformance suite',
        ],
      },
      {
        title: 'The language',
        state: 'planned',
        items: [
          'Checked annotations',
          'The whole standard library compiling under --strict',
          'A stability promise: accepted once, accepted with the same output after',
        ],
      },
    ],
  },
  {
    id: 'tooling',
    title: 'Tooling and distribution',
    summary: 'Make pyc easy to get, and use what it knows about your program in your editor.',
    milestones: [
      {
        title: 'Getting pyc',
        state: 'planned',
        items: ['Versioned releases', 'Binary packages for Linux and macOS', 'A container image'],
      },
      {
        title: 'Working with pyc',
        state: 'planned',
        items: [
          'A language server: inferred types on hover, refusals as live diagnostics',
          'An online playground on pyc-lang.org',
        ],
      },
    ],
  },
];
