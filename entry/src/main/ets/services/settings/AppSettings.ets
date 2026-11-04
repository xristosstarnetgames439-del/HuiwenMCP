// 应用设置只负责持久化；目录操作、截图及窗口关闭分别由原模块执行。
import { common } from '@kit.AbilityKit';
import { preferences } from '@kit.ArkData';
import { KeyCode } from '@kit.InputKit';

export interface ScreenshotShortcut {
  modifier: number;
  key: number;
  keyName?: string;
}

export interface AppSettings {
  storageRoot: string;
  storageUri: string;
  recentLimit: number;
  closeAction: string;
  hiddenShortcut: ScreenshotShortcut;
  screenShortcut: ScreenshotShortcut;
  screenshotCacheRoot: string;
  screenshotCacheUri: string;
  screenshotDownloadRoot: string;
  screenshotDownloadUri: string;
}

export const SETTINGS_CHANGED: string = 'huiwen.settings.changed';
export const DEFAULT_SCREENSHOT_DOWNLOAD_DIR: string = '/storage/Users/currentUser/Images/gallery/HuiWenScreenShot';

export function defaultSettings(): AppSettings {
  return {
    storageRoot: '', storageUri: '', recentLimit: 3, closeAction: 'home',
    hiddenShortcut: { modifier: KeyCode.KEYCODE_CTRL_LEFT, key: KeyCode.KEYCODE_1 },
    screenShortcut: { modifier: KeyCode.KEYCODE_CTRL_LEFT, key: KeyCode.KEYCODE_2 },
    screenshotCacheRoot: '', screenshotCacheUri: '', screenshotDownloadRoot: '', screenshotDownloadUri: ''
  };
}

export function readSettings(context: common.Context): AppSettings {
  const store = preferences.getPreferencesSync(context.getApplicationContext(), { name: 'app_settings' });
  const result = defaultSettings();
  const cacheRoot = store.getSync('screenshotCacheRoot', '');
  const cacheUri = store.getSync('screenshotCacheUri', '');
  const downloadRoot = store.getSync('screenshotDownloadRoot', '');
  const downloadUri = store.getSync('screenshotDownloadUri', '');
  result.screenshotCacheRoot = typeof cacheRoot === 'string' ? cacheRoot : '';
  result.screenshotCacheUri = typeof cacheUri === 'string' ? cacheUri : '';
  result.screenshotDownloadRoot = typeof downloadRoot === 'string' ? downloadRoot : '';
  result.screenshotDownloadUri = typeof downloadUri === 'string' ? downloadUri : '';
  const root = store.getSync('storageRoot', '');
  const uri = store.getSync('storageUri', '');
  const limit = store.getSync('recentLimit', 3);
  result.storageRoot = typeof root === 'string' ? root : '';
  result.storageUri = typeof uri === 'string' ? uri : '';
  result.recentLimit = limit === 5 || limit === 10 ? limit : 3;
  result.closeAction = store.getSync('closeAction', 'home') === 'exit' ? 'exit' : 'home';
  const hiddenModifier = store.getSync('hiddenModifier', result.hiddenShortcut.modifier);
  const hiddenKey = store.getSync('hiddenKey', result.hiddenShortcut.key);
  const screenModifier = store.getSync('screenModifier', result.screenShortcut.modifier);
  const screenKey = store.getSync('screenKey', result.screenShortcut.key);
  const hiddenName = store.getSync('hiddenName', '');
  const screenName = store.getSync('screenName', '');
  if (typeof hiddenModifier === 'number' && typeof hiddenKey === 'number') {
    result.hiddenShortcut = { modifier: hiddenModifier, key: hiddenKey,
      keyName: typeof hiddenName === 'string' ? hiddenName : '' };
  }
  if (typeof screenModifier === 'number' && typeof screenKey === 'number') {
    result.screenShortcut = { modifier: screenModifier, key: screenKey,
      keyName: typeof screenName === 'string' ? screenName : '' };
  }
  return result;
}

function writeSettings(store: preferences.Preferences, settings: AppSettings): void {
  store.putSync('storageRoot', settings.storageRoot);
  store.putSync('storageUri', settings.storageUri);
  store.putSync('recentLimit', settings.recentLimit);
  store.putSync('closeAction', settings.closeAction);
  store.putSync('hiddenModifier', settings.hiddenShortcut.modifier);
  store.putSync('hiddenKey', settings.hiddenShortcut.key);
  store.putSync('screenModifier', settings.screenShortcut.modifier);
  store.putSync('screenKey', settings.screenShortcut.key);
  store.putSync('hiddenName', settings.hiddenShortcut.keyName || '');
  store.putSync('screenName', settings.screenShortcut.keyName || '');
  store.putSync('screenshotCacheRoot', settings.screenshotCacheRoot);
  store.putSync('screenshotCacheUri', settings.screenshotCacheUri);
  store.putSync('screenshotDownloadRoot', settings.screenshotDownloadRoot);
  store.putSync('screenshotDownloadUri', settings.screenshotDownloadUri);
}

export async function saveSettings(context: common.Context, settings: AppSettings): Promise<void> {
  const previous = readSettings(context);
  const store = preferences.getPreferencesSync(context.getApplicationContext(), { name: 'app_settings' });
  writeSettings(store, settings);
  try {
    await store.flush();
  } catch (error) {
    // flush 失败也恢复进程内缓存，迁移调用方此时仍保留完整源文件。
    writeSettings(store, previous);
    throw new Error(`设置保存失败，原文件已保留：${(error as Error).message}`);
  }
  context.getApplicationContext().eventHub.emit(SETTINGS_CHANGED);
}

export function workspaceRoot(context: common.Context): string {
  return readSettings(context).storageRoot || context.getApplicationContext().filesDir;
}

export function screenshotCacheRoot(context: common.Context): string {
  return readSettings(context).screenshotCacheRoot || `${context.getApplicationContext().filesDir}/screenshots`;
}

export function screenshotDownloadRoot(context: common.Context): string {
  return readSettings(context).screenshotDownloadRoot || DEFAULT_SCREENSHOT_DOWNLOAD_DIR;
}
