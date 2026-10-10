// 应用级 Qt 平台初始化；首页仍由 ArkTS 的 EntryAbility 加载。
import { AbilityStage } from '@kit.AbilityKit';
import qpa from 'libplugins_platforms_qopenharmony.so';

export default class QtAbilityStage extends AbilityStage {
  onCreate(): void {
    qpa.attachAbilityStage(this);
  }
}
