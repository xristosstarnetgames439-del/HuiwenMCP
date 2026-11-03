// 文件管理左侧导航：只上报分类，选中状态由弹窗统一维护。
@Component
export struct FileManagerSidebar {
  @Prop selectedCategory: string = '全部';
  @State hoveredCategory: string = '';
  onSelect: (category: string) => void = () => {};

  @Builder
  categoryItem(label: string, icon: Resource, iconSize: number = 18) {
    Button({ type: ButtonType.Normal }) {
      Row({ space: 12 }) {
        Image(icon)
          .width(iconSize)
          .height(iconSize)
          .objectFit(ImageFit.Contain)
          .draggable(false)
        Text(label)
          .fontSize(14)
          .fontColor($r('app.color.home_text'))
          .layoutWeight(1)
          .textAlign(TextAlign.Start)
      }
      .width('100%')
      .height('100%')
      .hitTestBehavior(HitTestMode.None)
    }
    .width('100%')
    .height(40)
    .padding({ left: 16, right: 16 })
    .borderRadius(0)
    .backgroundColor(this.selectedCategory === label ? $r('app.color.home_border') :
      this.hoveredCategory === label ? $r('app.color.home_menu_hover') : Color.Transparent)
    .hoverEffect(HoverEffect.None)
    .stateEffect(false)
    .accessibilityText(`${label}${this.selectedCategory === label ? '，已选择' : ''}`)
    .onHover((hovered: boolean) => { this.hoveredCategory = hovered ? label : ''; })
    .onClick(() => this.onSelect(label))
  }

  build() {
    Column() {
      this.categoryItem('全部', $r('app.media.menu_folder'))
      this.categoryItem('输入文件', $r('app.media.file_manager_input'))
      this.categoryItem('输出文件', $r('app.media.file_manager_output'))
      Text('文件类型')
        .fontSize(12)
        .fontColor($r('app.color.home_muted'))
        .width('100%')
        .padding({ left: 16, right: 16, bottom: 10 })
        .margin({ top: 24 })
      this.categoryItem('Word', $r('app.media.home_format_word'), 20)
      this.categoryItem('PDF', $r('app.media.home_format_pdf'), 20)
      this.categoryItem('Markdown', $r('app.media.home_format_md'), 20)
    }
    .width(176)
    .height('100%')
    .padding({ top: 24 })
    .backgroundColor($r('app.color.home_card'))
  }
}
