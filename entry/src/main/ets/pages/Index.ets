import nativeBridge from 'libhuiwen_native.so';

@Entry
@Component
struct Index {
  @State pingResult: string = '待检测';
  @State selectedAction: string = '';

  aboutToAppear(): void {
    // 首页打开时检查 ArkTS 是否能调用 C++ NAPI 模块。
    this.pingResult = nativeBridge.ping();
  }

  build() {
    Row({ space: 24 }) {
      // 左侧暂用文字代替应用宣传图。
      Column({ space: 20 }) {
        Text('汇文 MCP')
          .fontSize(48)
          .fontWeight(FontWeight.Bold)
          .fontColor('#183153')
        Text('PDF 与 Markdown 的阅读和转换工具')
          .fontSize(20)
          .fontColor('#4B6080')
        Text('汇集千文 · 连通万物')
          .fontSize(16)
          .fontColor('#667B98')
      }
      .layoutWeight(1)
      .height('100%')
      .padding(48)
      .justifyContent(FlexAlign.Center)
      .alignItems(HorizontalAlign.Start)
      .backgroundColor('#EAF2FF')
      .borderRadius(24)

      Column({ space: 18 }) {
        Text('开始使用')
          .fontSize(28)
          .fontWeight(FontWeight.Bold)
          .fontColor('#183153')
        Text('选择需要的功能')
          .fontSize(16)
          .fontColor('#667B98')

        Button('PDF 预览')
          .width('100%')
          .height(60)
          .onClick(() => {
            this.selectedAction = 'PDF 预览';
          })
        Button('MD 预览')
          .width('100%')
          .height(60)
          .onClick(() => {
            this.selectedAction = 'MD 预览';
          })
        Button('PDF 转 MD')
          .width('100%')
          .height(60)
          .onClick(() => {
            this.selectedAction = 'PDF 转 MD';
          })

        if (this.selectedAction !== '') {
          Text(`已选择${this.selectedAction}，功能将在后续步骤接入。`)
            .fontSize(14)
            .fontColor('#667B98')
        }
        if (this.pingResult !== 'NAPI OK') {
          Text('原生模块尚未就绪')
            .fontSize(13)
            .fontColor('#B45309')
        }
      }
      .width(360)
      .height('100%')
      .padding(32)
      .justifyContent(FlexAlign.Center)
      .alignItems(HorizontalAlign.Start)
      .backgroundColor('#FFFFFF')
      .borderRadius(24)
    }
    .width('100%')
    .height('100%')
    .padding(24)
    .backgroundColor('#F5F7FB')
  }
}
