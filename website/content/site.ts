export const SITE_URL = 'https://pyc-lang.org';
export const GITHUB = 'https://github.com/jplevyak/pyc';
export const DPARSER = 'https://github.com/jplevyak/dparser';

/** A link into the repository, e.g. repo('blob/main/README.md'). */
export function repo(path: string) {
  return `${GITHUB}/${path}`;
}

/** Facts quoted on the site, with where each one comes from. Update together. */
export const FACTS = {
  // README.md "The shedskin corpus", sweep check__default__dffcdab6 (2026-10-18)
  corpusTotal: 77,
  corpusCompile: 76,
  corpusRun: 76,
  corpusMatch: 58,
  // README.md "Testing": test_pyc.py on each backend
  e2ePass: 488,
  // pyc_lib/*.py
  libModules: 22,
};
