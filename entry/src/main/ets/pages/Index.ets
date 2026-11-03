// QPA 固定加载 pages/Index；这里只承载 Qt 节点，不放首页和窗口关闭监听。
import { common } from '@kit.AbilityKit';
import CustomChildProcess from '../process/CustomChildProcess';

// 保留 CEF 动态启动的子进程模块，与适配层固定路径一致。
CustomChildProcess.keepModule();
const storage = LocalStorage.getShared();

@Entry(storage)
@Component
struct Index {
  // QPA 在 Qt 宿主 newLocalStorage() 返回的存储中写入窗口节点 ID。
  @LocalStorageProp('idName') windowId: string = '';
  @LocalStorageProp('huiwenScreenshot') screenshot: boolean = false;

  private notifyScreenshotReady(): void {
    if (this.screenshot) {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      // 下一轮事件才通知宿主，确保本次原生 loadContent 已完成。
      setTimeout(() => context.getApplicationContext().eventHub.emit('huiwen.screenshot.contentReady'), 0);
    }
  }

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      XComponent({
        id: this.windowId,
        type: XComponentType.NODE,
        libraryname: 'plugins_platforms_qopenharmony'
      })
        .width('100%')
        .height('100%')
    }
    .width('100%')
    .height('100%')
    .onAppear(() => this.notifyScreenshotReady())
  }
}
