// Optional host regression: build.mjs first; mocks Qt/CEF transport, never deploys to a device.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const require = createRequire(import.meta.url);
const { chromium } = require(process.env.HUIWEN_PLAYWRIGHT || 'playwright');
const browser = await chromium.launch({ headless: true,
  ...(process.env.HUIWEN_CHROME ? { executablePath: process.env.HUIWEN_CHROME } : {}) });
try {
  const context = await browser.newContext();
  const html = fs.readFileSync(path.join(root, 'entry/src/main/resources/rawfile/markdown/preview.html'));
  const errors = [], network = [], messages = [];
  await context.route('**/*', route => {
    const url = new URL(route.request().url());
    if (url.origin === 'https://huiwen-md.invalid' && ['/editor.html', '/live.html'].includes(url.pathname))
      return route.fulfill({ status: 200, contentType: 'text/html', body: html });
    network.push(url.href);
    return route.abort();
  });
  const left = await context.newPage(), right = await context.newPage();
  const send = (page, message) => page.evaluate(value => window.HuiwenPreview.receive(value), message);
  let ready = false, revision = 0;
  for (const [page, editor] of [[left, true], [right, false]]) {
    page.on('pageerror', error => errors.push(error.message));
    await page.exposeFunction('hostMarkdown', async json => {
      const message = JSON.parse(json);
      messages.push({ editor, ...message });
      if (editor && message.type === 'dirty') revision = message.revision;
      if (ready && editor && ['change', 'snapshot', 'save'].includes(message.type))
        await send(right, { type: 'content', text: message.text, revision: message.revision });
      if (ready && message.type === 'position' && message.revision === revision)
        await send(editor ? right : left, message);
    });
    await page.addInitScript(() => {
      window.huiwenReportMarkdown = json => { void window.hostMarkdown(json); };
      window.huiwenReportPreview = state => { window.previewReady = state; };
    });
    await page.goto(`https://huiwen-md.invalid/${editor ? 'editor' : 'live'}.html`);
  }
  const source = '# Markdown 编辑\n\n' + Array.from({ length: 80 }, (_, i) =>
    `## 第 ${i + 1} 节\n\n正文 **强调** ${i}\n\n`).join('') +
    '\n```mermaid\nflowchart LR\nA-->B\n```\n';
  for (const page of [left, right]) {
    await page.evaluate(text => window.HuiwenPreview.showPreview(text), source);
    await page.waitForFunction(() => window.previewReady);
  }
  ready = true;
  await right.waitForFunction(() => document.querySelector('.mermaid-rendered'));
  await right.evaluate(() => { window.firstDiagram = document.querySelector('.mermaid-rendered'); });
  assert.ok(await right.evaluate(text => {
    const heading = document.querySelector('h1');
    window.HuiwenPreview.receive({ type: 'content', revision: 0, text });
    return heading === document.querySelector('h1');
  }, source));
  assert.equal(await left.locator('.cm-editor').count(), 1);
  assert.ok(await right.locator('[data-source-line]').count() > 80);
  assert.equal(await right.locator('.pdf-page-number').count(), 0);
  await left.locator('.cm-content').press('ControlOrMeta+Home');
  await left.keyboard.insertText('新增内容\n\n');
  await right.waitForFunction(() => document.querySelector('main').textContent.includes('新增内容'));
  await right.waitForFunction(() => window.firstDiagram === document.querySelector('.mermaid-rendered'));
  await left.locator('.cm-content').press('ControlOrMeta+z');
  await right.waitForFunction(() => !document.querySelector('main').textContent.includes('新增内容'));
  await left.locator('.cm-content').press('ControlOrMeta+s');
  await left.waitForTimeout(200);
  assert.ok(messages.some(m => m.type === 'save' && m.text.includes('Markdown 编辑')));
  await send(left, { type: 'snapshot', request: 7 });
  await left.waitForTimeout(100);
  assert.ok(messages.some(m => m.type === 'snapshot' && m.request === 7));
  assert.equal(await left.locator('.cm-content').getAttribute('contenteditable'), 'false');
  await send(left, { type: 'unlock' });
  assert.equal(await left.locator('.cm-content').getAttribute('contenteditable'), 'true');
  await left.locator('.cm-scroller').evaluate(node => { node.scrollTop = node.scrollHeight * 0.55; });
  await right.waitForFunction(() => window.scrollY > 100);
  await right.evaluate(() => window.scrollTo(0, 200));
  await left.waitForTimeout(150);
  assert.ok(messages.some(m => !m.editor && m.type === 'position'));
  await send(right, { type: 'content', revision: revision + 1, text: '# 最新内容' });
  await send(right, { type: 'content', revision: 0, text: '# 旧内容' });
  assert.equal(await right.locator('h1').textContent(), '最新内容');
  assert.deepEqual(errors, []);
  assert.deepEqual(network, []);
  console.log('MD_EDIT_CHECK_OK: two views, live render, history, save/snapshot, source scroll and stale revisions');
} finally {
  await browser.close();
}
