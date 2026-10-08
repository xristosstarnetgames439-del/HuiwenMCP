import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import UIAbility from '@ohos.app.ability.UIAbility';
import Window from '@ohos.window'
import Want from '@ohos.app.ability.Want';
import hilog from '@ohos.hilog';


export default class EntryAbility extends UIAbility {
  tag = "HuiWenMCP";
  domain = 0xFF00;

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam) {
    super.onCreate(want, launchParam)
  }

  newLocalStorage() {
    return new LocalStorage()
  }

  onDestroy() {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onDestroy');
  }

  onWindowStageCreate(windowStage: Window.WindowStage) {
    // 启动后直接加载 ArkTS 首页。
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onWindowStageCreate');
    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(this.domain, this.tag, 'Failed to load page: %{public}s', JSON.stringify(err));
      }
    });
  }

  onWindowStageDestroy() {
    // Main window is destroyed, release UI related resources
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground() {
    // Ability has brought to foreground
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onForeground');
  }

  onBackground() {
    // Ability has back to background
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onBackground');
  }

};
