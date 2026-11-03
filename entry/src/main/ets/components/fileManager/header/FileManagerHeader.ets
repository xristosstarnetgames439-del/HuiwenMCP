// 固定标题栏：标题由外部传入，搜索框右侧为弹窗关闭按钮预留独立空间。
@Component
export struct FileManagerHeader {
  @Prop title: string = '全部';
  @Prop keyword: string = '';
  onSearch: (keyword: string) => void = () => {};

  build() {
    Column() {
      Row({ space: 12 }) {
        Text(this.title)
          .fontSize(18)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.home_text'))
          .layoutWeight(1)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
        TextInput({ text: this.keyword, placeholder: '搜索文件名' })
          .width(200)
          .height(32)
          .fontSize(14)
          .fontColor($r('app.color.home_text'))
          .placeholderColor($r('app.color.home_muted'))
          .backgroundColor($r('app.color.home_card'))
          .borderRadius(8)
          .padding({ left: 12, right: 12 })
          .onChange((keyword: string) => this.onSearch(keyword))
      }
      .width('100%')
      .layoutWeight(1)
      .padding({ left: 16, right: 48 })
      Divider().color($r('app.color.home_border')).strokeWidth(1)
    }
    .width('100%')
    .height(64)
  }
}
