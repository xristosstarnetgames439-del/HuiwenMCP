import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import UIAbility from '@ohos.app.ability.UIAbility';
import Window from '@ohos.window'
import Want from '@ohos.app.ability.Want';
import hilog from '@ohos.hilog';
import { BusinessError } from '@kit.BasicServicesKit';
import { Configuration } from '@kit.AbilityKit';
import { applyHomeWindowColors, initializeHomeAppearance } from '../utils/HomeAppearance';


export default class EntryAbility extends UIAbility {
  tag = "HuiwenMCP";
  domain = 0xFF00;
  private contentReady: boolean = false;
  private foreground: boolean = false;
  private mainWindow: Window.Window | undefined = undefined;

  // 页面已加载且实际进入前台，才确认可以结束旧工作区。
  private notifyHomeReady(): void {
    if (this.contentReady && this.foreground) {
      this.context.getApplicationContext().eventHub.emit('huiwen.home.ready', '');
    }
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    this.notifyHomeReady();
  }

  onConfigurationUpdate(_configuration: Configuration): void {
    // 跟随系统深浅色变化；手动外观切换由 HomeAppearance 同步。
    if (this.mainWindow) {
      applyHomeWindowColors(this.context, this.mainWindow);
    }
  }

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam) {
    super.onCreate(want, launchParam)
    // 每个新首页都等窗口内容加载完成后再启用自绘标题区。
    AppStorage.setOrCreate('homeCustomTitleBar', false);
    // 转化环境由 Qt 转化模式按需准备，首页和独立预览不依赖 Python 业务包。
    try {
      initializeHomeAppearance(this.context);
    } catch (error) {
      hilog.warn(this.domain, this.tag, 'Restore appearance failed: %{public}s', (error as Error).message);
    }
  }

  newLocalStorage() {
    return new LocalStorage()
  }

  onDestroy() {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onDestroy');
  }

  onWindowStageCreate(windowStage: Window.WindowStage) {
    // ArkTS 首页使用独立路由，pages/Index 留给 QPA 承载 Qt 节点。
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onWindowStageCreate');
    windowStage.loadContent('pages/HomePage', (err) => {
      if (err.code) {
        hilog.error(this.domain, this.tag, 'Failed to load page: %{public}s', JSON.stringify(err));
        this.context.getApplicationContext().eventHub.emit('huiwen.home.ready', err.message);
        return;
      }
      try {
        const mainWindow = windowStage.getMainWindowSync();
        this.mainWindow = mainWindow;
        const uiContext = mainWindow.getUIContext();
        // 窗口限制使用 px，按首页密度换算 880 × 680vp；不限制最大尺寸。
        mainWindow.setWindowLimits({
          minWidth: Math.round(uiContext.vp2px(880)),
          minHeight: Math.round(uiContext.vp2px(680))
        }).catch((error: BusinessError) => {
          hilog.warn(this.domain, this.tag, 'HUIWEN_HOME_LIMITS_FAILED: %{public}s', error.message);
        });
      } catch (error) {
        hilog.warn(this.domain, this.tag, 'HUIWEN_HOME_LIMITS_FAILED: %{public}s', (error as Error).message);
      }
      try {
        const mainWindow = windowStage.getMainWindowSync();
        // 只隐藏系统图标和名称，保留原生三键；标题区拖动和双击由组件处理。
        mainWindow.setWindowTitleMoveEnabled(false);
        mainWindow.setWindowDecorVisible(false);
        AppStorage.setOrCreate('homeCustomTitleBar', true);
      } catch (error) {
        hilog.warn(this.domain, this.tag, 'HUIWEN_HOME_TITLEBAR_FAILED: %{public}s',
          (error as Error).message);
      }
      // 两层背景分别设置，容器设置失败也不会阻断内容背景透明。
      try {
        windowStage.getMainWindowSync().setWindowBackgroundColor('#00000000');
      } catch (error) {
        const failure = error as BusinessError;
        hilog.warn(this.domain, this.tag,
          'HUIWEN_HOME_TRANSPARENT_FAILED: stage=background, code=%{public}d, message=%{public}s',
          failure.code, failure.message);
      }
      if (this.mainWindow) {
        applyHomeWindowColors(this.context, this.mainWindow);
      }
      this.contentReady = true;
      this.notifyHomeReady();
    });
  }

  onWindowStageDestroy() {
    this.mainWindow = undefined;
    this.contentReady = false;
    this.foreground = false;
    // Main window is destroyed, release UI related resources
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground() {
    this.foreground = true;
    this.notifyHomeReady();
    // Ability has brought to foreground
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onForeground');
  }

  onBackground() {
    this.foreground = false;
    // Ability has back to background
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onBackground');
  }

};
