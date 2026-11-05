// 最近任务只读取沙箱目录；首页默认三条，全部工作区按十条分页，不创建索引。
import { common } from '@kit.AbilityKit';
import { HarmonySandboxDocument } from 'libhuiwen_native.so';
import { getRecentDocuments } from '../utils/DocumentStore';

export const DEFAULT_RECENT_WORKSPACE_LIMIT: number = 3;
export const WORKSPACE_PAGE_SIZE: number = 10;

export interface RecentWorkspace {
  taskPath: string;
  name: string;
  mode: string;
  kind: string;
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

// 首页和文件管理共用支持规则；DOC 仍由工作区静默转换后预览。
export function canOpenWorkspace(mode: string, kind: string, name: string): boolean {
  return (mode === 'convert' && kind === 'pdf') ||
    (mode === 'edit' && kind === 'md') ||
    (mode === 'preview' && (kind === 'pdf' || kind === 'md' ||
      (kind === 'word' && (name.toLowerCase().endsWith('.doc') || name.toLowerCase().endsWith('.docx')))));
}

export async function listRecentWorkspaces(context: common.UIAbilityContext,
  limit: number = DEFAULT_RECENT_WORKSPACE_LIMIT, offset: number = 0): Promise<RecentWorkspace[]> {
  const documents = await getRecentDocuments(context, limit, 'all', offset);
  return documents.map((document: HarmonySandboxDocument): RecentWorkspace => {
    return {
      taskPath: document.taskPath,
      name: document.name,
      mode: document.mode,
      kind: document.kind,
      formatLabel: formatLabel(document),
      timeLabel: formatTime(document.lastUsedAt),
      canOpen: canOpenWorkspace(document.mode, document.kind, document.name)
    };
  });
}
