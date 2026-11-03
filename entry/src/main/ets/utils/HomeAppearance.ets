import { common, ConfigurationConstant } from '@kit.AbilityKit';
import { preferences } from '@kit.ArkData';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

export enum HomeAppearance {
  System = 'system',
  Light = 'light',
  Dark = 'dark'
}

function setHomeColorMode(context: common.Context, appearance: HomeAppearance): void {
  const mode = appearance === HomeAppearance.Light ? ConfigurationConstant.ColorMode.COLOR_MODE_LIGHT :
    appearance === HomeAppearance.Dark ? ConfigurationConstant.ColorMode.COLOR_MODE_DARK :
      ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET;
  context.getApplicationContext().setColorMode(mode);
}

// 首页窗口共用侧栏 RGB；聚焦透明，失焦使用系统要求的不透明底色。
export function applyHomeWindowColors(context: common.Context, mainWindow: window.Window): void {
  try {
    const sidebar = context.resourceManager.getColorSync($r('app.color.home_sidebar').id);
    const inactiveColor = `#FF${(sidebar & 0xFFFFFF).toString(16).padStart(6, '0')}`;
    mainWindow.setWindowContainerColor('#00000000', inactiveColor);
  } catch (error) {
    hilog.warn(0xFF00, 'HuiwenMCP', 'HUIWEN_HOME_TRANSPARENT_FAILED: stage=container, message=%{public}s',
      (error as Error).message);
  }
}

export async function applyHomeAppearance(context: common.Context, appearance: HomeAppearance): Promise<void> {
  const settings = preferences.getPreferencesSync(context, { name: 'home_settings' });
  settings.putSync('appearance', appearance);
  await settings.flush();
  setHomeColorMode(context, appearance);
  // 手动 setColorMode 不触发 Ability 配置回调，在窗口取得后同步原生容器颜色。
  try {
    applyHomeWindowColors(context, await window.getLastWindow(context));
  } catch (error) {
    hilog.warn(0xFF00, 'HuiwenMCP', 'HUIWEN_HOME_COLOR_WINDOW_FAILED: %{public}s', (error as Error).message);
  }
}

export function initializeHomeAppearance(context: common.Context): void {
  // Ability 创建时显式读取磁盘，避免依赖尚未初始化的 UI 持久存储后端。
  const settings = preferences.getPreferencesSync(context, { name: 'home_settings' });
  const saved = settings.getSync('appearance', HomeAppearance.System);
  const appearance = saved === HomeAppearance.Light ? HomeAppearance.Light :
    saved === HomeAppearance.Dark ? HomeAppearance.Dark : HomeAppearance.System;
  AppStorage.setOrCreate('homeAppearance', appearance);
  setHomeColorMode(context, appearance);
}
