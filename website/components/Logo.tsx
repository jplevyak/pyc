// The mark: a Python-ish brace folded into a downward "compile" chevron.
export function Logo({ className }: { className?: string }) {
  return (
    <svg className={className} viewBox="0 0 32 32" aria-hidden="true">
      <rect width="32" height="32" rx="7" fill="var(--accent)" />
      <path
        d="M12 8.5c-2.2 0-3 1-3 3v2.2c0 1.2-.7 2-2 2.3 1.3.3 2 1.1 2 2.3v2.2c0 2 .8 3 3 3"
        fill="none"
        stroke="#fff"
        strokeWidth="2.2"
        strokeLinecap="round"
      />
      <path
        d="M16 12.5l4.5 3.5-4.5 3.5M21.5 21h4"
        fill="none"
        stroke="#fff"
        strokeWidth="2.2"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );
}
