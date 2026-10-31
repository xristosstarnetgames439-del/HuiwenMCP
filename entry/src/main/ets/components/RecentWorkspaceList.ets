// 最近工作区只负责展示；图标、格式、时间固定宽度，文件名单独占满剩余空间。
import { RecentWorkspace } from '../services/RecentWorkspaceService';
import { RecentWorkspaceRow } from './RecentWorkspaceRow';

@Component
export struct RecentWorkspaceList {
  @Prop workspaces: RecentWorkspace[] = [];
  @Prop loading: boolean = false;
  @Prop available: boolean = true;
  @Prop error: string = '';
  @Prop maxListHeight: number = 240;
  private readonly rowHeight: number = 48;
  private readonly rowSpacing: number = 8;
  onOpen: (taskPath: string) => void = () => {};
  onViewAll: () => void = () => {};

  build() {
    Column({ space: 8 }) {
      Row() {
        Text('最近工作区')
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.home_text'))
        Button('查看全部 >', { type: ButtonType.Normal })
          .height(28)
          .fontSize(14)
          .fontWeight(FontWeight.Normal)
          .fontColor($r('app.color.home_secondary'))
          .padding({ top: 4, right: 0, bottom: 4, left: 8 })
          .backgroundColor(Color.Transparent)
          .enabled(this.available)
          .onClick(() => this.onViewAll())
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceBetween)
      .margin({ bottom: 8 })
      if (this.loading) {
        Text('正在读取最近工作区…')
          .fontSize(12)
          .fontColor($r('app.color.home_muted'))
      } else if (this.error !== '') {
        Text(this.error)
          .fontSize(12)
          .fontColor($r('app.color.home_warning'))
      } else if (this.workspaces.length === 0) {
        Text('暂无最近工作区')
          .fontSize(12)
          .fontColor($r('app.color.home_muted'))
      } else {
        // 标题固定，只让记录滚动；首页少量记录按实际高度收拢。
        Scroll() {
          Column({ space: this.rowSpacing }) {
            ForEach(this.workspaces, (workspace: RecentWorkspace) => {
              RecentWorkspaceRow({ workspace, available: this.available, onOpen: this.onOpen })
            }, (workspace: RecentWorkspace) => workspace.taskPath)
          }
          .width('100%')
        }
        .width('100%')
        .height(Math.min(this.workspaces.length * this.rowHeight +
          (this.workspaces.length - 1) * this.rowSpacing, this.maxListHeight))
        .align(Alignment.TopStart)
        .scrollBar(BarState.Auto)
      }
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
  }
}
