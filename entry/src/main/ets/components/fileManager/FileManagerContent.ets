// 右侧协调入口：只组合标题栏与列表，一次读取快照，切换分类／搜索不重复扫描沙箱。
import { common } from '@kit.AbilityKit';
import { FileManagerFile } from '../../models/FileManagerFile';
import { filterFileManagerFiles, listFileManagerFiles } from '../../services/FileManagerService';
import { FileManagerHeader } from './header/FileManagerHeader';
import { FileManagerList } from './list/FileManagerList';
import { FileManagerListEntry, groupFileManagerFiles } from './list/FileTimeGrouping';

@Component
export struct FileManagerContent {
  @Prop title: string = '全部';
  @Prop @Watch('updateList') selectedCategory: string = '全部';
  @State keyword: string = '';
  @State entries: FileManagerListEntry[] = [];
  @State loading: boolean = false;
  @State error: string = '';
  onOpen: (file: FileManagerFile) => void = () => {};
  private files: FileManagerFile[] = [];
  private active: boolean = false;
  private snapshotTime: number = Date.now();

  aboutToAppear(): void {
    this.active = true;
    this.loadFiles();
  }

  aboutToDisappear(): void { this.active = false; }

  private updateList(): void {
    this.entries = groupFileManagerFiles(
      filterFileManagerFiles(this.files, this.selectedCategory, this.keyword), this.snapshotTime);
  }

  private async loadFiles(): Promise<void> {
    if (this.loading) {
      return;
    }
    this.loading = true;
    this.error = '';
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const files = await listFileManagerFiles(context);
      if (this.active) {
        this.files = files;
        this.snapshotTime = Date.now();
        this.updateList();
      }
    } catch (error) {
      if (this.active) {
        this.error = `读取文件失败：${(error as Error).message}`;
      }
    } finally {
      if (this.active) {
        this.loading = false;
      }
    }
  }

  build() {
    Column() {
      FileManagerHeader({
        title: this.title, keyword: this.keyword,
        onSearch: (keyword: string) => {
          this.keyword = keyword;
          this.updateList();
        }
      })
      FileManagerList({
        entries: this.entries, loading: this.loading, error: this.error,
        onRetry: () => this.loadFiles(), onOpen: this.onOpen
      })
        .layoutWeight(1)
    }
    .width('100%')
    .height('100%')
    .backgroundColor($r('app.color.home_surface'))
  }
}
