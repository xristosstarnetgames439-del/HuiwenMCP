// 格式弹窗内容：与系统背板共用固定宽度，背景、圆角和阴影由外层统一绘制。
import { DocumentKind } from '../utils/DocumentStore';

export const HOME_FORMAT_DIALOG_WIDTH: number = 360;

export enum HomeFeature {
  None,
  Preview,
  Edit,
  Convert
}

@Component
export struct HomeFormatDialog {
  @Prop feature: HomeFeature = HomeFeature.Preview;
  @Prop title: string = '';
  onCloseRequested: () => void = () => {};
  onConvertRequested: () => void = () => {};
  onPreviewRequested: (kind: DocumentKind) => void = () => {};

  @Builder
  formatItem(name: string, icon: Resource, kind: DocumentKind) {
    Column({ space: 10 }) {
      Image(icon)
        .width(48)
        .height(48)
        .objectFit(ImageFit.Contain)
        .draggable(false)
      Text(name)
        .fontSize(14)
        .fontColor($r('app.color.home_secondary'))
    }
    .padding(12)
    .justifyContent(FlexAlign.Center)
    // 预览复用同一个工作区；Word 和编辑仍只保留格式展示。
    .onClick(() => {
      if (this.feature === HomeFeature.Preview && kind !== DocumentKind.Word) {
        this.onPreviewRequested(kind);
      }
    })
  }

  build() {
    Stack({ alignContent: Alignment.TopEnd }) {
      Column({ space: 16 }) {
        Text(this.title)
          .fontSize(18)
          .fontWeight(FontWeight.Normal)
          .fontColor($r('app.color.home_text'))
          .width('100%')
          .textAlign(TextAlign.Start)
        this.formatOptions()
      }
      .width('100%')
      .padding({ top: 40, right: 24, bottom: 24, left: 24 })
      .alignItems(HorizontalAlign.Center)
      Button({ type: ButtonType.Normal }) {
        Image($r('app.media.home_close'))
          .width(18)
          .height(18)
          .objectFit(ImageFit.Contain)
          .draggable(false)
      }
      .width(28)
      .height(28)
      .padding(0)
      .margin({ top: 8, right: 8 })
      .backgroundColor(Color.Transparent)
      .borderRadius(8)
      .hoverEffect(HoverEffect.Auto)
      .accessibilityText('关闭格式选择窗口')
      .onClick(() => this.onCloseRequested())
    }
    // 内外使用同一宽度，避免默认背板在内容四周留下透明空白。
    .width(HOME_FORMAT_DIALOG_WIDTH)
    .backgroundColor(Color.Transparent)
  }

  @Builder
  formatOptions() {
    if (this.feature === HomeFeature.Preview || this.feature === HomeFeature.Edit) {
      // 只有 PDF/MD 预览接入跳转；文件在 Qt 工作区内选择。
      Row({ space: 12 }) {
        this.formatItem('Word', $r('app.media.home_format_word'), DocumentKind.Word)
        this.formatItem('PDF', $r('app.media.home_format_pdf'), DocumentKind.PDF)
        this.formatItem('MD', $r('app.media.home_format_md'), DocumentKind.Markdown)
      }
    } else if (this.feature === HomeFeature.Convert) {
      Button({ type: ButtonType.Normal }) {
        Column({ space: 10 }) {
          Row({ space: 12 }) {
            Image($r('app.media.home_format_pdf'))
              .width(44)
              .height(44)
              .objectFit(ImageFit.Contain)
              .draggable(false)
            Image($r('app.media.home_arrow_right'))
              .width(24)
              .height(24)
              .objectFit(ImageFit.Contain)
              .draggable(false)
            Image($r('app.media.home_format_md'))
              .width(44)
              .height(44)
              .objectFit(ImageFit.Contain)
              .draggable(false)
          }
          Text('PDF → MD')
            .fontSize(16)
            .fontColor($r('app.color.home_text'))
        }
        .justifyContent(FlexAlign.Center)
      }
      .padding(12)
      .borderRadius(12)
      .backgroundColor(Color.Transparent)
      .hoverEffect(HoverEffect.Auto)
      .accessibilityText('PDF 转 MD，进入转换工作区')
      .onClick(() => this.onConvertRequested())
    }
  }
}
