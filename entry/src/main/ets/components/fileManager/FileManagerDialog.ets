// 主窗口内的可缩放文件管理弹窗；透明承载层内统一绘制面板，不创建独立系统窗口。
import pointer from '@ohos.multimodalInput.pointer';
import { FileManagerSidebar } from './FileManagerSidebar';
import { FileManagerContent } from './FileManagerContent';
import { FileManagerFile } from '../../models/FileManagerFile';

@Component
export struct FileManagerDialog {
  @Prop dialogWidth: number = 760;
  @State panelWidth: number = 760;
  @State panelHeight: number = 480;
  @State selectedCategory: string = '全部';
  private maxWidth: number = 760;
  private maxHeight: number = 480;
  private resizeStartWidth: number = 760;
  private resizeStartHeight: number = 480;
  private resizeHovered: boolean = false;
  private resizing: boolean = false;
  onCloseRequested: () => void = () => {};
  // 目录定位与工作区恢复分开传递，组件不直接操作系统窗口或任务时间。
  onOpenFile: (file: FileManagerFile) => void = () => {};
  onOpenWorkspace: (file: FileManagerFile) => void = () => {};

  aboutToAppear(): void { this.panelWidth = this.dialogWidth; }

  aboutToDisappear(): void { this.getUIContext().getCursorController().restoreDefault(); }

  private updateResizeCursor(): void {
    const cursor = this.getUIContext().getCursorController();
    if (this.resizeHovered || this.resizing) {
      cursor.setCursor(pointer.PointerStyle.NORTH_WEST_SOUTH_EAST);
    } else {
      cursor.restoreDefault();
    }
  }

  private resizePanel(width: number, height: number): void {
    // 小于初始尺寸的首页可用区优先保证面板不越界；正常最小尺寸为 760 × 480vp。
    this.panelWidth = Math.min(this.maxWidth, Math.max(Math.min(760, this.maxWidth), width));
    this.panelHeight = Math.min(this.maxHeight, Math.max(Math.min(480, this.maxHeight), height));
  }

  @Builder
  resizeHandle() {
    Stack() {
      Line().width(28).height(28).startPoint([10, 22]).endPoint([22, 10])
        .stroke($r('app.color.home_muted')).strokeWidth(1.5).hitTestBehavior(HitTestMode.None)
      Line().width(28).height(28).startPoint([17, 22]).endPoint([22, 17])
        .stroke($r('app.color.home_muted')).strokeWidth(1.5).hitTestBehavior(HitTestMode.None)
    }
    .width(28)
    .height(28)
    .position({ right: 0, bottom: 0 })
    .accessibilityText('拖动调整文件管理大小')
    .onHover((hovered: boolean) => {
      this.resizeHovered = hovered;
      this.updateResizeCursor();
    })
    .gesture(PanGesture({ direction: PanDirection.All, distance: 0 })
      .onActionStart(() => {
        this.resizeStartWidth = this.panelWidth;
        this.resizeStartHeight = this.panelHeight;
        this.resizing = true;
        this.updateResizeCursor();
      })
      .onActionUpdate((event: GestureEvent) => {
        // 面板保持居中，尺寸按双倍位移变化，使右下角跟随鼠标。
        this.resizePanel(this.resizeStartWidth + event.offsetX * 2,
          this.resizeStartHeight + event.offsetY * 2);
      })
      .onActionEnd(() => { this.resizing = false; this.updateResizeCursor(); })
      .onActionCancel(() => { this.resizing = false; this.updateResizeCursor(); }))
  }

  @Builder
  panel() {
    Stack({ alignContent: Alignment.TopEnd }) {
      Row() {
        FileManagerSidebar({
          selectedCategory: this.selectedCategory,
          onSelect: (category: string) => { this.selectedCategory = category; }
        })
          .flexShrink(0)
        FileManagerContent({
          title: this.selectedCategory, selectedCategory: this.selectedCategory,
          onOpen: this.onOpenFile, onOpenWorkspace: this.onOpenWorkspace
        })
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
      this.resizeHandle()
    }
    .width(this.panelWidth)
    .height(this.panelHeight)
    .backgroundColor($r('app.color.home_surface'))
    .borderRadius(16)
    .shadow({ radius: 16, color: '#22000000', offsetX: 0, offsetY: 6 })
    .clip(true)
    .hitTestBehavior(HitTestMode.Default)
  }

  build() {
    Stack({ alignContent: Alignment.Center }) {
      // 外部点击关闭；面板在同一主窗口内缩放，系统右键菜单仍可使用。
      Column()
        .width('100%')
        .height('100%')
        .backgroundColor(Color.Transparent)
        .onClick(() => this.onCloseRequested())
      this.panel()
    }
    .width('100%')
    .height('100%')
    .onAreaChange((_oldArea: Area, area: Area) => {
      this.maxWidth = Math.max(0, Number(area.width) - 32);
      this.maxHeight = Math.max(0, Number(area.height) - 32);
      this.resizePanel(this.panelWidth, this.panelHeight);
    })
  }
}
