// Qt SDK 的 Ability 启动接口，声明与本机 SDK 的 ArkTS 包装器保持一致。
import UIAbility from '@ohos.app.ability.UIAbility';
import AbilityStage from '@ohos.app.ability.AbilityStage';
import Want from '@ohos.app.ability.Want';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';

interface QtPlatform {
  attachAbilityStage(stage: AbilityStage): void;
  startQtApplication(ability: UIAbility): void;
  handleOnNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void;
}
declare const qpa: QtPlatform;
export default qpa;
