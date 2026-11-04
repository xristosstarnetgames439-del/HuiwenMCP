// 汇文截图独立 Qt 宿主；首页保留，截图结束后由 QPA 销毁覆盖窗口。
import { UIAbility, Want, AbilityConstant } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BusinessError } from '@kit.BasicServicesKit';
import qpa from 'libplugins_platforms_qopenharmony.so';
import nativeBridge from 'libhuiwen_native.so';
import { showScreenToast } from '../services/screenshot/ScreenToast';
import { finishScreenshot } from '../services/screenshot/ScreenshotService';
import { downloadScreenshot, rememberColor, rememberScreenshot } from '../services/screenshot/ScreenshotHistory';
import { screenshotCacheRoot } from '../services/settings/AppSettings';

interface ScreenshotResult {
  action: string;
  format: string;
  displayId: number;
  path: string;
  error: string;
  value: string;
  color: string;
  ok: boolean;
  cached: boolean;
}

export default class QtScreenshotAbility extends UIAbility {
  // 与工作区共用 Qt 运行时，启动参数由现有 NAPI 会话桥区分。
  private launchApplication: string = 'libhuiwen_qt.so';
  private launchParams: string = '';
  private contentReady: () => void = () => {};
  private destroyed: boolean = false;

  newLocalStorage(): LocalStorage {
    const storage = new LocalStorage();
    storage.setOrCreate('huiwenScreenshot', true);
    return storage;
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    const events = this.context.getApplicationContext().eventHub;
    this.contentReady = (): void => {
      events.off('huiwen.screenshot.contentReady', this.contentReady);
      this.startScreenshot(windowStage);
    };
    events.on('huiwen.screenshot.contentReady', this.contentReady);
    try {
      // 首页已抓屏；QPA 加载 Index 后再通知 Qt 取用起窗前的底图。
      qpa.startQtApplication(this);
    } catch (error) {
      this.failStart(error as Error);
    }
  }

  private startScreenshot(windowStage: window.WindowStage): void {
    try {
      // 此时 UIContent 已创建，可以安全设置原生窗口背景和装饰。
      const mainWindow = windowStage.getMainWindowSync();
      mainWindow.setWindowBackgroundColor('#00000000');
      mainWindow.setWindowDecorVisible(false);
      const screenshotContext = this.context;
      // 不注册 CEF 平台代理、不加载文件任务，也不向首页发送工作区 ready。
      nativeBridge.registerWorkspace((json: string) => {
        const result = JSON.parse(json) as ScreenshotResult;
        if (result.action === 'downloadScreenshot') {
          this.download(result);
          return;
        }
        if (result.action === 'colorCopied') {
          rememberColor(screenshotContext.getApplicationContext(), result.value, result.color)
            .catch((error: Error) => {
              hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_COLOR_HISTORY_FAILED: %{public}s', error.message);
            });
          // 独立浮窗使用有效的 Ability 上下文加载页面，不依赖首页。
          showScreenToast(screenshotContext, `${result.format} 色值已复制到剪贴板`, result.displayId)
            .catch((error: BusinessError) => {
              hilog.warn(0xFF00, 'HuiwenScreenshot',
                'HUIWEN_SCREEN_TOAST_FAILED: code=%{public}d message=%{public}s', error.code ?? -1, error.message);
            });
          return;
        }
        if (result.action !== 'screenshotFinished') {
          return;
        }
        if (result.error !== '') {
          hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_FAILED: %{public}s', result.error);
          windowStage.getMainWindowSync().getUIContext().getPromptAction().showToast({ message: result.error });
          // 抓屏失败时可能尚未创建 Qt 覆盖窗口，显式结束空宿主。
          this.context.terminateSelf().catch((error: Error) => console.warn(error.message));
        } else {
          hilog.info(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_FINISHED: %{public}s', result.path);
          if (result.ok && result.path !== '') {
            // 入库服务持有应用上下文，截图窗口销毁后仍能完成写盘；取消截图不会新增记录。
            rememberScreenshot(screenshotContext.getApplicationContext(), result.path, result.cached)
              .catch((error: Error) => {
                hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_HISTORY_FAILED: %{public}s', error.message);
              });
          }
        }
      }, 'screenshot', '', screenshotCacheRoot(screenshotContext));
    } catch (error) {
      this.failStart(error as Error);
    }
  }

  private async download(result: ScreenshotResult): Promise<void> {
    let path = '';
    let error = '';
    try {
      if (result.error !== '') { throw new Error(result.error); }
      path = await downloadScreenshot(this.context, result.path);
      hilog.info(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_DOWNLOADED: %{public}s', path);
    } catch (failure) {
      error = (failure as Error).message;
      hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_DOWNLOAD_FAILED: %{public}s', error);
    }
    if (this.destroyed) { return; }
    try {
      await showScreenToast(this.context, error || '截图已保存到设置的目录', result.displayId);
    } catch (failure) {
      hilog.warn(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREEN_TOAST_FAILED: %{public}s', (failure as Error).message);
    }
    // 用户可能在异步保存期间取消截图；旧宿主不能向下一次 Qt 会话发送回包。
    if (!this.destroyed) {
      nativeBridge.workspaceReply(JSON.stringify({ action: 'downloadScreenshot', path: path, error: error }));
    }
  }

  private failStart(error: Error): void {
    finishScreenshot();
    nativeBridge.clearPreparedScreenshot();
    this.context.getApplicationContext().eventHub.off('huiwen.screenshot.contentReady', this.contentReady);
    hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_START_FAILED: %{public}s', error.message);
    this.context.terminateSelf().catch((failure: Error) => console.warn(failure.message));
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    qpa.handleOnNewWant(want, launchParam);
  }

  onDestroy(): void {
    this.destroyed = true;
    finishScreenshot();
    nativeBridge.clearPreparedScreenshot();
    this.context.getApplicationContext().eventHub.off('huiwen.screenshot.contentReady', this.contentReady);
    // 等 QPA 标记宿主已销毁后，再解除本次 Qt 会话，沿用工作区的释放顺序。
    setTimeout(() => nativeBridge.clearWorkspace(), 0);
  }
}
