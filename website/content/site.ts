export const SITE_URL = 'https://pyc-lang.org';
export const GITHUB = 'https://github.com/jplevyak/pyc';
export const DPARSER = 'https://github.com/jplevyak/dparser';
export const RELEASES = 'https://github.com/jplevyak/pyc/releases';

/** The current release: the v* tag the release workflow published. */
export const RELEASE = '0.1.1';
/** A release tarball's name, for os linux or darwin and its arch. */
export function releaseName(os: string, arch: string) {
  return `pyc-${RELEASE}-${os}-${arch}`;
}
/** A release tarball's download URL. */
export function releaseTarball(os: string, arch: string) {
  return `${GITHUB}/releases/download/v${RELEASE}/${releaseName(os, arch)}.tar.gz`;
}

/** A link into the repository, e.g. repo('blob/main/README.md'). */
export function repo(path: string) {
  return `${GITHUB}/${path}`;
}

/** Facts quoted on the site, with where each one comes from. Update together. */
export const FACTS = {
  // README.md "The shedskin corpus" (2026-11-10); sweep
  // check__default__c7badb43+dc450f88 (2026-11-14) agrees
  corpusTotal: 77,
  corpusCompile: 77,
  corpusRun: 77,
  corpusMatch: 59,
  // README.md "Testing": test_pyc.py on each backend (2026-11-14)
  e2ePass: 523,
  // pyc_lib/*.py
  libModules: 22,
};
