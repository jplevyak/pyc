import Link from 'next/link';

export default function NotFound() {
  return (
    <section className="page-hero">
      <div className="wrap">
        <h1>Not found</h1>
        <p>
          That page doesn&apos;t exist. <Link href="/">Back to the home page</Link>.
        </p>
      </div>
    </section>
  );
}
