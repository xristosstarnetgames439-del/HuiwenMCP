interface QpaBridge {
  attachAbilityStage(stage: object): void;
  startQtApplication(ability: object, session?: object): void;
  handleOnNewWant(want: object, launchParam: object): void;
}

declare const qpa: QpaBridge;
export default qpa;
