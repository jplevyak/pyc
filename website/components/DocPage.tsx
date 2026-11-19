import type { ReactNode } from 'react';

export function PageHero({ title, children }: { title: string; children?: ReactNode }) {
  return (
    <section className="page-hero">
      <div className="wrap">
        <h1>{title}</h1>
        {children && <p>{children}</p>}
      </div>
    </section>
  );
}

/** A two-column documentation layout: a sticky table of contents and the prose. */
export function DocLayout({
  toc,
  children,
}: {
  toc: { id: string; label: string }[];
  children: ReactNode;
}) {
  return (
    <div className="wrap doc">
      <aside className="toc">
        <div className="toc-title">On this page</div>
        <ol>
          {toc.map((t) => (
            <li key={t.id}>
              <a href={`#${t.id}`}>{t.label}</a>
            </li>
          ))}
        </ol>
      </aside>
      <article className="prose">{children}</article>
    </div>
  );
}

export function Callout({ title, children }: { title?: string; children: ReactNode }) {
  return (
    <div className="callout">
      {title && <strong>{title} </strong>}
      {children}
    </div>
  );
}
