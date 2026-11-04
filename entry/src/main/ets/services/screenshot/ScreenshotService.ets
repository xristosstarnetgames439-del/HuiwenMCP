// 截图入口与全局快捷键集中管理，避免把监听器绑定到会反复销毁的首页组件。
import { AbilityConstant, abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';
import { inputConsumer, KeyCode } from '@kit.InputKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import nativeBridge from 'libhuiwen_native.so';
import { AppSettings, readSettings, SETTINGS_CHANGED, ScreenshotShortcut } from '../settings/AppSettings';

export const BEFORE_SCREENSHOT: string = 'huiwen.screenshot.before';
export const CANCEL_SHORTCUT_RECORDING: string = 'huiwen.shortcut.cancelRecording';

interface ShortcutBinding {
  options: inputConsumer.HotkeyOptions;
  callback: (options: inputConsumer.HotkeyOptions) => void;
}

let host: common.UIAbilityContext | undefined = undefined;
let stage: window.WindowStage | undefined = undefined;
let hiddenWindow: window.Window | undefined = undefined;
let busy: boolean = false;
let initialized: boolean = false;
let bindings: ShortcutBinding[] = [];

export function shortcutKeys(shortcut: ScreenshotShortcut): string[] {
  const prefix = shortcut.modifier === KeyCode.KEYCODE_ALT_LEFT ? 'Alt' :
    shortcut.modifier === KeyCode.KEYCODE_SHIFT_LEFT ? 'Shift' : 'Ctrl';
  let name = shortcut.keyName || String(shortcut.key);
  if (shortcut.key >= KeyCode.KEYCODE_0 && shortcut.key <= KeyCode.KEYCODE_9) {
    name = String(shortcut.key - KeyCode.KEYCODE_0);
  } else if (shortcut.key >= KeyCode.KEYCODE_A && shortcut.key <= KeyCode.KEYCODE_Z) {
    name = String.fromCharCode(65 + shortcut.key - KeyCode.KEYCODE_A);
  } else if (shortcut.key >= KeyCode.KEYCODE_F1 && shortcut.key <= KeyCode.KEYCODE_F12) {
    name = `F${shortcut.key - KeyCode.KEYCODE_F1 + 1}`;
  } else if (shortcut.key === KeyCode.KEYCODE_SPACE) {
    name = 'Space';
  } else if (shortcut.key === KeyCode.KEYCODE_ENTER) {
    name = 'Enter';
  }
  return [prefix, name];
}

function binding(shortcut: ScreenshotShortcut, hideApplication: boolean): ShortcutBinding {
  return {
    options: { preKeys: [shortcut.modifier], finalKey: shortcut.key, isRepeat: false },
    callback: (options: inputConsumer.HotkeyOptions): void => {
      if (AppStorage.get<boolean>('shortcutRecording') || AppStorage.get<boolean>('storageBusy')) {
        return;
      }
      if (!host || AppStorage.get<boolean>('documentWorkspaceActive')) {
        // 现有 Qt/NAPI 只允许一份文档或截图会话，不能用截图覆盖正在运行的工作区。
        hilog.warn(0xFF00, 'HuiwenScreenshot', 'HUIWEN_HOTKEY_UNAVAILABLE: 文档工作区打开期间暂不启动截图');
        return;
      }
      hilog.info(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SHORTCUT_TRIGGERED: modifier=%{public}d key=%{public}d',
        options.preKeys[0], options.finalKey);
      startScreenshot(hideApplication).catch((error: Error) => reportFailure(error));
    }
  };
}

// 先验证新组合可注册；失败时恢复旧组合，设置页只有成功后才保存。
export function applyScreenshotShortcuts(settings: AppSettings): void {
  if (settings.hiddenShortcut.modifier === settings.screenShortcut.modifier &&
    settings.hiddenShortcut.key === settings.screenShortcut.key) {
    throw new Error('两个截图快捷键不能相同');
  }
  const previous = bindings;
  const next: ShortcutBinding[] = [binding(settings.hiddenShortcut, true), binding(settings.screenShortcut, false)];
  // 修改条数或路径等其他设置，不重新抢占已有快捷键。
  if (previous.length === next.length && next.every((item: ShortcutBinding, index: number): boolean =>
    item.options.finalKey === previous[index].options.finalKey &&
    item.options.preKeys[0] === previous[index].options.preKeys[0])) {
    return;
  }
  previous.forEach((item: ShortcutBinding) => inputConsumer.off('hotkeyChange', item.options, item.callback));
  const added: ShortcutBinding[] = [];
  try {
    next.forEach((item: ShortcutBinding) => {
      inputConsumer.on('hotkeyChange', item.options, item.callback);
      added.push(item);
    });
    bindings = next;
    hilog.info(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SHORTCUT_APPLIED: hidden=%{public}d screen=%{public}d',
      settings.hiddenShortcut.key, settings.screenShortcut.key);
  } catch (error) {
    added.forEach((item: ShortcutBinding) => inputConsumer.off('hotkeyChange', item.options, item.callback));
    previous.forEach((item: ShortcutBinding) => inputConsumer.on('hotkeyChange', item.options, item.callback));
    throw new Error(`快捷键已被占用或不受系统支持：${(error as Error).message}`);
  }
}

export function setShortcutRecording(active: boolean): void {
  AppStorage.setOrCreate('shortcutRecording', active);
  if (active) {
    // 录入时取消本应用的订阅，让原有 Ctrl+1 等组合也能到达输入控件。
    bindings.forEach((item: ShortcutBinding) => inputConsumer.off('hotkeyChange', item.options, item.callback));
    bindings = [];
  } else if (host) {
    applyScreenshotShortcuts(readSettings(host));
  }
}

// 提交时调用方已注册新组合或回滚旧组合，只解除屏蔽，不能再次读配置覆盖订阅。
export function finishShortcutRecording(): void {
  AppStorage.setOrCreate('shortcutRecording', false);
}

function reportFailure(error: Error): void {
  hilog.error(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SCREENSHOT_START_FAILED: %{public}s', error.message);
  if (stage) {
    stage.getMainWindowSync().getUIContext().getPromptAction().showToast({ message: error.message });
  }
}

export function registerScreenshotHost(context: common.UIAbilityContext, windowStage: window.WindowStage): void {
  host = context;
  stage = windowStage;
  const application = context.getApplicationContext();
  const reload = (): void => {
    if (!host || AppStorage.get<boolean>('shortcutRecording')) {
      return;
    }
    try {
      applyScreenshotShortcuts(readSettings(application));
    } catch (error) {
      reportFailure(error as Error);
    }
  };
  if (!initialized) {
    initialized = true;
    application.eventHub.on(SETTINGS_CHANGED, reload);
  }
  reload();
}

export function releaseScreenshotHost(context: common.UIAbilityContext): void {
  if (host === context) {
    host = undefined;
    stage = undefined;
    // 页面真正结束时释放订阅，应用退出后不继续占用系统组合键；后台隐藏不走这里。
    bindings.forEach((item: ShortcutBinding) => {
      try {
        inputConsumer.off('hotkeyChange', item.options, item.callback);
      } catch (error) {
        hilog.warn(0xFF00, 'HuiwenScreenshot', 'HUIWEN_SHORTCUT_RELEASE_FAILED: %{public}s',
          (error as Error).message);
      }
    });
    bindings = [];
  }
}

export function isScreenshotRunning(): boolean {
  return busy;
}

async function minimizeBeforeCapture(mainWindow: window.Window): Promise<void> {
  if (!mainWindow.isWindowShowing()) {
    return;
  }
  // 等待真实可见性回执，不按固定延迟猜测最小化动画结束。
  let timer: number = -1;
  let onVisibility: (visible: boolean) => void = () => {};
  const hidden = new Promise<void>((resolve, reject) => {
    onVisibility = (visible: boolean): void => {
      if (!visible) {
        resolve();
      }
    };
    mainWindow.on('windowVisibilityChange', onVisibility);
    timer = setTimeout(() => reject(new Error('等待应用隐藏超时，请重试截图')), 1500);
  });
  hiddenWindow = mainWindow;
  try {
    await mainWindow.minimize();
    await hidden;
  } finally {
    clearTimeout(timer);
    mainWindow.off('windowVisibilityChange', onVisibility);
  }
}

export async function startScreenshot(hideApplication: boolean = false): Promise<void> {
  if (busy || AppStorage.get<boolean>('storageBusy')) {
    return;
  }
  const context = host;
  const windowStage = stage;
  if (!context || !windowStage || AppStorage.get<boolean>('documentWorkspaceActive')) {
    throw new Error('请先返回首页再启动截图');
  }
  busy = true;
  try {
    const access = abilityAccessCtrl.createAtManager();
    // 已授权时不再请求权限弹窗，首页在后台时也能直接抓屏。
    if (access.checkAccessTokenSync(context.applicationInfo.accessTokenId,
      'ohos.permission.CUSTOM_SCREEN_CAPTURE') !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
      const permissions: Permissions[] = ['ohos.permission.CUSTOM_SCREEN_CAPTURE'];
      const result = await access.requestPermissionsFromUser(context, permissions);
      if (result.authResults.length === 0 || result.authResults[0] !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        throw new Error('未获得屏幕截图权限');
      }
    }
    context.getApplicationContext().eventHub.emit(BEFORE_SCREENSHOT);
    if (hideApplication) {
      await minimizeBeforeCapture(windowStage.getMainWindowSync());
    }
    await nativeBridge.prepareScreenshot();
    await context.startAbility({ bundleName: 'cn.com.HuiWenMCP', abilityName: 'QtScreenshotAbility' }, {
      windowMode: AbilityConstant.WindowMode.WINDOW_MODE_FULLSCREEN, withAnimation: false
    });
  } catch (error) {
    nativeBridge.clearPreparedScreenshot();
    finishScreenshot();
    throw new Error((error as Error).message);
  }
}

export function finishScreenshot(): void {
  busy = false;
  const previous = hiddenWindow;
  hiddenWindow = undefined;
  if (previous) {
    previous.restore().catch((error: Error) => reportFailure(error));
  }
}
