// 首页自绘标题区：仅绘制图标和名称，右侧三键继续由鸿蒙系统承载。
import { window } from '@kit.ArkUI';
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

@Component
export struct HomeTitleBar {
  @State private barHeight: number = 37;
  @State private buttonsWidth: number = 160;
  @State private expanded: boolean = false;
  private mainWindow: window.Window | undefined = undefined;
  private active: boolean = false;
  private onStatusChanged = (status: window.WindowStatusType): void => {
    // 全屏与最大化共用品牌标识，普通窗口恢复应用图标。
    this.expanded = status === window.WindowStatusType.FULL_SCREEN ||
      status === window.WindowStatusType.MAXIMIZE;
  };
  private onButtonsChanged = (rect: window.TitleButtonRect): void => {
    // TitleButtonRect 使用 vp，right 是相对于窗口右上角的边界距离。
    this.buttonsWidth = Math.max(0, rect.right) + rect.width;
    if (this.mainWindow) {
      this.barHeight = this.mainWindow.getWindowDecorHeight();
    }
  };

  aboutToAppear(): void {
    this.active = true;
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    window.getLastWindow(context).then((mainWindow: window.Window) => {
      if (!this.active) {
        return;
      }
      this.mainWindow = mainWindow;
      mainWindow.on('windowStatusChange', this.onStatusChanged);
      this.onStatusChanged(mainWindow.getWindowStatus());
      this.onButtonsChanged(mainWindow.getTitleButtonRect());
      mainWindow.on('windowTitleButtonRectChange', this.onButtonsChanged);
    }).catch((error: Error) => this.reportError(error));
  }

  aboutToDisappear(): void {
    this.active = false;
    try {
      this.mainWindow?.off('windowStatusChange', this.onStatusChanged);
      this.mainWindow?.off('windowTitleButtonRectChange', this.onButtonsChanged);
    } catch (error) {
      this.reportError(error as Error);
    }
    this.mainWindow = undefined;
  }

  private reportError(error: Error): void {
    hilog.warn(0xFF00, 'HuiwenMCP', 'HUIWEN_HOME_TITLEBAR_FAILED: %{public}s', error.message);
  }

  private async openAboutDevice(): Promise<void> {
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      // 当前 SDK 使用系统设置的页面入口，直接定位到“关于本机”。
      await context.startAbility({
        bundleName: 'com.huawei.hmos.settings',
        abilityName: 'com.huawei.hmos.settings.MainAbility',
        uri: 'about_device'
      });
    } catch (error) {
      const failure = error as BusinessError;
      hilog.warn(0xFF00, 'HuiwenMCP',
        'HUIWEN_HOME_ABOUT_DEVICE_FAILED: code=%{public}d, message=%{public}s',
        failure.code, failure.message);
      this.getUIContext().getPromptAction().showToast({ message: '无法打开关于本机，请从系统设置进入' });
    }
  }

  private async toggleMaximize(): Promise<void> {
    if (!this.mainWindow) {
      return;
    }
    try {
      const status = this.mainWindow.getWindowStatus();
      // 跟随当前窗口设置，已铺满窗口时双击还原，不改动系统三键样式。
      if (status === window.WindowStatusType.MAXIMIZE ||
        status === window.WindowStatusType.FULL_SCREEN) {
        await this.mainWindow.recover();
      } else {
        await this.mainWindow.maximize();
      }
    } catch (error) {
      this.reportError(error as Error);
    }
  }

  build() {
    Row() {
      if (this.expanded) {
        // 独立点击区不参与右侧标题文字的拖动及双击手势。
        Button({ type: ButtonType.Normal }) {
          // 使用透明 SVG 原色，不随深色模式改色。
          Image($r('app.media.harmonyos_logo'))
            .width(108)
            .height(20)
            .objectFit(ImageFit.Contain)
            .draggable(false)
        }
        // 两侧各扩展 8vp，并回收外边距，保持字标和后续文字的原位置。
        .width(124)
        .height('100%')
        .margin({ left: 8 })
        .padding({ left: 8, right: 8 })
        .borderRadius(8)
        .backgroundColor(Color.Transparent)
        .accessibilityText('打开关于本机')
        .onClick(() => this.openAboutDevice())
      }
      Row({ space: 8 }) {
        if (this.expanded) {
          Text('X')
            .fontSize(16)
            .fontColor($r('app.color.home_text'))
            .margin({ left: 8, right: 8 })
        } else {
          Image($r('app.media.app_icon'))
            .width(20)
            .height(20)
            .objectFit(ImageFit.Contain)
            .draggable(false)
        }
        Text($r('app.string.app_name'))
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .fontColor($r('app.color.home_text'))
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .layoutWeight(1)
      }
      .layoutWeight(1)
      .height('100%')
      .padding({ left: this.expanded ? 0 : 16, right: 12 })
      .onTouch((event: TouchEvent) => {
        // Move 时再交给系统拖动，避免 Down 就接管导致双击手势失效。
        if (event.type === TouchType.Move) {
          this.mainWindow?.startMoving().catch((error: Error) => this.reportError(error));
        }
      })
      .gesture(TapGesture({ count: 2 }).onAction(() => this.toggleMaximize()))
      // 只避让系统控件，不复制三键图标或点击逻辑。
      Blank().width(this.buttonsWidth)
    }
    .width('100%')
    .height(this.barHeight)
    .flexShrink(0)
    .backgroundColor(Color.Transparent)
  }
}
