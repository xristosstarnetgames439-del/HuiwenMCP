// 首页功能卡片：浅灰方块只显示 SVG，普通字重的名称放在方块下方。
@Component
export struct HomeFeatureCard {
  @Prop title: string = '';
  @Prop icon: Resource = $r('app.media.home_preview');
  @Prop selected: boolean = false;
  @Prop available: boolean = true;
  onSelect: () => void = () => {};

  build() {
    Column({ space: 12 }) {
      Button({ type: ButtonType.Normal }) {
        Image(this.icon)
          .width('40%')
          .aspectRatio(1)
          .constraintSize({ maxWidth: 64, maxHeight: 64 })
          .objectFit(ImageFit.Contain)
          .draggable(false)
      }
      // 每个入口仍占等宽列，方块边长缩为原来整列宽度的一半。
      .width('50%')
      .aspectRatio(1)
      .padding(0)
      .borderRadius(16)
      .backgroundColor($r('app.color.home_card'))
      .border({ width: this.selected ? 2 : 1, color: this.selected ? $r('app.color.home_selected') : $r('app.color.home_border') })
      .hoverEffect(HoverEffect.Auto)
      .enabled(this.available)
      .accessibilityText(this.title)
      .onClick(() => this.onSelect())
      Text(this.title)
        .fontSize(16)
        .minFontSize(12)
        .maxFontSize(16)
        .maxLines(1)
        .fontWeight(FontWeight.Normal)
        .fontColor($r('app.color.home_text'))
        .textAlign(TextAlign.Center)
        .width('100%')
    }
    .width('100%')
    .alignItems(HorizontalAlign.Center)
  }
}
