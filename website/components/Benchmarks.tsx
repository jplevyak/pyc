import { BENCHMARKS, BENCH_SETUP } from '@/content/benchmarks';

function fmt(ms: number) {
  return ms >= 1000 ? `${(ms / 1000).toFixed(1)} s` : `${ms} ms`;
}

export function Benchmarks() {
  const rows = [...BENCHMARKS].sort((a, b) => b.cpython / b.pyc - a.cpython / a.pyc);
  return (
    <>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Program</th>
              <th>What it does</th>
              <th style={{ textAlign: 'right' }}>CPython</th>
              <th style={{ textAlign: 'right' }}>pyc</th>
              <th style={{ textAlign: 'right' }}>Speedup</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((b) => (
              <tr key={b.name}>
                <td>
                  <code>{b.name}</code>
                </td>
                <td>{b.what}</td>
                <td style={{ textAlign: 'right', fontVariantNumeric: 'tabular-nums' }}>
                  {fmt(b.cpython)}
                </td>
                <td style={{ textAlign: 'right', fontVariantNumeric: 'tabular-nums' }}>
                  {fmt(b.pyc)}
                </td>
                <td
                  style={{
                    textAlign: 'right',
                    fontWeight: 600,
                    fontVariantNumeric: 'tabular-nums',
                    color: 'var(--accent-ink)',
                  }}
                >
                  {(b.cpython / b.pyc).toFixed(b.cpython / b.pyc >= 10 ? 0 : 1)}×
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <p className="small muted">{BENCH_SETUP}</p>
    </>
  );
}
