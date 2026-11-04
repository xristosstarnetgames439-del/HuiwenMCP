// 最近截图横向列表：使用原生 Swiper 处理拖动和箭头翻页，不自行维护滚动动画。
import { fileUri } from '@kit.CoreFileKit';
import { ScreenshotImage } from '../../services/screenshot/ScreenshotHistory';

@Component
export struct RecentScreenshotList {
  @Prop images: ScreenshotImage[] = [];
  @Prop root: string = '';
  @State index: number = 0;
  @State visibleCount: number = 3;
  private controller: SwiperController = new SwiperController();
  onOpen: (path: string) => void = () => {};

  private canMove(label: string): boolean {
    const lastIndex = Math.max(0, this.images.length - this.visibleCount);
    return lastIndex > 0 && (label === '‹' ? this.index > 0 : this.index < lastIndex);
  }

  @Builder
  arrow(label: string, action: () => void) {
    Button(label, { type: ButtonType.Normal })
      .width(28)
      .height(100)
      .padding(0)
      .fontSize(28)
      // 直接读取组件状态，避免 @Builder 的布尔值参数停留在首次渲染结果。
      .fontColor(this.canMove(label) ? $r('app.color.home_secondary') : '#BFC4CB')
      .backgroundColor(Color.Transparent)
      .borderRadius(8)
      .enabled(this.canMove(label))
      .accessibilityText(label === '‹' ? '上一张截图' : '下一张截图')
      .onClick(() => action())
  }

  build() {
    if (this.images.length === 0) {
      Text('暂无最近截图')
        .fontSize(14)
        .fontColor($r('app.color.home_muted'))
        .width('100%')
        .height(120)
        .textAlign(TextAlign.Center)
    } else {
      Row({ space: 8 }) {
        this.arrow('‹', (): void => { this.controller.showPrevious(); })
        Swiper(this.controller) {
          ForEach(this.images, (item: ScreenshotImage) => {
            Button({ type: ButtonType.Normal }) {
              Image(fileUri.getUriFromPath(`${this.root}/${item.name}`))
                .width('100%')
                .height('100%')
                .objectFit(ImageFit.Contain)
                .draggable(false)
                .hitTestBehavior(HitTestMode.None)
            }
            .height(120)
            .padding(6)
            .backgroundColor($r('app.color.home_background'))
            .borderRadius(8)
            .accessibilityText(`预览截图 ${new Date(item.createdAt).toLocaleString()}`)
            .onClick(() => this.onOpen(`${this.root}/${item.name}`))
          }, (item: ScreenshotImage) => item.name)
        }
        .layoutWeight(1)
        .height(120)
        .loop(false)
        .indicator(false)
        .displayCount(Math.min(this.visibleCount, this.images.length))
        .itemSpace(12)
        .onAreaChange((_oldArea: Area, area: Area) => {
          this.visibleCount = Math.max(1, Math.floor((Number(area.width) + 12) / 172));
        })
        .onChange((index: number) => { this.index = index; })
        this.arrow('›', (): void => { this.controller.showNext(); })
      }
      .width('100%')
    }
  }
}
