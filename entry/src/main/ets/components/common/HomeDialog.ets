// 首页共用弹窗内容壳：标题、关闭按钮和内边距；背景由系统背板统一绘制。
@Component
export struct HomeDialog {
  @Prop title: string = '';
  @Prop dialogWidth: number = 360;
  // 列表弹窗可铺到背板右缘，由数据行自行预留滚动条空间。
  @Prop contentRightPadding: number = 24;
  @BuilderParam content: () => void;
  onCloseRequested: () => void = () => {};

  build() {
    Stack({ alignContent: Alignment.TopEnd }) {
      Column({ space: 16 }) {
        Text(this.title)
          .fontSize(18)
          .fontWeight(FontWeight.Normal)
          .fontColor($r('app.color.home_text'))
          .width('100%')
          .textAlign(TextAlign.Start)
        this.content()
      }
      .width('100%')
      .padding({ top: 40, right: this.contentRightPadding, bottom: 24, left: 24 })
      .alignItems(HorizontalAlign.Center)
      Button({ type: ButtonType.Normal }) {
        Image($r('app.media.home_close'))
          .width(18)
          .height(18)
          .objectFit(ImageFit.Contain)
          .draggable(false)
      }
      .width(28)
      .height(28)
      .padding(0)
      .margin({ top: 8, right: 8 })
      .backgroundColor(Color.Transparent)
      .borderRadius(8)
      .hoverEffect(HoverEffect.Auto)
      .accessibilityText(`关闭${this.title}窗口`)
      .onClick(() => this.onCloseRequested())
    }
    // 系统背板与内容必须传入相同宽度，避免出现透明外框。
    .width(this.dialogWidth)
    .backgroundColor(Color.Transparent)
  }
}
