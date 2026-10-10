// Qt 独立窗口入口；保留自己的 Context 处理文件选择，首页结束后仍可使用。
import { UIAbility, Want, AbilityConstant } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { fileIo, fileUri, picker } from '@kit.CoreFileKit';
import hilog from '@ohos.hilog';
import qpa from 'libplugins_platforms_qopenharmony.so';
import nativeBridge from 'libhuiwen_native.so';
import { DocumentKind, pickDocumentToSandbox } from '../utils/DocumentStore';

interface WorkspaceEvent {
  action: string;
  path: string;
}

interface WorkspaceReply {
  action: string;
  name: string;
  inputPath: string;
  outputPath: string;
  error: string;
}

export default class QtWorkspaceAbility extends UIAbility {
  // QPA 读取这两个字段，加载应用共享库并调用其中的 main()。
  private launchApplication: string = 'libhuiwen_qt.so';
  private launchParams: string = '';
  private pickerBusy: boolean = false;
  private workspaceReady: boolean = false;

  // QPA 调用本方法后写入 idName，并把存储传给固定的 pages/Index 承载页。
  newLocalStorage(): LocalStorage {
    return new LocalStorage();
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    try {
      nativeBridge.registerWorkspace((json: string) => {
        this.handleWorkspaceEvent(json).catch((error: Error) => {
          hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_EVENT_FAILED: %{public}s', error.message);
        });
      });
      qpa.startQtApplication(this);
      hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_START');
    } catch (error) {
      const message = (error as Error).message;
      this.context.getApplicationContext().eventHub.emit('huiwen.workspace.failed', message);
      this.context.terminateSelf().catch((failure: Error) => {
        hilog.error(0xFF00, 'QtWorkspace', '关闭失败的 Qt 窗口：%{public}s', failure.message);
      });
    }
  }

  private async handleWorkspaceEvent(json: string): Promise<void> {
    const event = JSON.parse(json) as WorkspaceEvent;
    if (event.action === 'ready') {
      this.workspaceReady = true;
      hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_READY');
      this.context.getApplicationContext().eventHub.emit('huiwen.workspace.ready');
      return;
    }
    if (event.action === 'close') {
      await this.context.terminateSelf();
      return;
    }
    const reply: WorkspaceReply = { action: event.action, name: '', inputPath: '', outputPath: '', error: '' };
    if (event.action === 'pick' && this.pickerBusy) {
      return;
    }
    try {
      if (event.action === 'pick') {
        this.pickerBusy = true;
        const document = await pickDocumentToSandbox(this.context, DocumentKind.PDF, true);
        if (document !== null) {
          reply.name = document.name;
          reply.inputPath = document.inputPath;
          reply.outputPath = document.outputPath;
          hilog.info(0xFF00, 'QtWorkspace', 'HUIWEN_QT_PICK_OK');
        }
      } else if (event.action === 'open') {
        await this.openOutputLocation(event.path);
      }
    } catch (error) {
      reply.error = (error as Error).message;
      hilog.error(0xFF00, 'QtWorkspace', 'HUIWEN_QT_%{public}s_FAILED: %{public}s', event.action, reply.error);
    } finally {
      if (event.action === 'pick') {
        this.pickerBusy = false;
      }
      nativeBridge.workspaceReply(JSON.stringify(reply));
    }
  }

  private async openOutputLocation(path: string): Promise<void> {
    const outputDir = `${this.context.getApplicationContext().filesDir}/outputs/md/`;
    if (!path.startsWith(outputDir) || path.includes('/../') || !path.endsWith('.md')) {
      throw new Error('输出路径不属于本应用 Markdown 沙箱');
    }
    // 尝试在系统文件管理器定位；部分系统不允许展示应用私有目录。
    try {
      await this.context.startAbility({
        action: 'ohos.want.action.viewData',
        uri: fileUri.getUriFromPath(outputDir),
        type: 'inode/directory'
      });
      return;
    } catch (_) {
      // 系统拒绝私有目录时，使用保存选择器导出副本供用户查看。
    }
    const options = new picker.DocumentSaveOptions();
    options.newFileNames = [path.slice(path.lastIndexOf('/') + 1)];
    options.fileSuffixChoices = ['Markdown|.md'];
    const targets = await new picker.DocumentViewPicker(this.context).save(options);
    if (targets.length === 0) {
      return;
    }
    const source = await fileIo.open(path, fileIo.OpenMode.READ_ONLY);
    try {
      const target = await fileIo.open(targets[0], fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
      try {
        await fileIo.copyFile(source.fd, target.fd);
      } finally {
        await fileIo.close(target);
      }
    } finally {
      await fileIo.close(source);
    }
  }

  onDestroy(): void {
    nativeBridge.clearWorkspace();
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    // 复用已有工作区时，继续由 QPA 管理窗口，并重发就绪通知结束新首页。
    qpa.handleOnNewWant(want, launchParam);
    if (this.workspaceReady) {
      this.context.getApplicationContext().eventHub.emit('huiwen.workspace.ready');
    }
  }
}
