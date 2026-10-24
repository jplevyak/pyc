import Link from 'next/link';
import { ThemeButton } from './ThemeButton';
import { GITHUB, repo } from '@/content/site';

export function Footer() {
  return (
    <footer className="site-footer">
      <div className="wrap footer-grid">
        <div>
          <strong style={{ color: 'var(--ink)' }}>pyc</strong> compiles Python 3 to native code.
          <br />
          BSD 3-Clause licensed. © John Bradley Plevyak.
          <br />
          <ThemeButton />
        </div>
        <div>
          <h4>Get started</h4>
          <ul>
            <li>
              <Link href="/install/">Install</Link>
            </li>
            <li>
              <Link href="/learn/">Tour of pyc</Link>
            </li>
            <li>
              <Link href="/reference/">Reference</Link>
            </li>
          </ul>
        </div>
        <div>
          <h4>Project</h4>
          <ul>
            <li>
              <Link href="/libraries/">Libraries</Link>
            </li>
            <li>
              <Link href="/strict/">Strict dialect</Link>
            </li>
            <li>
              <Link href="/roadmap/">Roadmap</Link>
            </li>
          </ul>
        </div>
        <div>
          <h4>Source</h4>
          <ul>
            <li>
              <a href={GITHUB}>GitHub</a>
            </li>
            <li>
              <a href={repo('tree/main/issues')}>Issues</a>
            </li>
            <li>
              <Link href="/community/">Contributing</Link>
            </li>
          </ul>
        </div>
      </div>
    </footer>
  );
}
