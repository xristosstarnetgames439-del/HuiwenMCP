// 图片预览使用独立应用子窗口；与截图覆盖窗口、Qt 工作区互不复用会话。
import { common } from '@kit.AbilityKit';
import { display, window } from '@kit.ArkUI';

let preview: window.Window | undefined = undefined;
let opening: boolean = false;

export async function closeScreenshotPreview(): Promise<void> {
  const current = preview;
  preview = undefined;
  if (current) { await current.destroyWindow(); }
}

export async function openScreenshotPreview(context: common.UIAbilityContext, path: string): Promise<void> {
  if (opening) { return; }
  opening = true;
  let created: window.Window | undefined = undefined;
  try {
    await closeScreenshotPreview();
    const parent = await window.getLastWindow(context);
    const screen = display.getDisplayByIdSync(parent.getWindowProperties().displayId ?? display.getDefaultDisplaySync().id);
    created = await parent.createSubWindowWithOptions('huiwen_screenshot_preview', {
      title: '截图预览', decorEnabled: true, isModal: false
    });
    const current = created;
    const width = Math.round(screen.width * 0.8);
    const height = Math.round(screen.height * 0.8);
    await current.resize(width, height);
    await current.moveWindowTo(Math.round((screen.width - width) / 2), Math.round((screen.height - height) / 2));
    const storage = new LocalStorage();
    storage.setOrCreate('screenshotPreviewPath', path);
    storage.setOrCreate('screenshotPreviewWindow', 'huiwen_screenshot_preview');
    await current.loadContent('pages/ScreenshotPreviewPage', storage);
    current.setWindowBackgroundColor('#F4F6F8');
    current.on('windowEvent', (event: window.WindowEventType) => {
      if (event === window.WindowEventType.WINDOW_DESTROYED && preview === current) { preview = undefined; }
    });
    await current.showWindow();
    preview = current;
  } catch (error) {
    if (created) { await created.destroyWindow(); }
    throw error as Error;
  } finally {
    opening = false;
  }
}
