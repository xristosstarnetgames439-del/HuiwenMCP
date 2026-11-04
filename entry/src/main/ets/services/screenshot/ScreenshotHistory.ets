// 截图记录与存储：界面和 Qt 回调共用此入口；只淘汰本模块的缓存，不删除下载副本。
import { common } from '@kit.AbilityKit';
import { fileIo, fileUri, fileShare, picker } from '@kit.CoreFileKit';
import { pasteboard } from '@kit.BasicServicesKit';
import { image } from '@kit.ImageKit';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { util } from '@kit.ArkTS';
import { DEFAULT_SCREENSHOT_DOWNLOAD_DIR, readSettings, saveSettings,
  screenshotCacheRoot, screenshotDownloadRoot } from '../settings/AppSettings';

export const SCREENSHOT_HISTORY_CHANGED: string = 'huiwen.screenshot.historyChanged';
const HISTORY_LIMIT: number = 10;

export interface ScreenshotImage {
  name: string;
  createdAt: number;
}

export interface ScreenshotColor {
  value: string;
  color: string;
  createdAt: number;
}

export interface ScreenshotHistory {
  images: ScreenshotImage[];
  colors: ScreenshotColor[];
}

// 截图回调可能仍在写盘时打开设置；串行处理入库和目录迁移，避免覆盖最近记录。
let pending: Promise<void> = Promise.resolve();
let imageSequence: number = 0;
function update(action: () => Promise<void>): Promise<void> {
  const next = pending.then(action);
  pending = next.catch(() => {});
  return next;
}

function policy(uri: string): fileShare.PolicyInfo {
  return { uri: uri, operationMode: fileShare.OperationMode.READ_MODE | fileShare.OperationMode.WRITE_MODE };
}

async function activate(uri: string): Promise<void> {
  if (uri !== '') { await fileShare.activatePermission([policy(uri)]); }
}

function ensureDirectory(root: string): void {
  if (!fileIo.accessSync(root)) { fileIo.mkdirSync(root, true); }
  if (!fileIo.lstatSync(root).isDirectory() || fileIo.lstatSync(root).isSymbolicLink()) {
    throw new Error('截图目录必须是普通文件夹');
  }
}

function readHistory(root: string): ScreenshotHistory {
  const path = `${root}/history.json`;
  if (!fileIo.accessSync(path)) { return { images: [], colors: [] }; }
  const history = JSON.parse(fileIo.readTextSync(path)) as ScreenshotHistory;
  if (!Array.isArray(history.images) || !Array.isArray(history.colors)) {
    throw new Error('截图历史记录格式错误，原文件已保留');
  }
  return {
    images: history.images.filter((item: ScreenshotImage) =>
      /^capture_[0-9_]+\.png$/.test(item.name) && Number.isFinite(item.createdAt) &&
      fileIo.accessSync(`${root}/${item.name}`)).slice(0, HISTORY_LIMIT),
    colors: history.colors.filter((item: ScreenshotColor) =>
      /^#[0-9a-fA-F]{6}$/.test(item.color) && typeof item.value === 'string' &&
      Number.isFinite(item.createdAt)).slice(0, HISTORY_LIMIT)
  };
}

function writeHistory(context: common.Context, root: string, history: ScreenshotHistory): void {
  const temporary = `${root}/history.json.tmp`;
  const file = fileIo.openSync(temporary, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
  try {
    // 索引最多二十条，原子替换；不会把截图大图作为 JSON 写入。
    const data = util.TextEncoder.create('utf-8').encodeInto(JSON.stringify(history));
    let offset = 0;
    while (offset < data.byteLength) {
      const written = fileIo.writeSync(file.fd, data.buffer.slice(offset));
      if (written <= 0) { throw new Error('截图历史写入未完成'); }
      offset += written;
    }
    fileIo.fsyncSync(file.fd);
  } finally {
    fileIo.closeSync(file);
  }
  fileIo.renameSync(temporary, `${root}/history.json`);
  context.getApplicationContext().eventHub.emit(SCREENSHOT_HISTORY_CHANGED);
}

export async function loadScreenshotHistory(context: common.Context): Promise<ScreenshotHistory> {
  const root = await prepareScreenshotCache(context);
  return readHistory(root);
}

// 在抓屏前取得缓存目录权限；不延迟 Qt 覆盖窗口的首次展示。
export async function prepareScreenshotCache(context: common.Context): Promise<string> {
  await pending;
  await activate(readSettings(context).screenshotCacheUri);
  const root = screenshotCacheRoot(context);
  ensureDirectory(root);
  return root;
}

// Qt 自动截图直接写入配置目录，保留剪贴板中的文件 URI；用户另存文件只复制、不删除。
export function rememberScreenshot(context: common.Context, source: string, cached: boolean): Promise<void> {
  return update(async () => {
    await activate(readSettings(context).screenshotCacheUri);
    const root = screenshotCacheRoot(context);
    ensureDirectory(root);
    const sourceName = source.substring(source.lastIndexOf('/') + 1);
    const alreadyCached = cached && /^capture_[0-9_]+\.png$/.test(sourceName) && source === `${root}/${sourceName}`;
    const name = alreadyCached ? sourceName : `capture_${Date.now()}_${++imageSequence}.png`;
    const target = `${root}/${name}`;
    if (!alreadyCached) { await copyVerified(source, target); }
    const history = readHistory(root);
    const images: ScreenshotImage[] = [{ name: name, createdAt: Date.now() },
      ...history.images.filter((item: ScreenshotImage) => item.name !== name)];
    writeHistory(context, root, { images: images.slice(0, HISTORY_LIMIT), colors: history.colors });
    images.slice(HISTORY_LIMIT).forEach((item: ScreenshotImage) => removeCache(`${root}/${item.name}`));
  });
}

export function rememberColor(context: common.Context, value: string, color: string): Promise<void> {
  return update(async () => {
    if (!/^#[0-9a-fA-F]{6}$/.test(color)) { throw new Error('取色色值格式无效'); }
    await activate(readSettings(context).screenshotCacheUri);
    const root = screenshotCacheRoot(context);
    ensureDirectory(root);
    const history = readHistory(root);
    const normalized = color.toLowerCase();
    const colors: ScreenshotColor[] = [{ value: value, color: normalized, createdAt: Date.now() },
      ...history.colors.filter((item: ScreenshotColor) => item.color.toLowerCase() !== normalized)];
    writeHistory(context, root, { images: history.images, colors: colors.slice(0, HISTORY_LIMIT) });
  });
}

export async function copyScreenshotColor(context: common.Context, item: ScreenshotColor): Promise<void> {
  await pasteboard.getSystemPasteboard().setData(pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, item.value));
  await rememberColor(context, item.value, item.color);
}

export async function copyScreenshotImage(path: string): Promise<void> {
  const source = image.createImageSource(path);
  let pixels: image.PixelMap | undefined = undefined;
  try {
    pixels = await source.createPixelMap();
    await pasteboard.getSystemPasteboard().setData(pasteboard.createData(pasteboard.MIMETYPE_PIXELMAP, pixels));
  } finally {
    if (pixels) { await pixels.release(); }
    await source.release();
  }
}

function removeCache(path: string): void {
  try {
    if (fileIo.accessSync(path)) { fileIo.unlinkSync(path); }
  } catch (error) {
    hilog.warn(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_CACHE_CLEAN_FAILED: %{public}s', (error as Error).message);
  }
}

async function digest(path: string): Promise<Uint8Array> {
  const md = cryptoFramework.createMd('SHA256');
  const file = await fileIo.open(path, fileIo.OpenMode.READ_ONLY);
  try {
    const buffer = new ArrayBuffer(1024 * 1024);
    let length = await fileIo.read(file.fd, buffer);
    while (length > 0) {
      await md.update({ data: new Uint8Array(buffer.slice(0, length)) });
      length = await fileIo.read(file.fd, buffer);
    }
    return (await md.digest()).data;
  } finally {
    await fileIo.close(file);
  }
}

async function copyVerified(source: string, target: string): Promise<void> {
  if (fileIo.accessSync(target)) { throw new Error('目标图片已存在，请重试'); }
  if (!fileIo.lstatSync(source).isFile() || fileIo.lstatSync(source).isSymbolicLink()) {
    throw new Error('截图源文件无效');
  }
  try {
    await fileIo.copyFile(source, target);
    const before = await digest(source);
    const after = await digest(target);
    if (before.length !== after.length || !before.every((value: number, index: number) => value === after[index])) {
      throw new Error('截图复制校验失败，原文件已保留');
    }
  } catch (error) {
    removeCache(target);
    throw error as Error;
  }
}

// 下载只生成副本，不改变缓存历史。权限不足时使用系统目录选择器获取持久授权。
export async function downloadScreenshot(context: common.UIAbilityContext, path: string): Promise<string> {
  let root = screenshotDownloadRoot(context);
  try {
    await activate(readSettings(context).screenshotDownloadUri);
    ensureDirectory(root);
    if (!fileIo.accessSync(root, fileIo.AccessModeType.WRITE)) { throw new Error('下载目录没有写入权限'); }
  } catch (_) {
    if (!await changeScreenshotDirectory(context, false, false)) { return ''; }
    root = screenshotDownloadRoot(context);
  }
  ensureDirectory(root);
  const target = `${root}/HuiWenScreenShot_${Date.now()}.png`;
  await copyVerified(path, target);
  return target;
}

export async function openScreenshotDirectory(context: common.UIAbilityContext, cache: boolean): Promise<void> {
  const settings = readSettings(context);
  await activate(cache ? settings.screenshotCacheUri : settings.screenshotDownloadUri);
  const root = cache ? screenshotCacheRoot(context) : screenshotDownloadRoot(context);
  ensureDirectory(root);
  await context.openLink('filemanager://openDirectory', { parameters: { fileUri: fileUri.getUriFromPath(root) } });
}

export async function changeScreenshotDirectory(context: common.UIAbilityContext, cache: boolean,
  restore: boolean): Promise<boolean> {
  let uri = '';
  let destination = cache ? `${context.getApplicationContext().filesDir}/screenshots` :
    DEFAULT_SCREENSHOT_DOWNLOAD_DIR;
  if (!restore) {
    const options = new picker.DocumentSelectOptions();
    options.selectMode = picker.DocumentSelectMode.FOLDER;
    options.maxSelectNumber = 1;
    const uris = await new picker.DocumentViewPicker(context).select(options);
    if (uris.length === 0) { return false; }
    uri = uris[0];
    await fileShare.persistPermission([policy(uri)]);
    await activate(uri);
    const selected = new fileUri.FileUri(uri).path.replace(/\/$/, '');
    destination = selected.endsWith('/HuiWenScreenShot') ? selected : `${selected}/HuiWenScreenShot`;
  }
  await update(async () => {
    const settings = readSettings(context);
    if (!cache) {
      settings.screenshotDownloadRoot = restore ? '' : destination;
      settings.screenshotDownloadUri = uri;
      await saveSettings(context, settings);
      return;
    }
    await activate(settings.screenshotCacheUri);
    const source = screenshotCacheRoot(context);
    if (source === destination) { return; }
    if (destination.startsWith(`${source}/`) || source.startsWith(`${destination}/`)) {
      throw new Error('截图缓存的新旧目录不能相互包含');
    }
    ensureDirectory(destination);
    if (fileIo.listFileSync(destination).length !== 0) { throw new Error('目标缓存文件夹非空，请选择其他位置'); }
    const history = readHistory(source);
    try {
      for (const item of history.images) { await copyVerified(`${source}/${item.name}`, `${destination}/${item.name}`); }
      writeHistory(context, destination, history);
      settings.screenshotCacheRoot = restore ? '' : destination;
      settings.screenshotCacheUri = uri;
      await saveSettings(context, settings);
    } catch (error) {
      // 目标原本为空；切换失败时仅清理本次副本，源图与原配置仍可用。
      history.images.forEach((item: ScreenshotImage) => removeCache(`${destination}/${item.name}`));
      removeCache(`${destination}/history.json`);
      removeCache(`${destination}/history.json.tmp`);
      throw error as Error;
    }
    history.images.forEach((item: ScreenshotImage) => removeCache(`${source}/${item.name}`));
    removeCache(`${source}/history.json`);
  });
  return true;
}
