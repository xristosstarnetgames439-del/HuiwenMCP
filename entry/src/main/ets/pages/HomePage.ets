// ArkTS 首页独立于 QPA 固定加载的 Index 承载页，负责功能入口和窗口交接。
import { bundleManager, common } from '@kit.AbilityKit';
import { FunctionSidebar } from '../components/FunctionSidebar';
import { HomeTitleBar } from '../components/HomeTitleBar';
import { HomeFeatureCard } from '../components/HomeFeatureCard';
import { RecentWorkspaceList } from '../components/RecentWorkspaceList';
import { RecentWorkspaceDialog } from '../components/RecentWorkspaceDialog';
import { HOME_FORMAT_DIALOG_WIDTH, HomeFeature, HomeFormatDialog } from '../components/HomeFormatDialog';
import { getNativeStatus } from '../services/ConversionService';
import {
  listRecentWorkspaces, RecentWorkspace, WORKSPACE_PAGE_SIZE
} from '../services/RecentWorkspaceService';
import { HomeMoreMenu, HOME_MENU_WIDTH, HOME_MENU_GAP } from '../components/HomeMoreMenu';
import { HomeAboutDialog } from '../components/HomeAboutDialog';
import { FileManagerDialog } from '../components/fileManager/FileManagerDialog';
import { FileManagerFile } from '../models/FileManagerFile';
import { openFileDirectory } from '../services/FileManagerService';
import { applyHomeAppearance, HomeAppearance } from '../utils/HomeAppearance';
import { DocumentKind, DocumentMode } from '../utils/DocumentStore';
import { hilog } from '@kit.PerformanceAnalysisKit';

@Entry
@Component
struct HomePage {
  @StorageLink('homeCustomTitleBar') customTitleBar: boolean = false;
  @StorageLink('homeAppearance') appearance: HomeAppearance = HomeAppearance.System;
  @State menuOpen: boolean = false;
  @State availableWidth: number = 640;
  @State availableHeight: number = 480;
  @State rootLeft: number = 0;
  @State rootTop: number = 0;
  @State menuAnchorRight: number = 0;
  @State menuAnchorBottom: number = 0;
  @State appVersion: string = '';
  private aboutDialogId: number = -1;
  private aboutDialogOpening: boolean = false;
  @State pingResult: string = '待检测';
  @State activeFeature: HomeFeature = HomeFeature.None;
  @State workspaceLaunching: boolean = false;
  @State recentWorkspaces: RecentWorkspace[] = [];
  @State recentLoading: boolean = false;
  @State recentError: string = '';
  private homeDialogWidth: number = HOME_FORMAT_DIALOG_WIDTH;
  private workspaceListHeight: number = 552;
  @State homeDialogId: number = -1;
  @State homeDialogOpening: boolean = false;
  private homeVisible: boolean = false;
  private recentRefreshPending: boolean = false;
  // 只等待窗口显示，不等待任务恢复，也不接收工作区业务错误。
  private workspaceReady = (): void => {
    if (!this.workspaceLaunching) {
      return;
    }
    this.workspaceLaunching = false;
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.terminateSelf().catch((error: Error) => {
      hilog.warn(0xFF00, 'HuiwenMCP', '关闭首页失败：%{public}s', error.message);
    });
  };
  private workspaceClosed = (): void => {
    this.workspaceLaunching = false;
    this.refreshRecentWorkspaces(true);
  };

  aboutToAppear(): void {
    this.homeVisible = true;
    // 首页打开时检查 ArkTS 是否能调用 C++ NAPI 模块。
    this.pingResult = getNativeStatus();
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.getApplicationContext().eventHub.on('huiwen.workspace.ready', this.workspaceReady);
    context.getApplicationContext().eventHub.on('huiwen.workspace.closed', this.workspaceClosed);
    this.refreshRecentWorkspaces();
  }

  aboutToDisappear(): void {
    this.homeVisible = false;
    this.menuOpen = false;
    this.closeAboutDialog();
    this.closeHomeDialog();
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.getApplicationContext().eventHub.off('huiwen.workspace.ready', this.workspaceReady);
    context.getApplicationContext().eventHub.off('huiwen.workspace.closed', this.workspaceClosed);
    this.recentRefreshPending = false;
  }

  onPageShow(): void {
    this.refreshRecentWorkspaces();
  }

  private async refreshRecentWorkspaces(queueIfLoading: boolean = false): Promise<void> {
    if (!this.homeVisible) {
      return;
    }
    if (this.recentLoading) {
      // 返回首页时的刷新不能被首次读取吞掉，只合并补刷一次。
      if (queueIfLoading) {
        this.recentRefreshPending = true;
      }
      return;
    }
    this.recentRefreshPending = false;
    this.recentLoading = true;
    this.recentError = '';
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const workspaces = await listRecentWorkspaces(context);
      if (this.homeVisible) {
        this.recentWorkspaces = workspaces;
      }
    } catch (error) {
      if (this.homeVisible) {
        this.recentError = `读取最近工作区失败：${(error as Error).message}`;
      }
    } finally {
      this.recentLoading = false;
      if (this.recentRefreshPending && this.homeVisible) {
        this.refreshRecentWorkspaces();
      }
    }
  }

  private async openWorkspace(taskPath: string = '', mode: string = DocumentMode.Convert,
    kind: string = DocumentKind.PDF): Promise<void> {
    if (this.workspaceLaunching) {
      return;
    }
    // 先收起首页子窗口，避免它残留在即将显示的 Qt 工作区上方。
    if (!this.closeHomeDialog()) {
      return;
    }
    this.workspaceLaunching = true;
    this.menuOpen = false;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await context.startAbility({
        bundleName: 'cn.com.HuiWenMCP', abilityName: 'QtWorkspaceAbility',
        parameters: { huiwenTaskPath: taskPath, huiwenMode: mode, huiwenKind: kind }
      });
    } catch (error) {
      if (this.homeVisible) {
        this.workspaceLaunching = false;
        hilog.warn(0xFF00, 'HuiwenMCP', '打开工作区失败：%{public}s', (error as Error).message);
      }
    }
  }

  private async openFileLocation(file: FileManagerFile): Promise<void> {
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await openFileDirectory(context, file);
    } catch (error) {
      hilog.warn(0xFF00, 'HuiwenMCP', '打开文件目录失败：%{public}s', (error as Error).message);
      // 只在文件管理仍打开时提示，避免关闭弹窗后把目录错误带回首页。
      if (this.homeVisible && this.homeDialogId >= 0 && this.activeFeature === HomeFeature.FileManager) {
        this.getUIContext().getPromptAction().showToast({ message: `打开文件目录失败：${(error as Error).message}` });
      }
    }
  }

  private closeHomeDialog(): boolean {
    if (this.homeDialogId < 0) {
      return true;
    }
    try {
      this.getUIContext().getPromptAction().closeCustomDialog(this.homeDialogId);
      this.homeDialogId = -1;
      return true;
    } catch (error) {
      hilog.warn(0xFF00, 'HuiwenMCP', '关闭首页弹窗失败：%{public}s', (error as Error).message);
      return false;
    }
  }

  @Builder
  homeDialogContent() {
    if (this.activeFeature === HomeFeature.FileManager) {
      FileManagerDialog({
        dialogWidth: this.homeDialogWidth,
        onCloseRequested: () => { this.closeHomeDialog(); },
        onOpenFile: (file: FileManagerFile) => this.openFileLocation(file),
        onOpenWorkspace: (file: FileManagerFile) =>
          this.openWorkspace(file.taskPath, file.mode, file.sourceKind || file.kind)
      })
    } else if (this.activeFeature === HomeFeature.Workspaces) {
      RecentWorkspaceDialog({
        dialogWidth: this.homeDialogWidth,
        listHeight: this.workspaceListHeight,
        available: !this.workspaceLaunching,
        onCloseRequested: () => {
          this.closeHomeDialog();
        },
        onOpen: (workspace: RecentWorkspace) =>
          this.openWorkspace(workspace.taskPath, workspace.mode, workspace.kind)
      })
    } else {
      HomeFormatDialog({
        feature: this.activeFeature,
        title: this.activeFeature === HomeFeature.Preview ? '请选择要预览的格式' :
          this.activeFeature === HomeFeature.Edit ? '请选择要编辑的格式' : '请选择要转化的格式',
        onCloseRequested: () => {
          this.closeHomeDialog();
        },
        onConvertRequested: () => {
          this.openWorkspace();
        },
        onPreviewRequested: (kind: DocumentKind) => {
          this.openWorkspace('', DocumentMode.Preview, kind);
        }
      })
    }
  }

  private async selectFeature(feature: HomeFeature): Promise<void> {
    if (this.workspaceLaunching || this.homeDialogOpening || this.homeDialogId >= 0) {
      return;
    }
    this.activeFeature = feature;
    // 文件管理与全部工作区复用首页弹窗的打开、关闭流程，背板宽度与内容一致。
    this.homeDialogWidth = feature === HomeFeature.FileManager ? Math.min(760, this.availableWidth - 32) :
      feature === HomeFeature.Workspaces ? Math.min(720, this.availableWidth - 32) : HOME_FORMAT_DIALOG_WIDTH;
    this.workspaceListHeight = Math.min(WORKSPACE_PAGE_SIZE * 48 +
      (WORKSPACE_PAGE_SIZE - 1) * 8, Math.max(48, this.availableHeight - 160));
    this.menuOpen = false;
    this.homeDialogOpening = true;
    try {
      const promptAction = this.getUIContext().getPromptAction();
      // 按自定义弹窗模板保存系统返回的 ID，由首页统一管理打开和关闭。
      const dialogId = await promptAction.openCustomDialog({
        builder: () => {
          this.homeDialogContent();
        },
        alignment: DialogAlignment.Center,
        // 文件管理在主窗口内承载，避免子窗口内的系统右键菜单被拒绝显示。
        showInSubWindow: feature !== HomeFeature.FileManager,
        // PromptAction 的独立子窗口不能同时启用模态。
        isModal: false,
        autoCancel: true,
        // 文件管理使用透明承载层，内部面板自行缩放并绘制背板；其他弹窗保持原尺寸。
        width: feature === HomeFeature.FileManager ? '100%' : this.homeDialogWidth,
        height: feature === HomeFeature.FileManager ? '100%' : undefined,
        backgroundColor: feature === HomeFeature.FileManager ? Color.Transparent : $r('app.color.home_surface'),
        backgroundBlurStyle: BlurStyle.NONE,
        cornerRadius: feature === HomeFeature.FileManager ? 0 : 16,
        shadow: {
          radius: feature === HomeFeature.FileManager ? 0 : 16,
          color: '#22000000', offsetX: 0, offsetY: feature === HomeFeature.FileManager ? 0 : 6
        },
        onWillDismiss: (action: DismissDialogAction) => {
          this.homeDialogId = -1;
          action.dismiss();
        }
      });
      // 打开是异步的；首页若已退出，及时关闭刚创建的子窗口。
      if (!this.homeVisible) {
        promptAction.closeCustomDialog(dialogId);
        return;
      }
      this.homeDialogId = dialogId;
    } catch (error) {
      hilog.warn(0xFF00, 'HuiwenMCP', '打开首页弹窗失败：%{public}s', (error as Error).message);
    } finally {
      this.homeDialogOpening = false;
    }
  }

  build() {
    // 根布局透明，只有左侧窄栏透出窗口后方；主内容自己绘制不透明背景。
    Stack({ alignContent: Alignment.BottomStart }) {
      Column() {
        if (this.customTitleBar) {
          HomeTitleBar()
        }
        Row() {
          FunctionSidebar({
            menuOpen: this.menuOpen,
            onMenuAnchorChanged: (right: number, bottom: number) => {
              this.menuAnchorRight = right;
              this.menuAnchorBottom = bottom;
            },
            onMenuRequested: () => {
              if (!this.workspaceLaunching && this.homeDialogId < 0 && !this.homeDialogOpening &&
                this.aboutDialogId < 0 && !this.aboutDialogOpening) {
                this.menuOpen = !this.menuOpen;
              }
            }
          })
          this.homeContent()
        }
        .width('100%')
        .layoutWeight(1)
        // 标题栏下方的内容按顶部圆角裁剪，缺口沿用窗口透明背景。
        .borderRadius({ topLeft: 24, topRight: 24, bottomLeft: 0, bottomRight: 0 })
        .clip(true)
      }
      .width('100%')
      .height('100%')
      if (this.menuOpen) {
        // 背景点击层关闭两级菜单，面板阻止命中其下方的背景层。
        Stack({ alignContent: Alignment.BottomStart }) {
          Column()
            .width('100%')
            .height('100%')
            .backgroundColor(Color.Transparent)
            .onClick(() => this.closeMenu())
          HomeMoreMenu({
            appearance: this.appearance,
            availableWidth: this.availableWidth - this.menuLeft() - 8,
            onClose: () => this.closeMenu(),
            onAbout: () => this.openAboutDialog(),
            onFileManager: () => {
              this.closeMenu();
              this.selectFeature(HomeFeature.FileManager);
            },
            onAppearance: (mode: HomeAppearance) => this.changeAppearance(mode),
            onPlaceholder: (name: string) => {
              this.closeMenu();
              this.getUIContext().getPromptAction().showToast({ message: `${name}功能暂未开放` });
            }
          })
          .margin({ left: this.menuLeft(), bottom: this.menuBottom() })
        }
        .width('100%')
        .height('100%')
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Transparent)
    .onAreaChange((_oldArea: Area, area: Area) => {
      this.availableWidth = Number(area.width);
      this.availableHeight = Number(area.height);
      this.rootLeft = Number(area.globalPosition.x);
      this.rootTop = Number(area.globalPosition.y);
    })
  }

  private menuLeft(): number {
    return Math.max(8, Math.min(this.menuAnchorRight - this.rootLeft + HOME_MENU_GAP, this.availableWidth - HOME_MENU_WIDTH - 8));
  }

  private menuBottom(): number {
    return Math.max(8, this.availableHeight - (this.menuAnchorBottom - this.rootTop));
  }

  private closeMenu(): void {
    this.menuOpen = false;
    try {
      this.getUIContext().getFocusController().requestFocus('home-menu-button');
    } catch (error) {
      hilog.warn(0xFF00, 'HuiwenMCP', 'Restore menu button focus failed: %{public}s', (error as Error).message);
    }
  }

  private async changeAppearance(mode: HomeAppearance): Promise<void> {
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await applyHomeAppearance(context, mode);
      this.appearance = mode;
    } catch (error) {
      this.getUIContext().getPromptAction().showToast({ message: `切换外观失败：${(error as Error).message}` });
    }
  }

  private closeAboutDialog(): void {
    if (this.aboutDialogId >= 0) {
      try {
        this.getUIContext().getPromptAction().closeCustomDialog(this.aboutDialogId);
        this.aboutDialogId = -1;
      } catch (error) {
        hilog.warn(0xFF00, 'HuiwenMCP', 'Close about dialog failed: %{public}s', (error as Error).message);
      }
    }
  }

  @Builder
  aboutDialogContent() {
    HomeAboutDialog({ version: this.appVersion, onClose: () => this.closeAboutDialog() })
  }

  private async openAboutDialog(): Promise<void> {
    this.closeMenu();
    if (this.aboutDialogOpening || this.aboutDialogId >= 0) {
      return;
    }
    this.aboutDialogOpening = true;
    try {
      this.appVersion = bundleManager.getBundleInfoForSelfSync(
        bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT).versionName;
    } catch (_) {
      this.appVersion = '';
    }
    try {
      const promptAction = this.getUIContext().getPromptAction();
      const id = await promptAction.openCustomDialog({
        builder: () => this.aboutDialogContent(),
        alignment: DialogAlignment.Center,
        width: Math.min(360, this.availableWidth - 32),
        autoCancel: true,
        isModal: true,
        backgroundColor: $r('app.color.home_surface'),
        backgroundBlurStyle: BlurStyle.NONE,
        cornerRadius: 20,
        onWillDismiss: (action: DismissDialogAction) => {
          this.aboutDialogId = -1;
          action.dismiss();
        }
      });
      if (!this.homeVisible) {
        promptAction.closeCustomDialog(id);
        return;
      }
      this.aboutDialogId = id;
    } catch (error) {
      hilog.warn(0xFF00, 'HuiwenMCP', '打开关于窗口失败：%{public}s', (error as Error).message);
    } finally {
      this.aboutDialogOpening = false;
    }
  }

  @Builder
  homeContent() {
    Row({ space: 24 }) {
      // 除窄侧栏及间距外，介绍区与主功能区按 3:7 分配剩余宽度。
      Stack({ alignContent: Alignment.Center }) {
        // 同名 base/dark 资源随外观切换，底图裁切留白，符号始终等比例居中。
        Image($r('app.media.home_brand_background'))
          .width('100%')
          .height('100%')
          .objectFit(ImageFit.Cover)
          .draggable(false)
          .hitTestBehavior(HitTestMode.None)
        Image($r('app.media.home_brand_infinity'))
          .width('76%')
          .aspectRatio(2)
          .objectFit(ImageFit.Contain)
          .draggable(false)
          .hitTestBehavior(HitTestMode.None)
        Column({ space: 20 }) {
          Text('汇文MCP')
            .fontSize(48)
            .minFontSize(20)
            .maxFontSize(48)
            .maxLines(1)
            .fontWeight(FontWeight.Bold)
            .fontColor($r('app.color.home_intro_title'))
          Column({ space: 20 }) {
            Text('PDF 与 Markdown 的阅读和转换工具')
              .fontSize(20)
              .fontColor($r('app.color.home_secondary'))
            Text('汇集千文 · 连通万物')
              .fontSize(16)
              .fontColor($r('app.color.home_intro_muted'))
          }
          .width('100%')
          .alignItems(HorizontalAlign.Start)
        }
        .width('100%')
        .height('100%')
        .padding({ top: 50, right: 32, bottom: 50, left: 32 })
        .justifyContent(FlexAlign.SpaceBetween)
        .alignItems(HorizontalAlign.Start)
      }
      .layoutWeight(3)
      .height('100%')
      .backgroundColor($r('app.color.home_intro'))
      .borderRadius(24)
      .clip(true)

      Column({ space: 0 }) {
        // 两区顶部顺序排列，最近工作区的间隔由自身外边距单独控制。
        Column({ space: 24 }) {
          Text('开始使用')
            .fontSize(28)
            .fontWeight(FontWeight.Bold)
            .fontColor($r('app.color.home_text'))
          Text('请选择转化模式')
            .fontSize(16)
            .fontColor($r('app.color.home_muted'))

          Row({ space: 12 }) {
            HomeFeatureCard({
              title: '文档预览',
              icon: $r('app.media.home_preview'),
              selected: this.activeFeature === HomeFeature.Preview,
              available: !this.workspaceLaunching,
              onSelect: () => this.selectFeature(HomeFeature.Preview)
            })
            .layoutWeight(1)
            HomeFeatureCard({
              title: '文档编辑',
              icon: $r('app.media.home_edit'),
              selected: this.activeFeature === HomeFeature.Edit,
              available: !this.workspaceLaunching,
              onSelect: () => this.selectFeature(HomeFeature.Edit)
            })
            .layoutWeight(1)
            HomeFeatureCard({
              title: '文档转化',
              icon: $r('app.media.home_convert'),
              selected: this.activeFeature === HomeFeature.Convert,
              available: !this.workspaceLaunching,
              onSelect: () => this.selectFeature(HomeFeature.Convert)
            })
            .layoutWeight(1)
          }
          .width('100%')
          .margin({ top: 60 })

          if (this.workspaceLaunching) {
            Text('正在打开转换工作区…')
              .fontSize(14)
              .fontColor($r('app.color.home_muted'))
          }
          if (this.pingResult !== 'NAPI OK') {
            Text('原生模块尚未就绪')
              .fontSize(13)
              .fontColor($r('app.color.home_warning'))
          }
        }
        .width('100%')
        .justifyContent(FlexAlign.Start)
        .alignItems(HorizontalAlign.Start)

        RecentWorkspaceList({
          workspaces: this.recentWorkspaces,
          loading: this.recentLoading,
          error: this.recentError,
          available: !this.workspaceLaunching && !this.homeDialogOpening && this.homeDialogId < 0,
          onOpen: (workspace: RecentWorkspace) =>
            this.openWorkspace(workspace.taskPath, workspace.mode, workspace.kind),
          onViewAll: () => this.selectFeature(HomeFeature.Workspaces)
        })
        .margin({ top: 70 })
        .flexShrink(0)
      }
      .layoutWeight(7)
      .height('100%')
      .padding({ top: 44, right: 24, bottom: 24, left: 24 })
      .justifyContent(FlexAlign.Start)
      .alignItems(HorizontalAlign.Start)
      .backgroundColor($r('app.color.home_surface'))
      .borderRadius(24)
    }
    .layoutWeight(1)
    .height('100%')
    .padding(24)
    .backgroundColor($r('app.color.home_background'))
  }
}
