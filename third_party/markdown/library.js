// 编辑能力保持独立；只读预览在内存中渲染 MD 原文，不另存每篇文档的 HTML。
import './library.css';
import { createMarkdownRenderer, renderDiagrams } from './source/rich-markdown.ts';
import { showWorkspace as setupWorkspace } from './workspace.js';
export { receiveWorkspace } from './workspace.js';
export function showWorkspace(source) { setupWorkspace(source, showBrokenImage, copyCode); }

export { EditorState } from '@codemirror/state';
export { EditorView } from '@codemirror/view';
export { markdown, markdownLanguage } from '@codemirror/lang-markdown';
export { parser, GFM } from '@lezer/markdown';

const renderer = createMarkdownRenderer();
const imageEnvironment = () => ({
  imageUrl: path => `https://huiwen-md.invalid/image?path=${encodeURIComponent(path)}`
});

export function renderMarkdown(source) {
  return renderer.render(source, imageEnvironment());
}

// 只识别顶层独立段落中的页码，代码块、列表和普通正文中的同样文字不作锚点。
export function renderPreview(source) {
  const environment = imageEnvironment();
  const tokens = renderer.parse(source, environment);
  const markers = [];
  for (let i = 0; i < tokens.length - 2; i++) {
    if (tokens[i].type !== 'paragraph_open' || tokens[i].level !== 0 ||
        tokens[i + 1].type !== 'inline' || tokens[i + 2].type !== 'paragraph_close') continue;
    const match = /^<!-- PDF page ([1-9]\d{0,5}) -->$/.exec(tokens[i + 1].content.trim());
    if (match && Number(match[1]) <= 100000) markers.push({ index: i, page: Number(match[1]) });
  }
  if (!markers.length || markers.some((marker, i) => i && marker.page <= markers[i - 1].page)) {
    return renderMarkdown(source);
  }
  const render = (start, end) => renderer.renderer.render(tokens.slice(start, end), renderer.options, environment);
  return render(0, markers[0].index) + markers.map((marker, i) => {
    const pageTokens = tokens.slice(marker.index + 3, markers[i + 1]?.index ?? tokens.length);
    const logos = pageTokens.filter(token => token.type === 'fence' && token.level === 0 &&
      token.info.trim() === 'pdf-logo');
    const body = pageTokens.filter(token => !logos.includes(token));
    const renderTokens = list => renderer.renderer.render(list, renderer.options, environment);
    return `<section class="pdf-page" data-pdf-page="${marker.page}">` + renderTokens(body) +
      `<div class="pdf-page-number">--第${marker.page}页--</div>` + renderTokens(logos) + '</section>';
  }).join('');
}

let pages = [];
let pageIndexes = new Map();
let listening = false;
let framePending = false;
let observer;
let appliedTop = null;
let lastPosition;
let initialReport = false;
const clamp = (value) => Math.max(0, Math.min(1, value));
const scrollRange = () => Math.max(0, document.documentElement.scrollHeight - window.innerHeight);
const pageTop = (page) => page.getBoundingClientRect().top + window.scrollY;

export function scrollPosition() {
  const top = window.scrollY;
  const range = scrollRange();
  let low = 0;
  let high = pages.length;
  while (low < high) {
    const middle = (low + high) >>> 1;
    if (pageTop(pages[middle]) <= top) low = middle + 1;
    else high = middle;
  }
  const index = Math.max(0, low - 1);
  const page = pages[index];
  const start = page ? pageTop(page) : 0;
  const end = pages[index + 1] ? pageTop(pages[index + 1]) :
    start + (page?.getBoundingClientRect().height ?? 1);
  return {
    page: page ? Number(page.dataset.pdfPage) : 0,
    fraction: clamp((top - start) / Math.max(1, end - start)),
    progress: range ? clamp(top / range) : 0
  };
}

export function setScrollPosition(page, fraction, progress) {
  if (![page, fraction, progress].every(Number.isFinite)) return;
  const range = scrollRange();
  const index = pageIndexes.get(page) ?? -1;
  let target = clamp(progress) * range;
  if (index >= 0 && progress > 0 && progress < 1) {
    const start = pageTop(pages[index]);
    const end = pages[index + 1] ? pageTop(pages[index + 1]) :
      start + pages[index].getBoundingClientRect().height;
    target = start + clamp(fraction) * Math.max(1, end - start);
  }
  window.scrollTo({ top: Math.max(0, Math.min(range, target)), behavior: 'instant' });
  // 保留目标值直至实际位置变化，异步 scroll 回调不能再次反向通知 Qt。
  appliedTop = window.scrollY;
  lastPosition = scrollPosition();
}

function publishPosition() {
  framePending = false;
  lastPosition = scrollPosition();
  if (!initialReport && appliedTop !== null && Math.abs(window.scrollY - appliedTop) < 1) return;
  initialReport = false;
  appliedTop = null;
  const { page, fraction, progress } = lastPosition;
  window.huiwenReportScroll?.(page, fraction, progress);
}

function queuePosition() {
  if (framePending) return;
  framePending = true;
  window.requestAnimationFrame(publishPosition);
}

function restorePosition() {
  if (lastPosition) {
    const { page, fraction, progress } = lastPosition;
    setScrollPosition(page, fraction, progress);
  }
}

function showBrokenImage(image) {
  if (image instanceof Event) image = image.target;
  if (!(image instanceof HTMLImageElement) || !image.isConnected) return;
  const placeholder = document.createElement('span');
  placeholder.className = 'md-image-error';
  placeholder.setAttribute('role', 'img');
  const raw = image.getAttribute('data-image-path') || '';
  let filename = raw;
  try { filename = decodeURIComponent(raw); } catch { /* retain malformed path for diagnosis */ }
  filename = filename.startsWith('data:') ? '内嵌图片' : filename.split('/').pop();
  const label = `${image.alt || '图片'}加载失败：${filename || '路径无效'}`;
  placeholder.setAttribute('aria-label', label);
  placeholder.title = label;
  placeholder.innerHTML = '<svg width="28" height="28" viewBox="0 0 28 28" aria-hidden="true">' +
    '<path d="M12 3H3v22h9m4-22h9v22h-9M15 2l-4 8 6 5-5 12M4 20l5-5 3 3m5 1 4-5 3 5" ' +
    'fill="none" stroke="currentColor" stroke-width="1.5"/></svg>';
  placeholder.append(document.createTextNode(label));
  image.replaceWith(placeholder);
}

async function copyCode(event) {
  const button = event.target.closest?.('.md-copy-code');
  if (!button) return;
  const text = button.closest('.md-code-block')?.querySelector('code')?.textContent;
  if (text === undefined) return;
  try {
    if (navigator.clipboard?.writeText) await navigator.clipboard.writeText(text);
    else {
      const field = document.createElement('textarea');
      field.value = text;
      field.style.cssText = 'position:fixed;opacity:0;pointer-events:none';
      document.body.append(field);
      try { field.select(); if (!document.execCommand('copy')) throw new Error('copy'); }
      finally { field.remove(); }
    }
    button.textContent = '已复制';
  } catch { button.textContent = '请选中代码复制'; }
}

export function showPreview(source) {
  const body = document.getElementById('markdown-body');
  observer?.disconnect();
  // Error does not bubble, so use capture and install before assigning image src attributes.
  if (!listening) body.addEventListener('error', event => showBrokenImage(event.target), true);
  body.innerHTML = renderPreview(source);
  for (const image of body.querySelectorAll('img[data-image-path]')) {
    if (!image.getAttribute('src')) showBrokenImage(image);
  }
  pages = [...body.querySelectorAll('.pdf-page')];
  pageIndexes = new Map(pages.map((page, i) => [Number(page.dataset.pdfPage), i]));
  window.scrollTo(0, 0);
  appliedTop = null;
  initialReport = true;
  lastPosition = scrollPosition();
  if (!listening) {
    window.addEventListener('scroll', queuePosition, { passive: true });
    window.addEventListener('resize', restorePosition);
    body.addEventListener('click', copyCode);
    listening = true;
  }
  if (typeof ResizeObserver !== 'undefined') {
    observer = new ResizeObserver(restorePosition);
    observer.observe(body);
  }
  // 首次通知也作为正文已渲染的握手，原生端随后才开启同步。
  queuePosition();
  // Diagram/font/image reflow goes through the existing ResizeObserver and page anchors.
  void renderDiagrams(body);
}
