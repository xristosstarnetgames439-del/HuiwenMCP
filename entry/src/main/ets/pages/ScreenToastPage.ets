// 独立提示浮窗的纯展示页面；窗口层负责位置、鼠标穿透和两秒后销毁。
@Entry(LocalStorage.getShared())
@Component
struct ScreenToastPage {
  @LocalStorageProp('screenToastMessage') message: string = '';

  build() {
    Row() {
      Text(this.message)
        .fontSize(16)
        .fontColor(Color.White)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
    }
    .width('100%')
    .height('100%')
    .padding({ left: 16, right: 16 })
    .justifyContent(FlexAlign.Center)
    .backgroundColor('#F5000000')
    .borderRadius(12)
    .clip(true)
  }
}
