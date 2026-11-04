// Run locally after build.mjs; no HAP/device and no external network access.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
import os from 'node:os';

const require = createRequire(import.meta.url);
const { chromium } = require(process.env.HUIWEN_PLAYWRIGHT || 'playwright');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const browser = await chromium.launch({ headless: true,
  ...(process.env.HUIWEN_CHROME ? { executablePath: process.env.HUIWEN_CHROME } : {}) });
const fixtureDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'huiwen-image-check-'));
try {
  const page = await browser.newPage({ viewport: { width: 1000, height: 800 } });
  const errors = [];
  const network = [];
  page.on('pageerror', error => errors.push(error.message));
  let imageRoot = fixtureDirectory;
  const imageRequests = [];
  const previewUrl = 'https://huiwen-md.invalid/preview.html';
  let pageRequests = 0;
  await page.addInitScript(() => {
    window.previewReports = [];
    window.huiwenReportPreview = (ready, error) => window.previewReports.push({ ready, error });
  });
  // Host-only substitute for the CEF resource callback, not a network server.
  await page.route(/^https?:/, async route => {
    const url = new URL(route.request().url());
    if (url.origin !== 'https://huiwen-md.invalid') {
      network.push(url.href); return route.abort();
    }
    if (url.href === previewUrl && route.request().resourceType() === 'document') {
      pageRequests++;
      return route.fulfill({ status: 200, contentType: 'text/html',
        body: fs.readFileSync(path.join(root, 'entry/src/main/resources/rawfile/markdown/preview.html')) });
    }
    if (url.pathname !== '/image') return route.fulfill({ status: 404, body: '' });
    const relative = url.searchParams.get('path') || '';
    imageRequests.push(relative);
    const file = path.resolve(imageRoot, relative);
    if (!file.startsWith(imageRoot + path.sep) || !fs.existsSync(file)) return route.fulfill({ status: 404, body: '' });
    return route.fulfill({ status: 200, contentType: /\.png$/i.test(file) ? 'image/png' : 'image/jpeg', body: fs.readFileSync(file) });
  });
  await page.goto(previewUrl);
  assert.equal(pageRequests, 1);
  assert.equal(page.url(), previewUrl);
  await page.evaluate(() => window.HuiwenPreview.showPreview('# 加载就绪'));
  assert.deepEqual(await page.evaluate(() => window.previewReports), [{ ready: true, error: '' }]);
  const fixture = `<!-- PDF page 1 -->

~~~pdf-watermark
{"text":"水印<script>测试</script>","angle":45}
~~~

# 标题

**粗体** *斜体* ~~删除~~ ==高亮== H~2~O x^2^

- [x] 完成
- [ ] 待办

| 表头 | 内容 |
| --- | --- |
| 一行<br>二行 | **强调** |

\`\`\`python
print("<script>")
\`\`\`

\`\`\`mermaid
flowchart LR
  A[PDF] --> B[Markdown]
\`\`\`

\`\`\`mermaid
sequenceDiagram
  Client->>Server: Hello
\`\`\`

\`\`\`mermaid
this is not a diagram
\`\`\`

公式 $E=mc^2$

数组 &#91;1, 'apple', True&#93;

$$\\frac{a}{b}$$

\\(x^2\\)

脚注[^1]

[^1]: 脚注正文

~~~pdf-logo
{"image":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLttAAAAABJRU5ErkJggg==","alt":"测试logo"}
~~~

<!-- PDF page 2 -->

<small>页脚 &amp; 正文</small>

<script>window.pwned=true</script>

[x](javascript:alert(1))

~~~pdf-logo
{"image":"javascript:alert(1)"}
~~~

\`\`\`text
<!-- PDF page 99 -->
\`\`\`
`;
  await page.evaluate(source => window.HuiwenPreview.showPreview(source), fixture);
  await page.waitForFunction(() => document.querySelectorAll('.mermaid-rendered svg').length === 2 &&
    document.querySelectorAll('.diagram-error').length === 1);
  const result = await page.evaluate(() => {
    const body = document.getElementById('markdown-body');
    const first = body.querySelector('.pdf-page');
    return {
      pages: [...body.querySelectorAll('.pdf-page')].map(n => n.dataset.pdfPage),
      watermarks: body.querySelectorAll('.pdf-watermark').length,
      logos: body.querySelectorAll('.pdf-logo').length,
      logoAfterPageNumber: Boolean(first.querySelector('.pdf-page-number + .pdf-logo')),
      firstChildWatermark: first.firstElementChild.classList.contains('pdf-watermark'),
      math: body.querySelectorAll('.katex').length,
      footnote: body.querySelectorAll('.footnote-item').length,
      tasks: body.querySelectorAll('input[type=checkbox][disabled]').length,
      highlight: body.querySelectorAll('.hljs-built_in').length,
      tableLineBreak: body.querySelectorAll('td br').length,
      literalArray: body.textContent.includes("数组 [1, 'apple', True]"),
      footer: body.querySelector('small').textContent,
      injected: Boolean(window.pwned || body.querySelector('script, iframe, a[href^="javascript:"]'))
    };
  });
  assert.deepEqual(result.pages, ['1', '2']);
  for (const key of ['logoAfterPageNumber', 'firstChildWatermark']) assert.equal(result[key], true, key);
  for (const key of ['watermarks', 'logos', 'footnote', 'tableLineBreak']) assert.equal(result[key], 1, key);
  assert.equal(result.math, 3); assert.equal(result.tasks, 2); assert.ok(result.highlight > 0);
  assert.equal(result.literalArray, true);
  assert.equal(result.footer, '页脚 & 正文'); assert.equal(result.injected, false);
  fs.writeFileSync(path.join(fixtureDirectory, '中文 图.png'), Buffer.from(
    'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLttAAAAABJRU5ErkJggg==', 'base64'));
  fs.writeFileSync(path.join(fixtureDirectory, 'broken.jpg'), 'not an image');
  await page.evaluate(() => window.HuiwenPreview.showPreview(
    '<!-- PDF page 1 -->\n\n![正常](<中文 图.png>)\n\n![丢失](missing.png)\n\n![损坏](broken.jpg)\n\n![越界](../outside.png)\n\n' +
    '~~~pdf-logo\n{"image":"中文 图.png","alt":"页尾Logo"}\n~~~'));
  await page.waitForFunction(() => document.querySelectorAll('.md-image-error').length === 3);
  assert.equal(await page.locator('.pdf-page-number + .pdf-logo').count(), 1);
  const good = await page.locator('img[alt="正常"]').evaluate(i => i.complete && i.naturalWidth > 0);
  assert.equal(good, true);
  assert.ok(imageRequests.includes('中文 图.png'));
  assert.ok(!imageRequests.some(name => name.includes('..')));
  assert.match(await page.locator('.md-image-error').allTextContents().then(a => a.join(' ')), /missing\.png/);
  if (process.env.HUIWEN_IMAGE_SCREENSHOT) await page.screenshot({path: process.env.HUIWEN_IMAGE_SCREENSHOT, fullPage: true});
  if (process.env.HUIWEN_TEST_MARKDOWN) {
    const document = path.resolve(process.env.HUIWEN_TEST_MARKDOWN);
    imageRoot = path.dirname(document);
    await page.evaluate(source => window.HuiwenPreview.showPreview(source), fs.readFileSync(document, 'utf8'));
    // Force all generated document images to load for this check, including lazy/offscreen ones.
    await page.evaluate(() => { for (const image of document.querySelectorAll('img')) image.loading = 'eager'; });
    await page.waitForFunction(() => [...document.images].every(i => i.complete));
    assert.equal(await page.locator('.md-image-error').count(), 0);
    assert.ok(await page.locator('img').count() > 0);
    console.log('ACTUAL_MD_IMAGES_OK', await page.locator('img').count());
  }
  // Re-render while an asynchronous Mermaid job is queued; old jobs must not overwrite the new body.
  await page.evaluate(() => {
    window.HuiwenPreview.showPreview('```mermaid\nflowchart LR\nA-->B\n```');
    window.HuiwenPreview.showPreview('# 新文档');
  });
  await page.waitForTimeout(100);
  assert.equal(await page.locator('#markdown-body').textContent(), '新文档\n');
  // A tall two-page fixture verifies the existing page-based/fraction fallback interface.
  await page.evaluate(() => window.HuiwenPreview.showPreview(
    '<!-- PDF page 1 -->\n\n' + ('正文\n\n'.repeat(100)) +
    '<!-- PDF page 2 -->\n\n' + ('第二页\n\n'.repeat(100))));
  await page.evaluate(() => window.HuiwenPreview.setScrollPosition(2, 0.2, 0.6));
  const position = await page.evaluate(() => window.HuiwenPreview.scrollPosition());
  assert.equal(position.page, 2); assert.ok(Math.abs(position.fraction - 0.2) < 0.01);
  assert.deepEqual(errors, []); assert.deepEqual(network, []);
  if (process.env.HUIWEN_PREVIEW_SCREENSHOT) {
    await page.evaluate(source => window.HuiwenPreview.showPreview(source), fixture);
    await page.waitForFunction(() => document.querySelectorAll('.mermaid-rendered svg').length === 2);
    await page.screenshot({ path: process.env.HUIWEN_PREVIEW_SCREENSHOT, fullPage: true });
  }
  console.log('RICH_PREVIEW_CHECK_OK: formats, offline assets, XSS, stale diagrams, page order and scroll');
} finally {
  await browser.close();
  fs.rmSync(fixtureDirectory, { recursive: true, force: true });
}
