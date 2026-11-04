// 屏幕级轻提示：独立 TYPE_FLOAT 窗口，不依赖首页的 UIContext。
import { abilityAccessCtrl, common } from '@kit.AbilityKit';
import { display, window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

let toastSequence: number = 0;

export async function showScreenToast(context: common.UIAbilityContext, message: string,
  displayId: number): Promise<void> {
  // 受限权限须由签名授权，不能通过 requestPermissionsFromUser 获取。
  const granted = abilityAccessCtrl.createAtManager().checkAccessTokenSync(
    context.applicationInfo.accessTokenId, 'ohos.permission.SYSTEM_FLOAT_WINDOW');
  if (granted !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
    throw new Error('SYSTEM_FLOAT_WINDOW 未授权，请检查签名 Profile 的悬浮窗权限');
  }
  const screen = display.getDisplayByIdSync(displayId);
  const density = screen.densityPixels;
  const width = Math.min(screen.width, Math.round(320 * density));
  const height = Math.min(screen.height, Math.round(48 * density));
  let toastWindow: window.Window | undefined = undefined;
  try {
    toastWindow = await window.createWindow({
      name: `huiwen_screen_toast_${++toastSequence}`,
      windowType: window.WindowType.TYPE_FLOAT,
      // loadContent 需要 Ability 的模块与页面信息，ApplicationContext 无法加载此页面。
      ctx: context,
      displayId: displayId
    });
    await toastWindow.resize(width, height);
    await toastWindow.moveWindowTo(Math.round((screen.width - width) / 2),
      Math.max(0, screen.height - height - Math.round(24 * density)));
    const storage = new LocalStorage();
    storage.setOrCreate('screenToastMessage', message);
    // 先加载 UIContent，再设透明背景，避免窗口尚未就绪。
    await toastWindow.loadContent('pages/ScreenToastPage', storage);
    toastWindow.setWindowBackgroundColor('#00000000');
    await toastWindow.setWindowFocusable(false);
    await toastWindow.setWindowTouchable(false);
    await toastWindow.showWindow();
    hilog.info(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREEN_TOAST_SHOWN: display=%{public}d', displayId);
    const shownWindow = toastWindow;
    // 计时器只持有提示窗口，不绑定 ScreenshotAbility 的生命周期。
    setTimeout(() => {
      shownWindow.destroyWindow().catch((error: Error) => {
        hilog.warn(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREEN_TOAST_CLOSE_FAILED: %{public}s', error.message);
      });
    }, 2000);
  } catch (error) {
    if (toastWindow) {
      await toastWindow.destroyWindow().catch((failure: Error) => {
        hilog.warn(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREEN_TOAST_CLOSE_FAILED: %{public}s', failure.message);
      });
    }
    throw error as Error;
  }
}
