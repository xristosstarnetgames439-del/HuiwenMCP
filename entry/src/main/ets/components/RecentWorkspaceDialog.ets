// 全部工作区：复用首页行和弹窗壳，每批读取十条，List 只创建可见行及少量缓存。
import { common } from '@kit.AbilityKit';
import { listRecentWorkspaces, RecentWorkspace, WORKSPACE_PAGE_SIZE } from '../services/RecentWorkspaceService';
import { HomeDialog } from './HomeDialog';
import { RecentWorkspaceRow } from './RecentWorkspaceRow';

class WorkspaceDataSource implements IDataSource {
  private items: RecentWorkspace[] = [];
  private listeners: DataChangeListener[] = [];

  totalCount(): number {
    return this.items.length;
  }

  getData(index: number): RecentWorkspace {
    return this.items[index];
  }

  registerDataChangeListener(listener: DataChangeListener): void {
    if (this.listeners.indexOf(listener) < 0) {
      this.listeners.push(listener);
    }
  }

  unregisterDataChangeListener(listener: DataChangeListener): void {
    const index = this.listeners.indexOf(listener);
    if (index >= 0) {
      this.listeners.splice(index, 1);
    }
  }

  append(items: RecentWorkspace[]): void {
    for (const item of items) {
      const index = this.items.length;
      this.items.push(item);
      this.listeners.forEach((listener: DataChangeListener) => listener.onDataAdd(index));
    }
  }
}

@Component
export struct RecentWorkspaceDialog {
  @Prop dialogWidth: number = 720;
  // 48vp 行高、8vp 间距：默认最多可见十条，较小窗口由首页传入更小高度。
  @Prop listHeight: number = WORKSPACE_PAGE_SIZE * 48 + (WORKSPACE_PAGE_SIZE - 1) * 8;
  @Prop available: boolean = true;
  @State loading: boolean = false;
  @State error: string = '';
  @State hasMore: boolean = true;
  @State loadedCount: number = 0;
  private dataSource: WorkspaceDataSource = new WorkspaceDataSource();
  private active: boolean = false;
  onCloseRequested: () => void = () => {};
  onOpen: (taskPath: string) => void = () => {};

  aboutToAppear(): void {
    this.active = true;
    this.loadNextPage();
  }

  aboutToDisappear(): void {
    this.active = false;
  }

  private async loadNextPage(): Promise<void> {
    if (!this.active || this.loading || !this.hasMore) {
      return;
    }
    this.loading = true;
    this.error = '';
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const items = await listRecentWorkspaces(context, WORKSPACE_PAGE_SIZE, this.loadedCount);
      if (!this.active) {
        return;
      }
      this.dataSource.append(items);
      this.loadedCount = this.dataSource.totalCount();
      this.hasMore = items.length === WORKSPACE_PAGE_SIZE;
    } catch (error) {
      if (this.active) {
        this.error = `读取工作区失败：${(error as Error).message}`;
      }
    } finally {
      if (this.active) {
        this.loading = false;
      }
    }
  }

  build() {
    HomeDialog({
      title: '全部工作区',
      dialogWidth: this.dialogWidth,
      contentRightPadding: 0,
      onCloseRequested: this.onCloseRequested
    }) {
      Column({ space: 8 }) {
        Stack() {
          List({ space: 8 }) {
            LazyForEach(this.dataSource, (workspace: RecentWorkspace) => {
              ListItem() {
                RecentWorkspaceRow({ workspace, available: this.available, onOpen: this.onOpen })
              }
              .height(48)
              // 数据保持原位置，最右侧空出滚动条通道。
              .padding({ right: 24 })
            }, (workspace: RecentWorkspace) => workspace.taskPath)
          }
          .width('100%')
          .height(this.listHeight)
          .cachedCount(2)
          .scrollBar(BarState.Auto)
          .onReachEnd(() => {
            if (this.error === '') {
              this.loadNextPage();
            }
          })
          if (this.loadedCount === 0) {
            Text(this.loading ? '正在读取工作区…' : this.error !== '' ? '读取失败' : '暂无工作区')
              .fontSize(12)
              .fontColor($r('app.color.home_muted'))
          }
        }
        .width('100%')
        .height(this.listHeight)
        if (this.error !== '' || this.hasMore) {
          // 精确满十条时也能手动续取；错误不会在触底回调中无限重试。
          Button(this.error !== '' ? `${this.error}，点击重试` : this.loading ? '正在加载…' : '加载更多',
            { type: ButtonType.Normal })
            .fontSize(12)
            .fontColor($r('app.color.home_secondary'))
            .height(28)
            .padding({ left: 8, right: 8 })
            .backgroundColor(Color.Transparent)
            .enabled(!this.loading)
            .onClick(() => this.loadNextPage())
        } else {
          Text(`已显示全部 ${this.loadedCount} 条工作区`)
            .fontSize(12)
            .fontColor($r('app.color.home_muted'))
            .height(28)
        }
      }
      .width('100%')
    }
  }
}
