'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { useState } from 'react';
import { Logo } from './Logo';
import { GITHUB } from '@/content/site';

const NAV = [
  { href: '/install/', label: 'Install' },
  { href: '/learn/', label: 'Learn' },
  { href: '/reference/', label: 'Reference' },
  { href: '/libraries/', label: 'Libraries' },
  { href: '/strict/', label: 'Strict' },
  { href: '/roadmap/', label: 'Roadmap' },
  { href: '/community/', label: 'Community' },
];

export function Header() {
  const path = usePathname();
  const [open, setOpen] = useState(false);
  return (
    <header className="site-header">
      <div className="wrap">
        <Link href="/" className="brand" onClick={() => setOpen(false)}>
          <Logo className="brand-mark" />
          pyc
        </Link>
        <button
          className="menu-toggle"
          aria-expanded={open}
          aria-controls="site-nav"
          onClick={() => setOpen(!open)}
        >
          Menu
        </button>
        <nav id="site-nav" className="nav" data-open={open}>
          {NAV.map((n) => (
            <Link
              key={n.href}
              href={n.href}
              aria-current={path?.startsWith(n.href.slice(0, -1)) ? 'page' : undefined}
              onClick={() => setOpen(false)}
            >
              {n.label}
            </Link>
          ))}
          <a className="gh" href={GITHUB}>
            GitHub
          </a>
        </nav>
      </div>
    </header>
  );
}
