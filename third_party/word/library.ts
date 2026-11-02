// 只导出离线库，不创建编辑器实例或业务页面。
import './source/src/styles/editor.css';

export * from './source/src/index';
export * as core from './source/src/core';
export * as React from 'react';
export { createRoot } from 'react-dom/client';
