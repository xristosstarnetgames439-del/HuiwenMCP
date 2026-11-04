// 独立截图预览页面：图片等比完整展示，右键提供复制、下载和关闭当前预览。
import { common } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { fileUri } from '@kit.CoreFileKit';
import { copyScreenshotImage, downloadScreenshot } from '../services/screenshot/ScreenshotHistory';

@Entry(LocalStorage.getShared())
@Component
struct ScreenshotPreviewPage {
  @LocalStorageProp('screenshotPreviewPath') path: string = '';
  @LocalStorageProp('screenshotPreviewWindow') windowName: string = '';
  @State busy: boolean = false;

  private async close(): Promise<void> {
    try {
      await window.findWindow(this.windowName).destroyWindow();
    } catch (error) {
      this.getUIContext().getPromptAction().showToast({ message: (error as Error).message });
    }
  }

  private async perform(download: boolean): Promise<void> {
    if (this.busy) { return; }
    this.busy = true;
    try {
      if (download) {
        const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
        const saved = await downloadScreenshot(context, this.path);
        this.getUIContext().getPromptAction().showToast({ message: `已保存到 ${saved}` });
      } else {
        await copyScreenshotImage(this.path);
        this.getUIContext().getPromptAction().showToast({ message: '图片已复制到剪贴板' });
      }
    } catch (error) {
      this.getUIContext().getPromptAction().showToast({ message: (error as Error).message });
    } finally {
      this.busy = false;
    }
  }

  @Builder
  previewMenu() {
    Menu() {
      MenuItem({ content: '复制' }).enabled(!this.busy).onClick(() => this.perform(false))
      MenuItem({ content: '下载' }).enabled(!this.busy).onClick(() => this.perform(true))
      MenuItem({ content: '关闭' }).onClick(() => this.close())
    }
  }

  build() {
    Stack() {
      Image(fileUri.getUriFromPath(this.path))
        .width('100%')
        .height('100%')
        .objectFit(ImageFit.Contain)
        .draggable(false)
    }
    .width('100%')
    .height('100%')
    .padding(16)
    .backgroundColor($r('app.color.home_background'))
    .bindContextMenu(this.previewMenu, ResponseType.RightClick)
  }
}
