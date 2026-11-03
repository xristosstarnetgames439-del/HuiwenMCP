// 文件管理弹窗：组合左右区域，负责分类状态和关闭入口。
import { FileManagerSidebar } from './FileManagerSidebar';
import { FileManagerContent } from './FileManagerContent';

@Component
export struct FileManagerDialog {
  @Prop dialogWidth: number = 760;
  @State selectedCategory: string = '全部';
  onCloseRequested: () => void = () => {};

  build() {
    Stack({ alignContent: Alignment.TopEnd }) {
      Row() {
        FileManagerSidebar({
          selectedCategory: this.selectedCategory,
          onSelect: (category: string) => { this.selectedCategory = category; }
        })
          .flexShrink(0)
        FileManagerContent({ selectedCategory: this.selectedCategory })
          .layoutWeight(1)
      }
      .width('100%')
      .height('100%')
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
      .borderRadius(8)
      .backgroundColor(Color.Transparent)
      .accessibilityText('关闭文件管理')
      .onClick(() => this.onCloseRequested())
    }
    .width(this.dialogWidth)
    .height(480)
    .borderRadius(16)
    .clip(true)
  }
}
