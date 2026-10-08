import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import UIAbility from '@ohos.app.ability.UIAbility';
import Window from '@ohos.window'
import Want from '@ohos.app.ability.Want';
import qpa from 'libplugins_platforms_qopenharmony.so'
import hilog from '@ohos.hilog';


export default class EntryAbility extends UIAbility {
  tag = "QtForOpenHarmony";
  domain = 0xFF00;
  private launchApplication = "libentry.so"
  private launchParams = ""

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
    // Main window is created, set main page for this ability
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onWindowStageCreate');
    this.launchParamSet();
    qpa.startQtApplication(this);
  }

  async launchParamSet(){
    let params:string[] = [];

    //NOTE：应用启动参数在此处使用params.push添加
    this.launchParams = params.join(' ');
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

  /**
   * note：启动模式为singleton/specified才生效
   * @param want
   * @param launchParam
   */
  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    qpa.handleOnNewWant(want, launchParam);
  }
};
