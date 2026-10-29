import { KeyCode } from '@kit.InputKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { HomeAppearance } from '../utils/HomeAppearance';

export const HOME_MENU_WIDTH: number = 160;
export const HOME_APPEARANCE_WIDTH: number = 152;
export const HOME_MENU_GAP: number = 6;

@Component
export struct HomeMoreMenu {
  @Prop appearance: HomeAppearance = HomeAppearance.System;
  @Prop availableWidth: number = 640;
  @State appearanceOpen: boolean = false;
  @State hoveredItem: string = '';
  onClose: () => void = () => {};
  onAbout: () => void = () => {};
  onAppearance: (appearance: HomeAppearance) => void = () => {};
  onPlaceholder: (name: string) => void = () => {};

  private focusMenu(): void {
    try {
      this.getUIContext().getFocusController().requestFocus('home-more-menu');
    } catch (error) {
      hilog.warn(0xFF00, 'HuiwenMCP', 'Menu focus failed: %{public}s', (error as Error).message);
    }
  }

  @Builder
  menuItem(label: string, icon: Resource, action: () => void, expandable: boolean = false) {
    Button({ type: ButtonType.Normal }) {
      Row({ space: 8 }) {
        Image(icon)
          .width(14)
          .height(14)
          .draggable(false)
        Text(label)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.home_text'))
          .layoutWeight(1)
          .textAlign(TextAlign.Start)
        if (expandable) {
          Image($r('app.media.menu_chevron'))
            .width(14)
            .height(14)
            .draggable(false)
        }
      }
      .width('100%')
    }
    .height(36)
    .width('100%')
    .padding({ left: 8, right: 8 })
    .borderRadius(6)
    .backgroundColor(this.hoveredItem === label || (expandable && this.appearanceOpen) ?
      $r('app.color.home_menu_hover') : Color.Transparent)
    .hoverEffect(HoverEffect.None)
    .accessibilityText(expandable ? `${label}，打开子菜单` : label)
    .onHover((hovered: boolean) => { this.hoveredItem = hovered ? label : ''; })
    .onClick(() => action())
  }

  @Builder
  separator() {
    Divider()
      .color($r('app.color.home_border'))
      .strokeWidth(1)
      .margin({ top: 3, bottom: 3, left: 8, right: 8 })
  }

  @Builder
  appearanceItem(label: string, icon: Resource, mode: HomeAppearance) {
    Button({ type: ButtonType.Normal }) {
      Row({ space: 8 }) {
        Image(icon)
          .width(14)
          .height(14)
          .draggable(false)
        Text(label)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.home_text'))
          .layoutWeight(1)
          .textAlign(TextAlign.Start)
        if (this.appearance === mode) {
          Image($r('app.media.menu_check'))
            .width(14)
            .height(14)
            .draggable(false)
        }
      }
      .width('100%')
    }
    .width('100%')
    .height(36)
    .padding({ left: 8, right: 8 })
    .borderRadius(6)
    .backgroundColor(this.hoveredItem === label ? $r('app.color.home_menu_hover') : Color.Transparent)
    .hoverEffect(HoverEffect.None)
    .accessibilityText(`${label}${this.appearance === mode ? '，已选择' : ''}`)
    .onHover((hovered: boolean) => { this.hoveredItem = hovered ? label : ''; })
    .onClick(() => this.onAppearance(mode))
  }

  @Builder
  appearancePanel() {
    Column() {
      // 窄窗口时在原面板内展示子菜单，避免超出窗口右侧。
      if (this.availableWidth < HOME_MENU_WIDTH + HOME_APPEARANCE_WIDTH + HOME_MENU_GAP) {
        this.menuItem('返回', $r('app.media.menu_back'), () => { this.appearanceOpen = false; })
        this.separator()
      }
      this.appearanceItem('浅色', $r('app.media.menu_sun'), HomeAppearance.Light)
      this.appearanceItem('深色', $r('app.media.menu_moon'), HomeAppearance.Dark)
      this.appearanceItem('跟随系统', $r('app.media.menu_monitor'), HomeAppearance.System)
    }
    .width(HOME_APPEARANCE_WIDTH)
    .padding(6)
    .backgroundColor($r('app.color.home_surface'))
    .border({ width: 1, color: $r('app.color.home_border') })
    .borderRadius(12)
    .shadow({ radius: 12, color: '#1A000000', offsetX: 0, offsetY: 3 })
  }

  build() {
    Row({ space: HOME_MENU_GAP }) {
      if (!this.appearanceOpen || this.availableWidth >= HOME_MENU_WIDTH + HOME_APPEARANCE_WIDTH + HOME_MENU_GAP) {
        Column() {
          this.menuItem('关于', $r('app.media.menu_info'), () => this.onAbout())
          this.separator()
          this.menuItem('外观', $r('app.media.menu_appearance'), () => {
            this.appearanceOpen = !this.appearanceOpen;
          }, true)
          this.menuItem('截图', $r('app.media.menu_capture'), () => this.onPlaceholder('截图'))
          this.menuItem('文件管理', $r('app.media.menu_folder'), () => this.onPlaceholder('文件管理'))
          this.separator()
          this.menuItem('设置', $r('app.media.menu_settings'), () => this.onPlaceholder('设置'))
        }
        .width(HOME_MENU_WIDTH)
        .padding(6)
        .backgroundColor($r('app.color.home_surface'))
        .border({ width: 1, color: $r('app.color.home_border') })
        .borderRadius(12)
        .shadow({ radius: 12, color: '#1A000000', offsetX: 0, offsetY: 3 })
      }
      if (this.appearanceOpen) {
        this.appearancePanel()
      }
    }
    .alignItems(VerticalAlign.Bottom)
    .id('home-more-menu')
    .focusable(true)
    .defaultFocus(true)
    // Default 保留子菜单项的点击响应；Block 会连同子节点一起拦截。
    .hitTestBehavior(HitTestMode.Default)
    .onAppear(() => this.focusMenu())
    .onKeyEvent((event: KeyEvent) => {
      if (event.type === KeyType.Down && event.keyCode === KeyCode.KEYCODE_ESCAPE) {
        if (this.appearanceOpen) {
          this.appearanceOpen = false;
        } else {
          this.onClose();
        }
        event.stopPropagation();
      }
    })
  }
}
