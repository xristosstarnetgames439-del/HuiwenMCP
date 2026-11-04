// 首页截图管理入口：只组合最近图片和色值；不启动截图或持有 Qt 会话。
import { common } from '@kit.AbilityKit';
import { HomeDialog } from '../common/HomeDialog';
import { RecentScreenshotList } from './RecentScreenshotList';
import { RecentColorList } from './RecentColorList';
import { screenshotCacheRoot } from '../../services/settings/AppSettings';
import { copyScreenshotColor, loadScreenshotHistory, SCREENSHOT_HISTORY_CHANGED,
  ScreenshotImage, ScreenshotColor } from '../../services/screenshot/ScreenshotHistory';
import { openScreenshotPreview } from '../../services/screenshot/ScreenshotPreview';

@Component
export struct ScreenshotDialog {
  @Prop dialogWidth: number = 720;
  @Prop contentHeight: number = 360;
  @State images: ScreenshotImage[] = [];
  @State colors: ScreenshotColor[] = [];
  @State root: string = '';
  @State error: string = '';
  private visible: boolean = false;
  private changed = (): void => { this.reload(); };
  onCloseRequested: () => void = () => {};

  private context(): common.UIAbilityContext {
    return this.getUIContext().getHostContext() as common.UIAbilityContext;
  }

  aboutToAppear(): void {
    this.visible = true;
    this.context().getApplicationContext().eventHub.on(SCREENSHOT_HISTORY_CHANGED, this.changed);
    this.reload();
  }

  aboutToDisappear(): void {
    this.visible = false;
    this.context().getApplicationContext().eventHub.off(SCREENSHOT_HISTORY_CHANGED, this.changed);
  }

  private async reload(): Promise<void> {
    try {
      const history = await loadScreenshotHistory(this.context());
      if (!this.visible) { return; }
      this.root = screenshotCacheRoot(this.context());
      this.images = history.images;
      this.colors = history.colors;
      this.error = '';
    } catch (error) {
      if (this.visible) { this.error = (error as Error).message; }
    }
  }

  private async copyColor(item: ScreenshotColor): Promise<void> {
    try {
      await copyScreenshotColor(this.context(), item);
      this.getUIContext().getPromptAction().showToast({ message: '色值已复制到剪贴板' });
    } catch (error) {
      this.error = (error as Error).message;
    }
  }

  private async preview(path: string): Promise<void> {
    try {
      await openScreenshotPreview(this.context(), path);
    } catch (error) {
      this.error = `打开截图失败：${(error as Error).message}`;
    }
  }

  build() {
    HomeDialog({ title: '截图', dialogWidth: this.dialogWidth, onCloseRequested: this.onCloseRequested }) {
      Scroll() {
        Column({ space: 16 }) {
          Text('最近截图').fontSize(16).fontColor($r('app.color.home_text'))
          RecentScreenshotList({ images: this.images, root: this.root, onOpen: (path: string) => this.preview(path) })
          Text('最近复制的色值').fontSize(16).fontColor($r('app.color.home_text')).margin({ top: 12 })
          RecentColorList({ colors: this.colors, onCopy: (item: ScreenshotColor) => this.copyColor(item) })
          if (this.error !== '') {
            Text(this.error).fontSize(13).fontColor($r('app.color.home_warning'))
          }
        }
        .width('100%')
        .alignItems(HorizontalAlign.Start)
      }
      .width('100%')
      .constraintSize({ maxHeight: this.contentHeight })
      .scrollBar(BarState.Auto)
    }
  }
}
