// 独立键帽录入：分别修改修饰键和普通键，提交后由系统检查组合占用。
import { KeyCode } from '@kit.InputKit';
import { common } from '@kit.AbilityKit';
import { ScreenshotShortcut } from '../../services/settings/AppSettings';
import { CANCEL_SHORTCUT_RECORDING, setShortcutRecording,
  shortcutKeys } from '../../services/screenshot/ScreenshotService';

@Component
export struct ShortcutInput {
  @Prop shortcut: ScreenshotShortcut = { modifier: KeyCode.KEYCODE_CTRL_LEFT, key: KeyCode.KEYCODE_1 };
  @Prop controlId: string = '';
  @Prop available: boolean = true;
  @State recordingKey: number = 0;
  @State error: string = '';
  onChange: (shortcut: ScreenshotShortcut) => void = () => {};
  private cancelRecording = (): void => this.stopRecording();

  aboutToAppear(): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.getApplicationContext().eventHub.on(CANCEL_SHORTCUT_RECORDING, this.cancelRecording);
  }

  aboutToDisappear(): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.getApplicationContext().eventHub.off(CANCEL_SHORTCUT_RECORDING, this.cancelRecording);
    this.stopRecording();
  }

  private stopRecording(): void {
    if (this.recordingKey !== 0) {
      this.recordingKey = 0;
      try {
        setShortcutRecording(false);
      } catch (error) {
        this.error = (error as Error).message;
      }
    }
  }

  private record(event: KeyEvent): void {
    if (this.recordingKey === 0 || event.type !== KeyType.Down) {
      return;
    }
    event.stopPropagation();
    if (event.keyCode === KeyCode.KEYCODE_ESCAPE) {
      this.stopRecording();
      return;
    }
    const key = event.keyCode;
    const modifier = key === KeyCode.KEYCODE_CTRL_LEFT || key === KeyCode.KEYCODE_CTRL_RIGHT ?
      KeyCode.KEYCODE_CTRL_LEFT : key === KeyCode.KEYCODE_ALT_LEFT || key === KeyCode.KEYCODE_ALT_RIGHT ?
        KeyCode.KEYCODE_ALT_LEFT : key === KeyCode.KEYCODE_SHIFT_LEFT || key === KeyCode.KEYCODE_SHIFT_RIGHT ?
          KeyCode.KEYCODE_SHIFT_LEFT : 0;
    if (this.recordingKey === 1 && modifier === 0) {
      this.error = '左侧请选择 Ctrl、Alt 或 Shift';
      return;
    }
    if (this.recordingKey === 2 && (modifier !== 0 || key === KeyCode.KEYCODE_META_LEFT ||
      key === KeyCode.KEYCODE_META_RIGHT)) {
      this.error = '右侧请按字母、数字或其他普通按键';
      return;
    }
    const shortcut: ScreenshotShortcut = {
      modifier: this.recordingKey === 1 ? modifier : this.shortcut.modifier,
      key: this.recordingKey === 2 ? key : this.shortcut.key,
      keyName: this.recordingKey === 2 ? event.keyText : this.shortcut.keyName
    };
    // 成功录入只退出控件编辑态；新组合保存完成前不恢复旧订阅。
    this.recordingKey = 0;
    this.error = '';
    this.onChange(shortcut);
  }

  private beginRecording(position: number): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.getApplicationContext().eventHub.emit(CANCEL_SHORTCUT_RECORDING);
    this.error = '';
    this.recordingKey = position;
    try {
      setShortcutRecording(true);
      this.getUIContext().getFocusController().requestFocus(`${this.controlId}-${position}`);
    } catch (error) {
      this.stopRecording();
      this.error = (error as Error).message;
    }
  }

  @Builder
  keycap(position: number) {
    Button(this.recordingKey === position ? '请按下键值' : shortcutKeys(this.shortcut)[position - 1],
      { type: ButtonType.Normal })
      .id(`${this.controlId}-${position}`)
      .width(this.recordingKey === position ? 144 : Math.max(position === 1 ? 64 : 52,
        Math.min(144, shortcutKeys(this.shortcut)[position - 1].length * 9 + 20)))
      .height(36)
      .fontSize(14)
      .fontWeight(FontWeight.Normal)
      .fontColor($r('app.color.home_text'))
      .backgroundColor($r('app.color.home_background'))
      .border({ width: 1, color: this.recordingKey === position ? '#3983F7' : $r('app.color.home_border') })
      .borderRadius(8)
      .padding({ left: 10, right: 10 })
      .enabled(this.available)
      .focusable(true)
      .accessibilityText(`${position === 1 ? '修饰键' : '普通按键'}：${shortcutKeys(this.shortcut)[position - 1]}`)
      .onClick(() => this.beginRecording(position))
      .onBlur(() => {
        if (this.recordingKey === position) { this.stopRecording(); }
      })
      .onKeyEvent((event: KeyEvent) => this.record(event))
  }

  build() {
    Column({ space: 4 }) {
      Row({ space: 6 }) {
        this.keycap(1)
        Text('+')
          .width(12)
          .fontSize(14)
          .fontColor($r('app.color.home_muted'))
          .textAlign(TextAlign.Center)
        this.keycap(2)
      }
      if (this.error !== '') {
        Text(this.error)
          .fontSize(12)
          .fontColor($r('app.color.home_warning'))
      }
    }
    .alignItems(HorizontalAlign.Start)
  }
}
