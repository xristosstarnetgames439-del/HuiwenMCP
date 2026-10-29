import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import UIAbility from '@ohos.app.ability.UIAbility';
import Window from '@ohos.window'
import Want from '@ohos.app.ability.Want';
import hilog from '@ohos.hilog';
import { BusinessError } from '@kit.BasicServicesKit';
import { initializeHomeAppearance } from '../utils/HomeAppearance';
import nativeBridge from 'libhuiwen_native.so';


export default class EntryAbility extends UIAbility {
  tag = "HuiwenMCP";
  domain = 0xFF00;
  private contentReady: boolean = false;
  private foreground: boolean = false;

  // 页面已加载且实际进入前台，才确认可以结束旧工作区。
  private notifyHomeReady(): void {
    if (this.contentReady && this.foreground) {
      this.context.getApplicationContext().eventHub.emit('huiwen.home.ready', '');
    }
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    this.notifyHomeReady();
  }

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam) {
    super.onCreate(want, launchParam)
    // 业务 pyz 随 HAP 更新，部署到缓存后供共享 PythonRunner 使用。
    try {
      nativeBridge.configurePython(this.context.resourceManager, this.context.cacheDir);
    } catch (error) {
      hilog.error(this.domain, this.tag, 'PYTHON_BUNDLE_FAILED: %{public}s', (error as Error).message);
    }
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
      // 两层背景分别设置，容器设置失败也不会阻断内容背景透明。
      try {
        windowStage.getMainWindowSync().setWindowBackgroundColor('#00000000');
      } catch (error) {
        const failure = error as BusinessError;
        hilog.warn(this.domain, this.tag,
          'HUIWEN_HOME_TRANSPARENT_FAILED: stage=background, code=%{public}d, message=%{public}s',
          failure.code, failure.message);
      }
      try {
        // 仅设置首页窗口；主内容保持实色，保留系统标题栏与窗口控制按钮。
        // 焦点态透明，非焦点态需使用不透明底色。
        windowStage.getMainWindowSync().setWindowContainerColor('#00000000', '#FFF5F7FB');
      } catch (error) {
        const failure = error as BusinessError;
        // 不支持容器透明时保留系统容器背景，首页与按钮仍可使用。
        hilog.warn(this.domain, this.tag,
          'HUIWEN_HOME_TRANSPARENT_FAILED: stage=container, code=%{public}d, message=%{public}s',
          failure.code, failure.message);
      }
      this.contentReady = true;
      this.notifyHomeReady();
    });
  }

  onWindowStageDestroy() {
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
