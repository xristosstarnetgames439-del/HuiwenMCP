// 首页和全部工作区共用一行：固定图标、格式与时间，文件名占据剩余空间。
import { RecentWorkspace } from '../../services/RecentWorkspaceService';

@Component
export struct RecentWorkspaceRow {
  @Prop workspace: RecentWorkspace;
  @Prop @Watch('clearHover') available: boolean = true;
  @State hovered: boolean = false;
  onOpen: (workspace: RecentWorkspace) => void = () => {};

  private clearHover(): void {
    this.hovered = false;
  }

  aboutToDisappear(): void {
    this.clearHover();
  }

  private modeIcon(mode: string): Resource {
    if (mode === 'preview') {
      return $r('app.media.home_preview');
    }
    return mode === 'edit' ? $r('app.media.home_edit') : $r('app.media.home_convert');
  }

  build() {
    Button({ type: ButtonType.Normal }) {
      Row({ space: 12 }) {
        Stack() {
          Image(this.modeIcon(this.workspace.mode))
            .width(18)
            .height(18)
            .objectFit(ImageFit.Contain)
            .draggable(false)
        }
        .width(28)
        .height(28)
        .flexShrink(0)
        .backgroundColor($r('app.color.home_card'))
        .borderRadius(8)

        Text(this.workspace.name)
          .layoutWeight(1)
          .constraintSize({ minWidth: 0 })
          .fontSize(14)
          .fontWeight(FontWeight.Normal)
          .fontColor($r('app.color.home_text'))
          .textAlign(TextAlign.Start)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .ellipsisMode(EllipsisMode.CENTER)

        Row({ space: 8 }) {
          Text(this.workspace.formatLabel)
            .width(104)
            .flexShrink(0)
            .fontSize(12)
            .fontWeight(FontWeight.Normal)
            .fontColor($r('app.color.home_secondary'))
            .textAlign(TextAlign.Start)
            .maxLines(1)
          Text(this.workspace.timeLabel)
            .width(152)
            .flexShrink(0)
            .fontSize(11)
            .fontWeight(FontWeight.Normal)
            .fontColor($r('app.color.home_muted'))
            .textAlign(TextAlign.End)
            .maxLines(1)
        }
        .flexShrink(0)
      }
      .width('100%')
      .height('100%')
      .padding({ right: 12 })
      .alignItems(VerticalAlign.Center)
      // 子组件不接管鼠标命中，空白和文字都由外层整行按钮处理。
      .hitTestBehavior(HitTestMode.None)
    }
    .width('100%')
    .height(48)
    .padding(0)
    .backgroundColor(this.hovered && this.available ? $r('app.color.home_card') : Color.Transparent)
    .borderRadius(8)
    // 仅 hover 控制背景，禁用按钮自带的焦点／按压残留效果。
    .hoverEffect(HoverEffect.None)
    .stateEffect(false)
    .enabled(this.available && this.workspace.canOpen)
    .onHover((hovered: boolean) => { this.hovered = hovered; })
    .accessibilityText(`${this.workspace.name}，${this.workspace.formatLabel}，${this.workspace.timeLabel}`)
    .onClick(() => {
      this.clearHover();
      this.onOpen(this.workspace);
    })
  }

}
