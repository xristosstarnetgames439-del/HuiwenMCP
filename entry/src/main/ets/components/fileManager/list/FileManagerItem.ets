// 单个文件行：左键定位目录，右键菜单请求打开所属工作区；两种事件均回传完整文件对象。
import { FileManagerFile } from '../../../models/FileManagerFile';
import { canOpenWorkspace } from '../../../services/RecentWorkspaceService';

@Component
export struct FileManagerItem {
  @Prop file: FileManagerFile;
  @Prop timeLabel: string = '';
  onOpen: (file: FileManagerFile) => void = () => {};
  onOpenWorkspace: (file: FileManagerFile) => void = () => {};

  private fileIcon(): Resource {
    if (this.file.kind === 'word') {
      return $r('app.media.home_format_word');
    }
    return this.file.kind === 'pdf' ? $r('app.media.home_format_pdf') : $r('app.media.home_format_md');
  }

  @Builder
  workspaceMenu() {
    Menu() {
      MenuItem({ content: '打开最近工作区' })
        // 输出 MD 的工作区类型取原始输入类型，不按该行的 MD 类型启动 Qt。
        .enabled(canOpenWorkspace(this.file.mode, this.file.sourceKind || this.file.kind, this.file.name))
        .onClick(() => this.onOpenWorkspace(this.file))
    }
  }

  build() {
    Button({ type: ButtonType.Normal }) {
      Row({ space: 12 }) {
        Image(this.fileIcon())
          .width(32)
          .height(40)
          .objectFit(ImageFit.Contain)
          .draggable(false)
          .flexShrink(0)
        Column({ space: 6 }) {
          Text(this.file.name)
            .fontSize(14)
            .fontColor($r('app.color.home_text'))
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
            .ellipsisMode(EllipsisMode.CENTER)
          Text(`${this.file.formatLabel}｜${this.file.workspaceName}`)
            .fontSize(11)
            .fontColor($r('app.color.home_muted'))
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)
        .constraintSize({ minWidth: 0 })
        Column({ space: 6 }) {
          Text(this.timeLabel)
            .fontSize(11)
            .fontColor($r('app.color.home_muted'))
            .maxLines(1)
          Text(this.file.sizeLabel)
            .fontSize(11)
            .fontColor($r('app.color.home_muted'))
            .maxLines(1)
        }
        .width(110)
        .flexShrink(0)
        .alignItems(HorizontalAlign.End)
      }
      .width('100%')
      .height('100%')
      .hitTestBehavior(HitTestMode.None)
    }
    .width('100%')
    .height(76)
    .padding({ left: 8, right: 8 })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .accessibilityText(`${this.file.name}，${this.file.role === 'input' ? '输入文件' : '输出文件'}，${this.timeLabel}，${this.file.sizeLabel}`)
    .onClick(() => this.onOpen(this.file))
    .bindContextMenu(this.workspaceMenu, ResponseType.RightClick)
  }
}
