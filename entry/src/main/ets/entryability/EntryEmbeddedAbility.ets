import { hilog } from '@kit.PerformanceAnalysisKit';
import { UIExtensionContentSession, Want } from '@kit.AbilityKit';
import EmbeddedUIExtensionAbility from '@ohos.app.ability.EmbeddedUIExtensionAbility';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import qpa from 'libplugins_platforms_qopenharmony.so'

export default class EntryEmbeddedAbility extends EmbeddedUIExtensionAbility {
  tag = "QtForOpenHarmony";
  domain = 0x0000;
  private name: string = "qt_entry_window"
  private launchParams = ""
  private launchApplication = "libentry.so"
  private launchWant: Want  = {}
  onCreate(launchParam: AbilityConstant.LaunchParam) {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onCreate');
  }

  onForeground() {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onForeground');
  }

  onBackground() {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onBackground');
  }

  onDestroy() {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onDestroy');
  }

  onSessionCreate(want: Want, session: UIExtensionContentSession) {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onSessionCreate');
    this.launchWant = want;
    this.launchParamSet();
    qpa.startQtApplication(this, session);
  }

  newLocalStorage() {
    return new LocalStorage();
  }

  async launchParamSet(){
    let params:string[] = [];

    //NOTE：应用启动参数在此处添加
    this.launchParams = params.join(' ');
  }
  onSessionDestroy(session: UIExtensionContentSession) {
    hilog.info(this.domain, this.tag, '%{public}s', 'Ability onSessionDestroy');
  }
};
