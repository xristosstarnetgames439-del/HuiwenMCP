// 最近任务只读取沙箱目录，不创建索引；数量通过参数统一控制，默认展示三条。
import { common } from '@kit.AbilityKit';
import nativeBridge, { HarmonySandboxDocument } from 'libhuiwen_native.so';
import { getRecentDocuments } from '../utils/DocumentStore';

export const DEFAULT_RECENT_WORKSPACE_LIMIT: number = 3;

export interface RecentWorkspace {
  taskPath: string;
  name: string;
  mode: string;
  formatLabel: string;
  timeLabel: string;
  canOpen: boolean;
}

function formatTime(timestamp: number): string {
  const date = new Date(timestamp);
  if (!Number.isFinite(timestamp) || Number.isNaN(date.getTime())) {
    return '--';
  }
  const month = String(date.getMonth() + 1).padStart(2, '0');
  const day = String(date.getDate()).padStart(2, '0');
  const hours = String(date.getHours()).padStart(2, '0');
  const minutes = String(date.getMinutes()).padStart(2, '0');
  return `${date.getFullYear()}-${month}-${day} ${hours}:${minutes}`;
}

function formatLabel(document: HarmonySandboxDocument): string {
  if (document.mode === 'convert') {
    return `${document.kind === 'word' ? 'Word' : document.kind.toUpperCase()} → MD`;
  }
  return document.mode === 'edit' ? '编辑模式' : '预览模式';
}

export async function listRecentWorkspaces(context: common.UIAbilityContext,
  limit: number = DEFAULT_RECENT_WORKSPACE_LIMIT): Promise<RecentWorkspace[]> {
  const documents = await getRecentDocuments(context, limit, 'all');
  return documents.map((document: HarmonySandboxDocument): RecentWorkspace => {
    return {
      taskPath: document.taskPath,
      name: document.name,
      mode: document.mode,
      formatLabel: formatLabel(document),
      timeLabel: formatTime(document.lastUsedAt),
      // 预览和转化进入同一个 Qt 工作区；编辑及 Word 引擎尚未接入。
      canOpen: (document.mode === 'convert' && document.kind === 'pdf') ||
        (document.mode === 'preview' && (document.kind === 'pdf' || document.kind === 'md'))
    };
  });
}

// 拉起 Qt 前先筛出失效任务；Qt Ability 仍会再次校验，避免使用页面缓存的路径。
export function validateRecentWorkspace(context: common.UIAbilityContext,
  taskPath: string): Promise<HarmonySandboxDocument> {
  return nativeBridge.harmonyLoadTask(context.getApplicationContext().filesDir, taskPath);
}
