// 文件管理右侧内容入口：接收当前分类，本阶段留空，不读取沙箱。
@Component
export struct FileManagerContent {
  @Prop selectedCategory: string = '全部';

  build() {
    // TODO：按 selectedCategory 接入文件列表和筛选。
    Column() {}
      .width('100%')
      .height('100%')
      .backgroundColor($r('app.color.home_surface'))
  }
}
