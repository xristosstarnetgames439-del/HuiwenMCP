@Component
export struct HomeAboutDialog {
  @Prop version: string = '';
  onClose: () => void = () => {};

  build() {
    Stack({ alignContent: Alignment.TopEnd }) {
      Column({ space: 14 }) {
        Image($r('app.media.app_icon'))
          .width(64)
          .height(64)
          .draggable(false)
        Text('汇文MCP')
          .fontSize(24)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.home_text'))
        Text(this.version === '' ? '版本信息暂不可用' : `版本 ${this.version}`)
          .fontSize(14)
          .fontColor($r('app.color.home_muted'))
        Text('PDF 与 Markdown 的阅读和转换工具')
          .fontSize(16)
          .fontColor($r('app.color.home_text'))
          .textAlign(TextAlign.Center)
        Text('汇集千文 · 连通万物\n支持文字型 PDF 转换为 Markdown，让阅读与整理更轻松。')
          .fontSize(14)
          .lineHeight(22)
          .fontColor($r('app.color.home_muted'))
          .textAlign(TextAlign.Center)
      }
      .width('100%')
      .padding({ top: 36, bottom: 32, left: 28, right: 28 })
      Button({ type: ButtonType.Normal }) {
        Image($r('app.media.home_close'))
          .width(18)
          .height(18)
          .draggable(false)
      }
      .width(32)
      .height(32)
      .padding(0)
      .margin(8)
      .backgroundColor(Color.Transparent)
      .borderRadius(8)
      .accessibilityText('关闭关于')
      .onClick(() => this.onClose())
    }
    .width('100%')
  }
}
