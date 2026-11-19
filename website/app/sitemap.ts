import type { MetadataRoute } from 'next';
import { SITE_URL } from '@/content/site';

export const dynamic = 'force-static';

const PAGES = ['', 'install', 'learn', 'reference', 'libraries', 'strict', 'roadmap', 'community'];

export default function sitemap(): MetadataRoute.Sitemap {
  return PAGES.map((p) => ({ url: `${SITE_URL}/${p ? p + '/' : ''}` }));
}
