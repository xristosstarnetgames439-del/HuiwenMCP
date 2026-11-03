// 启动首页窄侧栏：背景半透明，底部菜单按钮只向页面发出点击事件。
@Component
export struct FunctionSidebar {
  @Prop menuOpen: boolean = false;
  onMenuRequested: () => void = () => {};
  onMenuAnchorChanged: (right: number, bottom: number) => void = () => {};

  build() {
    Column() {
      Blank().layoutWeight(1)
      Button({ type: ButtonType.Normal }) {
        // 本地 SVG 不依赖字符字体；零内边距保证窄按钮内图标完整显示。
        Image($r('app.media.sidebar_menu'))
          .width(24)
          .height(24)
          .objectFit(ImageFit.Contain)
          .draggable(false)
      }
      .width(36)
      .height(36)
      .padding(0)
      .backgroundColor(this.menuOpen ? $r('app.color.home_menu_hover') : Color.Transparent)
      .borderRadius(8)
      .hoverEffect(HoverEffect.Auto)
      .accessibilityText('更多功能')
      .id('home-menu-button')
      .onAreaChange((_oldArea: Area, area: Area) => {
        this.onMenuAnchorChanged(Number(area.globalPosition.x) + Number(area.width),
          Number(area.globalPosition.y) + Number(area.height));
      })
      .onClick(() => this.onMenuRequested())
    }
    .width(60)
    .height('100%')
    .padding({ top: 20, bottom: 20 })
    // 仅背景带透明度，避免菜单图标与按钮一起变淡。
    .backgroundColor($r('app.color.home_sidebar'))
    .border({ width: { right: 0.5 }, color: '#2453616D' })
  }
}
