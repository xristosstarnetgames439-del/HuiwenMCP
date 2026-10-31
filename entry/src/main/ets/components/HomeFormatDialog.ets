// 格式弹窗内容：与系统背板共用固定宽度，背景、圆角和阴影由外层统一绘制。
import { DocumentKind } from '../utils/DocumentStore';
import { HomeDialog } from './HomeDialog';

export const HOME_FORMAT_DIALOG_WIDTH: number = 360;

export enum HomeFeature {
  None,
  Preview,
  Edit,
  Convert,
  Workspaces
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
    HomeDialog({
      title: this.title,
      dialogWidth: HOME_FORMAT_DIALOG_WIDTH,
      onCloseRequested: this.onCloseRequested
    }) {
      this.formatOptions()
    }
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
