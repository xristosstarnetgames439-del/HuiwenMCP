// 汇文截图独立 Qt 宿主；首页保留，截图结束后由 QPA 销毁覆盖窗口。
import { UIAbility, Want, AbilityConstant } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import qpa from 'libplugins_platforms_qopenharmony.so';
import nativeBridge from 'libhuiwen_native.so';

interface ScreenshotResult {
  action: string;
  path: string;
  error: string;
}

export default class QtScreenshotAbility extends UIAbility {
  // 与工作区共用 Qt 运行时，启动参数由现有 NAPI 会话桥区分。
  private launchApplication: string = 'libhuiwen_qt.so';
  private launchParams: string = '';
  private contentReady: () => void = () => {};

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
      // 不注册 CEF 平台代理、不加载文件任务，也不向首页发送工作区 ready。
      nativeBridge.registerWorkspace((json: string) => {
        const result = JSON.parse(json) as ScreenshotResult;
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
        }
      }, 'screenshot', '');
    } catch (error) {
      this.failStart(error as Error);
    }
  }

  private failStart(error: Error): void {
    nativeBridge.clearPreparedScreenshot();
    this.context.getApplicationContext().eventHub.off('huiwen.screenshot.contentReady', this.contentReady);
    hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_START_FAILED: %{public}s', error.message);
    this.context.terminateSelf().catch((failure: Error) => console.warn(failure.message));
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    qpa.handleOnNewWant(want, launchParam);
  }

  onDestroy(): void {
    nativeBridge.clearPreparedScreenshot();
    this.context.getApplicationContext().eventHub.off('huiwen.screenshot.contentReady', this.contentReady);
    // 等 QPA 标记宿主已销毁后，再解除本次 Qt 会话，沿用工作区的释放顺序。
    setTimeout(() => nativeBridge.clearWorkspace(), 0);
  }
}
