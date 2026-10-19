# pyc-lang.org

The project website: Next.js (App Router) and React, exported as a static site.

```sh
cd website
npm install
npm run dev      # http://localhost:3000
npm run build    # static site in out/, ready for any static host
```

`public/CNAME` names the domain, so `out/` deploys as-is to GitHub Pages. Netlify,
Cloudflare Pages or Vercel need only `npm run build` with `out` as the publish directory.

## Layout

| Path | What |
|---|---|
| `app/` | One directory per page (`install`, `learn`, `reference`, `libraries`, `strict`, `roadmap`, `community`) |
| `components/` | Header, footer, doc layout, and `Code`, a build-time Python/shell highlighter |
| `content/site.ts` | URLs, and the figures quoted on the home page, with where each comes from |
| `content/libraries.ts` | Status of every `pyc_lib/` module, and the wanted list |
| `content/roadmap.ts` | Roadmap tracks and milestones |
| `content/benchmarks.ts` | Measured timings, with the machine and commit they came from |

## Keeping it true

The site makes claims about the compiler. They go stale, so:

- **Every code example compiles with `pyc --strict` and prints what CPython prints.**
  Re-check after a compiler change, the same way the end-to-end suite does.
- **Figures** (corpus counts, test counts) come from [../README.md](../README.md) and
  `sweeps/`. Update `content/site.ts` when they change.
- **Library status** follows `pyc_lib/`. Adding a module means adding a row to
  `content/libraries.ts`.
- **Benchmarks** are re-measured, never edited by hand. Record the date, commit and machine.
