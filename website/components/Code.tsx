import type { ReactNode } from 'react';

// A small, dependency-free highlighter. It runs at build time (server
// component), so the exported site ships no highlighting JavaScript.

const KEYWORDS = new Set(
  (
    'False None True and as assert async await break case class continue def del elif else ' +
    'except finally for from global if import in is lambda match nonlocal not or pass raise ' +
    'return try while with yield'
  ).split(' '),
);
const BUILTINS = new Set(
  (
    'print len range int float str bool list dict set tuple bytes bytearray super object ' +
    'isinstance enumerate zip map filter sorted min max sum abs open iter next self'
  ).split(' '),
);

const PY_TOKEN =
  /(#[^\n]*)|([rRbBfFuU]{0,2}("""[\s\S]*?"""|'''[\s\S]*?'''|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'))|(\b\d[\d_]*(?:\.\d+)?(?:[eE][+-]?\d+)?\b)|(@?[A-Za-z_]\w*)/g;

function highlightPython(src: string): ReactNode[] {
  const out: ReactNode[] = [];
  let last = 0;
  let m: RegExpExecArray | null;
  let prevWord = '';
  let i = 0;
  PY_TOKEN.lastIndex = 0;
  while ((m = PY_TOKEN.exec(src))) {
    if (m.index > last) out.push(src.slice(last, m.index));
    const text = m[0];
    let cls: string | null = null;
    if (m[1]) cls = 'tok-com';
    else if (m[2]) cls = 'tok-str';
    else if (m[4]) cls = 'tok-num';
    else if (m[5]) {
      if (text.startsWith('@')) cls = 'tok-fn';
      else if (KEYWORDS.has(text)) cls = 'tok-kw';
      else if (prevWord === 'def' || prevWord === 'class') cls = 'tok-fn';
      else if (BUILTINS.has(text)) cls = 'tok-bi';
      prevWord = text;
    }
    out.push(cls ? <span key={i++} className={cls}>{text}</span> : text);
    last = m.index + text.length;
  }
  if (last < src.length) out.push(src.slice(last));
  return out;
}

// Shell transcripts: `$ cmd` lines get a prompt; lines starting with
// `!ok `, `!warn `, `!err `, `!dim ` are coloured output.
function highlightShell(src: string): ReactNode[] {
  return src.split('\n').map((line, n) => {
    const nl = n > 0 ? '\n' : '';
    if (line.startsWith('$ ')) {
      return (
        <span key={n}>
          {nl}
          <span className="tok-prompt">$ </span>
          {line.slice(2)}
        </span>
      );
    }
    const tag = /^!(ok|warn|err|dim) /.exec(line);
    if (tag) {
      return (
        <span key={n}>
          {nl}
          <span className={`tok-${tag[1]}`}>{line.slice(tag[0].length)}</span>
        </span>
      );
    }
    if (line.trimStart().startsWith('#')) {
      return (
        <span key={n}>
          {nl}
          <span className="tok-com">{line}</span>
        </span>
      );
    }
    return (
      <span key={n}>
        {nl}
        {line}
      </span>
    );
  });
}

export function Code({
  children,
  lang = 'python',
  title,
  note,
}: {
  children: string;
  lang?: 'python' | 'shell' | 'text';
  title?: string;
  note?: string;
}) {
  const src = children.replace(/^\n/, '').replace(/\s+$/, '');
  const body =
    lang === 'python' ? highlightPython(src) : lang === 'shell' ? highlightShell(src) : src;
  return (
    <div className="code">
      {(title || note) && (
        <div className="code-title">
          <span>{title}</span>
          {note && <span>{note}</span>}
        </div>
      )}
      <pre>
        <code>{body}</code>
      </pre>
    </div>
  );
}
