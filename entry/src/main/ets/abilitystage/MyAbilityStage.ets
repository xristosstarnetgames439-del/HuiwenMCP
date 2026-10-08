import { AbilityStage } from '@kit.AbilityKit';
import qpa from 'libplugins_platforms_qopenharmony.so'

export default class MyAbilityStage extends AbilityStage {
  onCreate(): void {
    qpa.attachAbilityStage(this);
  }
}
