// 离线库及 DOCX 只读预览入口；只在 showPreview 收到文档后挂载。
import './source/src/styles/editor.css';
import './preview.css';
import * as React from 'react';
import { createRoot, type Root } from 'react-dom/client';
import { DocxEditor, type DocxImportError } from './source/src/index';

export * from './source/src/index';
export * as core from './source/src/core';
export { React, createRoot };

declare global {
  interface Window {
    huiwenReportPreview?: (ready: boolean, error: string) => void;
    huiwenReportScroll?: (page: number, fraction: number, progress: number) => void;
  }
}

let previewRoot: Root | undefined;
let generation = 0;

function scrollElement(): HTMLElement | null {
  return document.querySelector<HTMLElement>('.docx-editor-editor');
}

export function scrollPosition() {
  const element = scrollElement();
  const maximum = element ? element.scrollHeight - element.clientHeight : 0;
  const progress = maximum > 0 ? Math.max(0, Math.min(1, element!.scrollTop / maximum)) : 0;
  return { page: 0, fraction: 0, progress };
}

function reportPosition() {
  const position = scrollPosition();
  window.huiwenReportScroll?.(position.page, position.fraction, position.progress);
}

export function setScrollPosition(_page: number, _fraction: number, progress: number) {
  const element = scrollElement();
  if (element && Number.isFinite(progress)) {
    element.scrollTop = Math.max(0, Math.min(1, progress)) *
      Math.max(0, element.scrollHeight - element.clientHeight);
  }
}

function PreviewError({ error }: { error: DocxImportError }) {
  const message = `无法预览 DOCX：${String(error.message).slice(0, 512)}`;
  React.useEffect(() => { window.huiwenReportPreview?.(false, message); }, [message]);
  return React.createElement('p', { className: 'word-preview-error', role: 'alert' }, message);
}

export function showPreview(encoded: string) {
  const container = document.getElementById('word-body');
  if (!container) throw new Error('缺少 Word 预览容器');
  const bytes = Uint8Array.from(atob(encoded), (character) => character.charCodeAt(0));
  const current = ++generation;
  previewRoot ??= createRoot(container);
  previewRoot.render(React.createElement(DocxEditor, {
    key: current,
    document: bytes,
    mode: { kind: 'readOnly' },
    defaultZoom: 'fit-width',
    showPageGuides: true,
    renderImportError: (error) => React.createElement(PreviewError, { error }),
    onReady: () => {
      requestAnimationFrame(() => {
        if (current !== generation) return;
        scrollElement()?.addEventListener('scroll', reportPosition, { passive: true });
        window.huiwenReportPreview?.(true, '');
        reportPosition();
      });
    }
  }));
}
