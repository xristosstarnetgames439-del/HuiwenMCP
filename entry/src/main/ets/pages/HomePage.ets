// ArkTS 首页独立于 QPA 固定加载的 Index 承载页，负责功能入口和窗口交接。
import { common } from '@kit.AbilityKit';
import { getNativeStatus, pdfToMd } from '../services/ConversionService';
import { DocumentKind, pickDocumentToSandbox } from '../utils/DocumentStore';

@Entry
@Component
struct HomePage {
  @State pingResult: string = '待检测';
  @State selectedAction: string = '';
  @State selectedName: string = '';
  @State sourceUri: string = '';
  @State inputPath: string = '';
  @State outputPath: string = '';
  @State selectionError: string = '';
  @State conversionStatus: string = '';
  @State pickerBusy: boolean = false;
  @State workspaceLaunching: boolean = false;
  private launchTimeout: number = -1;
  private workspaceReady = (): void => {
    // 只有发起跳转的首页消费一次就绪通知，避免其他页面结束自己的 Ability。
    if (!this.workspaceLaunching) {
      return;
    }
    this.workspaceLaunching = false;
    clearTimeout(this.launchTimeout);
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    // Qt 窗口确认显示后，只结束原首页 Ability，不结束应用进程。
    context.terminateSelf().catch((error: Error) => {
      this.selectionError = `关闭首页失败：${error.message}`;
      this.workspaceLaunching = false;
    });
  };
  private workspaceFailed = (message: string): void => {
    if (!this.workspaceLaunching) {
      return;
    }
    clearTimeout(this.launchTimeout);
    this.workspaceLaunching = false;
    this.selectionError = `Qt 工作区启动失败：${message}`;
  };

  aboutToAppear(): void {
    // 首页打开时检查 ArkTS 是否能调用 C++ NAPI 模块。
    this.pingResult = getNativeStatus();
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.getApplicationContext().eventHub.on('huiwen.workspace.ready', this.workspaceReady);
    context.getApplicationContext().eventHub.on('huiwen.workspace.failed', this.workspaceFailed);
  }

  aboutToDisappear(): void {
    clearTimeout(this.launchTimeout);
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    context.getApplicationContext().eventHub.off('huiwen.workspace.ready', this.workspaceReady);
    context.getApplicationContext().eventHub.off('huiwen.workspace.failed', this.workspaceFailed);
  }

  private async openWorkspace(): Promise<void> {
    this.workspaceLaunching = true;
    this.selectionError = '';
    this.launchTimeout = setTimeout(() => {
      this.workspaceFailed('未收到窗口显示通知，请检查 HUIWEN_QT 日志');
    }, 15000);
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await context.startAbility({ bundleName: 'cn.com.HuiWenMCP', abilityName: 'QtWorkspaceAbility' });
    } catch (error) {
      this.workspaceFailed((error as Error).message);
    }
  }

  // 三个入口共用沙箱导入；仅转换入口生成 MD，完成后仍留在首页。
  private async chooseDocument(action: string, kind: DocumentKind, forConversion: boolean): Promise<void> {
    if (this.pickerBusy) {
      return;
    }
    this.pickerBusy = true;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const document = await pickDocumentToSandbox(context, kind, forConversion);
      if (document === null) {
        return;
      }
      this.selectedAction = action;
      this.selectedName = document.name;
      this.sourceUri = document.uri;
      this.inputPath = document.inputPath;
      this.outputPath = '';
      this.selectionError = '';
      this.conversionStatus = '';
      if (forConversion) {
        this.conversionStatus = '正在转换 PDF…';
        try {
          this.outputPath = await pdfToMd(document.inputPath, document.outputPath);
          this.conversionStatus = 'MD 文件已生成';
        } catch (error) {
          this.conversionStatus = '';
          this.selectionError = `PDF 转换失败：${(error as Error).message}`;
        }
      }
    } catch (error) {
      this.selectionError = `选择或复制文件失败：${(error as Error).message}`;
      this.conversionStatus = '';
    } finally {
      this.pickerBusy = false;
    }
  }

  build() {
    Row({ space: 24 }) {
      // 左侧暂用文字代替应用宣传图。
      Column({ space: 20 }) {
        Text('汇文 MCP')
          .fontSize(48)
          .fontWeight(FontWeight.Bold)
          .fontColor('#183153')
        Text('PDF 与 Markdown 的阅读和转换工具')
          .fontSize(20)
          .fontColor('#4B6080')
        Text('汇集千文 · 连通万物')
          .fontSize(16)
          .fontColor('#667B98')
      }
      .layoutWeight(1)
      .height('100%')
      .padding(48)
      .justifyContent(FlexAlign.Center)
      .alignItems(HorizontalAlign.Start)
      .backgroundColor('#EAF2FF')
      .borderRadius(24)

      Column({ space: 18 }) {
        Text('开始使用')
          .fontSize(28)
          .fontWeight(FontWeight.Bold)
          .fontColor('#183153')
        Text('选择需要的功能')
          .fontSize(16)
          .fontColor('#667B98')

        Button('PDF 预览')
          .width('100%')
          .height(60)
          .enabled(!this.pickerBusy)
          .onClick(() => {
            this.chooseDocument('PDF 预览', DocumentKind.PDF, false);
          })
        Button('MD 预览')
          .width('100%')
          .height(60)
          .enabled(!this.pickerBusy)
          .onClick(() => {
            this.chooseDocument('MD 预览', DocumentKind.Markdown, false);
          })
        Button('PDF 转 MD')
          .width('100%')
          .height(60)
          .enabled(!this.pickerBusy && !this.workspaceLaunching)
          .onClick(() => {
            // 原首页即时转换暂时禁用，改由 Qt 工作区选择文件并调用转换服务。
            // this.chooseDocument('PDF 转 MD', DocumentKind.PDF, true);
            this.openWorkspace();
          })

        if (this.selectedName !== '') {
          Text(`${this.selectedAction}：${this.selectedName}`)
            .fontSize(14)
            .fontColor('#667B98')
          Text(`输入沙箱路径：${this.inputPath}`)
            .fontSize(12)
            .fontColor('#667B98')
          if (this.outputPath !== '') {
            Text(`生成的 MD 路径：${this.outputPath}`)
              .fontSize(12)
              .fontColor('#667B98')
          }
        }
        if (this.pickerBusy) {
          Text(this.conversionStatus || '正在选择或复制文件…')
            .fontSize(13)
            .fontColor('#667B98')
        }
        if (!this.pickerBusy && this.conversionStatus !== '') {
          Text(this.conversionStatus)
            .fontSize(13)
            .fontColor('#26734D')
        }
        if (this.selectionError !== '') {
          Text(this.selectionError)
            .fontSize(13)
            .fontColor('#B45309')
        }
        if (this.pingResult !== 'NAPI OK') {
          Text('原生模块尚未就绪')
            .fontSize(13)
            .fontColor('#B45309')
        }
      }
      .width(360)
      .height('100%')
      .padding(32)
      .justifyContent(FlexAlign.Center)
      .alignItems(HorizontalAlign.Start)
      .backgroundColor('#FFFFFF')
      .borderRadius(24)
    }
    .width('100%')
    .height('100%')
    .padding(24)
    .backgroundColor('#F5F7FB')
  }
}
