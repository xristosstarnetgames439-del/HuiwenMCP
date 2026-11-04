// 最近色值胶囊：展示复制时的文本和真实颜色，剪贴板及历史更新由上层服务处理。
import { ScreenshotColor } from '../../services/screenshot/ScreenshotHistory';

@Component
export struct RecentColorList {
  @Prop colors: ScreenshotColor[] = [];
  onCopy: (item: ScreenshotColor) => void = () => {};

  build() {
    Flex({ wrap: FlexWrap.Wrap }) {
      if (this.colors.length === 0) {
        Text('暂无最近色值')
          .fontSize(14)
          .fontColor($r('app.color.home_muted'))
      }
      ForEach(this.colors, (item: ScreenshotColor) => {
        Button({ type: ButtonType.Normal }) {
          Row({ space: 8 }) {
            Text(item.value)
              .fontSize(18)
              .fontFamily('monospace')
              .fontColor($r('app.color.home_text'))
            Row()
              .width(20)
              .height(20)
              .backgroundColor(item.color)
              .border({ width: 1, color: $r('app.color.home_border') })
              .borderRadius(4)
          }
          .hitTestBehavior(HitTestMode.None)
        }
        .height(36)
        .padding({ left: 10, right: 10 })
        .margin({ right: 10, bottom: 10 })
        .backgroundColor($r('app.color.home_menu_hover'))
        .borderRadius(10)
        .accessibilityText(`复制色值 ${item.value}`)
        .onClick(() => this.onCopy(item))
      }, (item: ScreenshotColor) => `${item.color}-${item.createdAt}`)
    }
    .width('100%')
  }
}
