// 自定义目录采用系统选择器的持久授权；复制校验完成后才切换根目录。
import { common } from '@kit.AbilityKit';
import { fileIo, fileUri, fileShare, picker } from '@kit.CoreFileKit';
import nativeBridge from 'libhuiwen_native.so';
import { readSettings, saveSettings, workspaceRoot } from './AppSettings';

function directoryPolicy(uri: string): fileShare.PolicyInfo {
  return { uri: uri, operationMode: fileShare.OperationMode.READ_MODE | fileShare.OperationMode.WRITE_MODE };
}

export async function ensureWorkspaceAccess(context: common.Context): Promise<string> {
  const settings = readSettings(context);
  if (settings.storageUri !== '') {
    await fileShare.activatePermission([directoryPolicy(settings.storageUri)]);
  }
  return workspaceRoot(context);
}

// 浏览当前工作区根目录，不打开保存选择器，也不触发目录迁移。
export async function openWorkspaceLocation(context: common.UIAbilityContext): Promise<void> {
  const root = await ensureWorkspaceAccess(context);
  await context.openLink('filemanager://openDirectory', {
    parameters: { fileUri: fileUri.getUriFromPath(root) }
  });
}

export async function chooseWorkspaceLocation(context: common.UIAbilityContext): Promise<boolean> {
  const options = new picker.DocumentSelectOptions();
  options.selectMode = picker.DocumentSelectMode.FOLDER;
  options.maxSelectNumber = 1;
  const uris = await new picker.DocumentViewPicker(context).select(options);
  if (uris.length === 0) {
    return false;
  }
  const uri = uris[0];
  await fileShare.persistPermission([directoryPolicy(uri)]);
  await fileShare.activatePermission([directoryPolicy(uri)]);
  const selected = new fileUri.FileUri(uri).path;
  // 单独建立应用目录，避免将公共目录中的其他文件当作工作区清理。
  const destination = selected.replace(/\/$/, '') + '/HuiWenMCP';
  await migrateWorkspaceLocation(context, destination, uri);
  return true;
}

export async function restoreWorkspaceLocation(context: common.UIAbilityContext): Promise<void> {
  if (readSettings(context).storageRoot === '') {
    return;
  }
  await migrateWorkspaceLocation(context, context.getApplicationContext().filesDir, '');
}

async function migrateWorkspaceLocation(context: common.UIAbilityContext, destination: string,
  uri: string): Promise<void> {
  const source = await ensureWorkspaceAccess(context);
  const settings = readSettings(context);
  // Native 返回校验后的真实根目录；目标有已有数据时拒绝覆盖。
  const root = await nativeBridge.harmonyCopyWorkspaces(source, destination);
  settings.storageRoot = uri === '' ? '' : root;
  settings.storageUri = uri;
  await saveSettings(context, settings);
  if (source !== root) {
    // 切换已落盘后才清理旧副本；失败保留原副本，避免回滚已生效的新目录。
    try {
      await nativeBridge.harmonyCleanWorkspaces(source, 'workspaces', root);
    } catch (error) {
      throw new Error(`新位置已生效，旧目录副本未能清理：${(error as Error).message}`);
    }
  }
}

export async function cleanWorkspaceCache(context: common.UIAbilityContext, mode: string): Promise<number> {
  // 输入缓存始终限定默认沙箱；自定义目录中的用户文件不属于默认缓存。
  const root = mode === 'inputs' ? context.getApplicationContext().filesDir :
    await ensureWorkspaceAccess(context);
  if (!fileIo.accessSync(root)) {
    return 0;
  }
  return nativeBridge.harmonyCleanWorkspaces(root, mode);
}
