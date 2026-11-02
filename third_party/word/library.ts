// 只导出离线库，不创建编辑器实例或业务页面。
import '../portone-docx/src/styles/editor.css';

export * from '../portone-docx/src/index';
export * as core from '../portone-docx/src/core';
export * as React from 'react';
export { createRoot } from 'react-dom/client';
