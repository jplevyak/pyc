import type { Metadata } from 'next';
import Link from 'next/link';
import { DocLayout, PageHero } from '@/components/DocPage';
import { TRACKS } from '@/content/roadmap';
import { repo } from '@/content/site';

export const metadata: Metadata = {
  title: 'Roadmap',
  description:
    'Where pyc is going: language completeness, a larger standard library, the strict dialect, and tooling.',
};

const LABEL = { done: 'done', active: 'next', planned: 'planned' } as const;
const PILL = { done: 'ok', active: 'partial', planned: 'planned' } as const;

export default function Roadmap() {
  return (
    <>
      <PageHero title="Roadmap">
        The compiler core is in place. The next phase is breadth and definition: more of the
        standard library, and a strict mode developed into a fully realized dialect of Python.
      </PageHero>
      <DocLayout toc={TRACKS.map((t) => ({ id: t.id, label: t.title }))}>
        {TRACKS.map((t) => (
          <section key={t.id}>
            <h2 id={t.id}>{t.title}</h2>
            <p>{t.summary}</p>
            <div className="track">
              {t.milestones.map((m) => (
                <div className="milestone" data-state={m.state} key={m.title}>
                  <h3>
                    {m.title} <span className={`pill ${PILL[m.state]}`}>{LABEL[m.state]}</span>
                  </h3>
                  {m.when && <div className="when">{m.when}</div>}
                  <ul>
                    {m.items.map((i) => (
                      <li key={i}>{i}</li>
                    ))}
                  </ul>
                </div>
              ))}
            </div>
          </section>
        ))}
        <p>
          Detailed work items, with symptoms, root causes and verification plans, live in the
          repository&apos;s <a href={repo('tree/master/issues')}>issues/</a> and{' '}
          <a href={repo('tree/master/ifa/issues')}>ifa/issues/</a> directories. To help, see{' '}
          <Link href="/community/">Community</Link>.
        </p>
      </DocLayout>
    </>
  );
}
