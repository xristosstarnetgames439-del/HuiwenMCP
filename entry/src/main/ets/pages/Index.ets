let storage = LocalStorage.getShared()

@Entry(storage)
@Component
struct Index {
  @LocalStorageProp("idName") windowId: string = "";

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      XComponent({
        id: this.windowId,
        type: XComponentType.NODE,
        libraryname: 'plugins_platforms_qopenharmony'
      }).width('100%')
        .height('100%')
    }
  }
}
