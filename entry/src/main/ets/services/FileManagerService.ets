// 文件管理数据入口：只读一次沙箱快照，分类与搜索复用内存结果。
import { common, OpenLinkOptions } from '@kit.AbilityKit';
import { fileUri } from '@kit.CoreFileKit';
import nativeBridge, { HarmonySandboxFile } from 'libhuiwen_native.so';
import { FileManagerFile } from '../models/FileManagerFile';

function formatMode(file: HarmonySandboxFile): string {
  if (file.mode !== 'convert') {
    return file.mode === 'edit' ? '编辑模式' : '预览模式';
  }
  if (file.sourceKind === '') {
    return '转化模式';
  }
  return `${file.sourceKind === 'word' ? 'Word' : file.sourceKind.toUpperCase()} → MD`;
}

export function formatFileSize(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes < 0) {
    return '--';
  }
  const units: string[] = ['B', 'KB', 'MB', 'GB', 'TB'];
  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit++;
  }
  return `${value.toFixed(unit === 0 ? 0 : 1)} ${units[unit]}`;
}

export async function listFileManagerFiles(context: common.UIAbilityContext): Promise<FileManagerFile[]> {
  const files = await nativeBridge.harmonyListFiles(context.getApplicationContext().filesDir);
  return files.map((file: HarmonySandboxFile): FileManagerFile => {
    return {
      name: file.name, path: file.path, role: file.role, kind: file.kind,
      taskPath: file.taskPath, workspaceName: file.workspaceName,
      mode: file.mode, sourceKind: file.sourceKind, lastUsedAt: file.lastUsedAt,
      sizeBytes: file.sizeBytes, formatLabel: formatMode(file), sizeLabel: formatFileSize(file.sizeBytes)
    };
  });
}

export function filterFileManagerFiles(files: FileManagerFile[], category: string,
  keyword: string): FileManagerFile[] {
  const query = keyword.trim().toLowerCase();
  return files.filter((file: FileManagerFile): boolean => {
    const matchesCategory = category === '全部' ||
      (category === '输入文件' && file.role === 'input') ||
      (category === '输出文件' && file.role === 'output') ||
      (category === 'Word' && file.kind === 'word') ||
      (category === 'PDF' && file.kind === 'pdf') ||
      (category === 'Markdown' && file.kind === 'md');
    return matchesCategory && file.name.toLowerCase().includes(query);
  });
}

// 左键只定位所在目录：复用沙箱目录校验，不恢复任务或修改最近使用时间。
export async function openFileDirectory(context: common.UIAbilityContext, file: FileManagerFile): Promise<void> {
  const directory = nativeBridge.harmonyValidateTaskDirectory(
    context.getApplicationContext().filesDir, file.path.substring(0, file.path.lastIndexOf('/')));
  const options: OpenLinkOptions = {
    parameters: { fileUri: fileUri.getUriFromPath(directory) }
  };
  await context.openLink('filemanager://openDirectory', options);
}
