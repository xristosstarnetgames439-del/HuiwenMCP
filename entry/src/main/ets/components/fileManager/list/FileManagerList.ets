// 文件列表：懒创建可见行，标题与搜索留在列表外；数据源只替换内存快照。
import { FileManagerFile } from '../../../models/FileManagerFile';
import { FileManagerItem } from './FileManagerItem';
import { FileManagerListEntry } from './FileTimeGrouping';

class FileListDataSource implements IDataSource {
  private entries: FileManagerListEntry[] = [];
  private listeners: DataChangeListener[] = [];

  totalCount(): number { return this.entries.length; }
  getData(index: number): FileManagerListEntry { return this.entries[index]; }

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

  replace(entries: FileManagerListEntry[]): void {
    this.entries = entries;
    this.listeners.forEach((listener: DataChangeListener) => listener.onDataReloaded());
  }
}

@Component
export struct FileManagerList {
  @Prop @Watch('reload') entries: FileManagerListEntry[] = [];
  @Prop loading: boolean = false;
  @Prop error: string = '';
  onRetry: () => void = () => {};
  onOpen: (file: FileManagerFile) => void = () => {};
  private dataSource: FileListDataSource = new FileListDataSource();
  private scroller: Scroller = new Scroller();

  aboutToAppear(): void { this.dataSource.replace(this.entries); }

  private reload(): void {
    // 分类或搜索结果变化时停止旧滚动并回顶，不沿用上一组文件的位置。
    this.scroller.scrollTo({ xOffset: 0, yOffset: 0, animation: false });
    this.dataSource.replace(this.entries);
  }

  build() {
    Stack() {
      List({ scroller: this.scroller }) {
        LazyForEach(this.dataSource, (entry: FileManagerListEntry) => {
          ListItem() {
            if (entry.file === null) {
              Text(entry.title)
                .fontSize(14)
                .fontColor($r('app.color.home_muted'))
                .width('100%')
                .height(36)
                .padding({ left: 8 })
            } else {
              Column() {
                FileManagerItem({
                  file: entry.file as FileManagerFile, timeLabel: entry.timeLabel, onOpen: this.onOpen
                })
                Divider().color($r('app.color.home_border')).strokeWidth(1)
              }
              .width('100%')
            }
          }
          // 滚动条位于列表最右侧，文件内容单独留出通道。
          .padding({ right: 16 })
        }, (entry: FileManagerListEntry) => entry.key)
      }
      .width('100%')
      .height('100%')
      .cachedCount(2)
      .scrollBar(BarState.Auto)
      if (this.error !== '') {
        Column({ space: 12 }) {
          Text(this.error).fontSize(13).fontColor($r('app.color.home_warning'))
          Button('重试').onClick(() => this.onRetry())
        }
        .padding(24)
      } else if (this.loading || this.entries.length === 0) {
        Text(this.loading ? '正在读取文件…' : '暂无符合条件的文件')
          .fontSize(13)
          .fontColor($r('app.color.home_muted'))
      }
    }
    .width('100%')
    .height('100%')
    .padding({ left: 8, right: 4, bottom: 12 })
  }
}
