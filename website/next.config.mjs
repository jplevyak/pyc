/** @type {import('next').NextConfig} */
const nextConfig = {
  // Static export: `npm run build` writes a self-contained site to out/,
  // deployable to pyc-lang.org on any static host.
  output: 'export',
  trailingSlash: true,
  images: { unoptimized: true },
};

export default nextConfig;
