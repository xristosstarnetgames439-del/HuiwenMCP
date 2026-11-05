// Two browser views share source positions and revisions through Qt, not PDF page numbers.
import { EditorState, Compartment } from '@codemirror/state';
import { EditorView, keymap, lineNumbers } from '@codemirror/view';
import { markdown } from '@codemirror/lang-markdown';
import { history, historyKeymap, defaultKeymap, indentWithTab } from '@codemirror/commands';
import { defaultHighlightStyle, syntaxHighlighting } from '@codemirror/language';
import { renderSourceMarkdown, renderDiagrams } from './source/rich-markdown.ts';

const clamp = (v, max = 1) => Math.max(0, Math.min(max, v));
const report = value => window.huiwenReportMarkdown?.(JSON.stringify(value));
let editor, body, revision = 0, lines = 1, timer, observer, frame;
let appliedTop = null, lastPosition = null, pendingPosition = null;
let currentSource, cachedAnchors;
const editability = new Compartment();
const isEditor = () => location.pathname === '/editor.html';
const scroller = () => editor?.scrollDOM || document.scrollingElement;
const range = () => Math.max(0, scroller().scrollHeight - scroller().clientHeight);

function anchors(value, axis) {
  cachedAnchors ||= [...body.querySelectorAll('.md-source-block')].map(node => {
    const rect = node.getBoundingClientRect();
    return { line: Number(node.dataset.sourceLine), top: rect.top + window.scrollY, height: rect.height };
  });
  let i = 0;
  while (i + 1 < cachedAnchors.length && cachedAnchors[i + 1][axis] <= value) i++;
  const a = cachedAnchors[i];
  return [a, cachedAnchors[i + 1] || (a && { line: lines + 1, top: a.top + a.height })];
}

function position() {
  const top = scroller().scrollTop;
  let value = 1;
  if (editor) {
    const y = editor.scrollDOM.getBoundingClientRect().top - editor.documentTop;
    const block = editor.lineBlockAtHeight(Math.max(0, y));
    value = editor.state.doc.lineAt(block.from).number + clamp((y - block.top) / Math.max(1, block.height));
  } else {
    const [a, b] = anchors(top, 'top');
    if (a) {
      value = a.line + clamp((top - a.top) / Math.max(1, b.top - a.top)) * Math.max(1, b.line - a.line);
    }
  }
  return { type: 'position', revision, line: Math.floor(value), fraction: value % 1,
    progress: top / Math.max(1, range()) };
}

function scrollToSource(p) {
  if (p.revision !== revision) return;
  let top = clamp(p.progress) * range();
  if (p.progress > 0 && p.progress < 1) {
    if (editor) {
      const line = editor.state.doc.line(Math.max(1, Math.min(editor.state.doc.lines, p.line)));
      const block = editor.lineBlockAt(line.from);
      top = scroller().scrollTop + editor.documentTop - scroller().getBoundingClientRect().top +
        block.top + clamp(p.fraction) * block.height;
    } else {
      const value = p.line + clamp(p.fraction);
      const [a, b] = anchors(value, 'line');
      if (a) {
        top = a.top + clamp((value - a.line) / Math.max(1, b.line - a.line)) * Math.max(1, b.top - a.top);
      }
    }
  }
  scroller().scrollTop = clamp(top, range());
  appliedTop = scroller().scrollTop;
  lastPosition = position();
}

function queuePosition() {
  if (frame) return;
  frame = requestAnimationFrame(() => {
    frame = null;
    if (appliedTop !== null && Math.abs(scroller().scrollTop - appliedTop) < 1) return;
    appliedTop = null;
    lastPosition = position();
    report(lastPosition);
  });
}

function snapshot(type = 'change', request = 0) {
  clearTimeout(timer);
  const text = editor.state.doc.toString();
  if (new TextEncoder().encode(text).length > 16 * 1024 * 1024)
    report({ type: 'tooLarge', revision, request });
  else report({ type, text, revision, request });
}

function render(source, nextRevision) {
  const old = lastPosition || position();
  revision = nextRevision;
  if (source !== currentSource) {
    currentSource = source;
    lines = source.split('\n').length;
    observer?.disconnect();
    const reusable = new Map();
    for (const node of body.querySelectorAll('.mermaid-rendered')) {
      const key = node.dataset.mermaidSource;
      if (!reusable.has(key)) reusable.set(key, []);
      reusable.get(key).push(node);
    }
    body.innerHTML = renderSourceMarkdown(source, {
      imageUrl: path => `https://huiwen-md.invalid/image?path=${encodeURIComponent(path)}`
    });
    cachedAnchors = null;
    scrollToSource({ ...old, revision });
    observer?.observe(body);
    for (const node of body.querySelectorAll('.md-source-block')) observer?.observe(node);
    void renderDiagrams(body, reusable);
  }
  if (lastPosition) lastPosition.revision = revision;
  if (pendingPosition?.revision === revision) {
    scrollToSource(pendingPosition); pendingPosition = null;
  }
}

export function receiveWorkspace(message) {
  if (!body) return;
  if (message.type === 'content' && !isEditor() && message.revision >= revision) render(message.text, message.revision);
  if (message.type === 'position') {
    if (message.revision === revision) scrollToSource(message);
    else if (!isEditor() && message.revision > revision) pendingPosition = message;
  }
  if (message.type === 'publish') report(position());
  if (message.type === 'snapshot' && editor) {
    editor.dispatch({ effects: editability.reconfigure(EditorView.editable.of(false)) });
    snapshot('snapshot', message.request);
  }
  if (message.type === 'unlock' && editor)
    editor.dispatch({ effects: editability.reconfigure(EditorView.editable.of(true)) });
}

function saveShortcut(event) {
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') {
    event.preventDefault(); report({ type: 'saveRequest', revision });
  }
}

export function showWorkspace(source, showBrokenImage, copyCode) {
  editor?.destroy();
  clearTimeout(timer);
  revision = 0;
  currentSource = undefined;
  lastPosition = pendingPosition = cachedAnchors = null;
  body = document.getElementById('markdown-body');
  body.classList.toggle('markdown-body', !isEditor());
  body.classList.toggle('markdown-editor', isEditor());
  body.innerHTML = '';
  if (isEditor()) {
    const save = () => { snapshot('save'); return true; };
    editor = new EditorView({ parent: body, state: EditorState.create({ doc: source, extensions: [
      markdown(), lineNumbers(), history(), syntaxHighlighting(defaultHighlightStyle), EditorView.lineWrapping,
      editability.of(EditorView.editable.of(true)),
      keymap.of([{ key: 'Mod-s', run: save }, ...defaultKeymap, ...historyKeymap, indentWithTab]),
      EditorView.updateListener.of(update => {
        if (!update.docChanged) return;
        revision++;
        report({ type: 'dirty', revision });
        clearTimeout(timer);
        const flush = () => { if (editor.composing) timer = setTimeout(flush, 150); else snapshot(); };
        timer = setTimeout(flush, 150);
      })
    ] }) });
    editor.scrollDOM.addEventListener('scroll', queuePosition, { passive: true });
    editor.focus();
  } else {
    editor = null;
    observer ||= new ResizeObserver(() => {
      cachedAnchors = null;
      if (lastPosition) scrollToSource(lastPosition);
    });
    render(source, 0);
    window.addEventListener('scroll', queuePosition, { passive: true });
    window.addEventListener('keydown', saveShortcut);
    body.addEventListener('click', copyCode);
    body.addEventListener('error', showBrokenImage, true);
  }
  window.huiwenReportPreview?.(true, '');
}
