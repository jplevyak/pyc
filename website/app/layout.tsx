import type { Metadata, Viewport } from 'next';
import { IBM_Plex_Sans, JetBrains_Mono } from 'next/font/google';
import { Header } from '@/components/Header';
import { Footer } from '@/components/Footer';
import { SITE_URL } from '@/content/site';
import './globals.css';

const plex = IBM_Plex_Sans({
  subsets: ['latin'],
  weight: ['400', '500', '600', '700'],
  variable: '--font-plex',
  display: 'swap',
});
const jbm = JetBrains_Mono({
  subsets: ['latin'],
  weight: ['400', '600'],
  variable: '--font-jbm',
  display: 'swap',
});

export const metadata: Metadata = {
  metadataBase: new URL(SITE_URL),
  title: {
    default: 'pyc — Python, compiled',
    template: '%s · pyc',
  },
  description:
    'pyc is a whole-program compiler for Python 3. It infers every type without annotations ' +
    'and emits a stand-alone native binary with CPython semantics, or refuses the program at ' +
    'compile time.',
  openGraph: {
    title: 'pyc — Python, compiled',
    description: 'Whole-program type inference for Python 3, compiled to native code.',
    url: SITE_URL,
    siteName: 'pyc',
    type: 'website',
  },
  icons: { icon: '/favicon.svg' },
};

export const viewport: Viewport = {
  width: 'device-width',
  initialScale: 1,
};

// Applied before paint so a stored theme choice never flashes.
const THEME_INIT = `try{var t=localStorage.getItem('theme');if(t==='light'||t==='dark')document.documentElement.setAttribute('data-theme',t)}catch(e){}`;

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className={`${plex.variable} ${jbm.variable}`} suppressHydrationWarning>
      <head>
        <script dangerouslySetInnerHTML={{ __html: THEME_INIT }} />
      </head>
      <body>
        <Header />
        <main>{children}</main>
        <Footer />
      </body>
    </html>
  );
}
