// 系统选择器负责选取授权，沙箱导入由 C++ NAPI 完成。
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { fileIo, fileUri, picker } from '@kit.CoreFileKit';
import hilog from '@ohos.hilog';
import nativeBridge, { HarmonySandboxDocument } from 'libhuiwen_native.so';

function importError(stage: string, error: BusinessError): Error {
  const code = String(error.code ?? 'unknown');
  hilog.error(0xFF00, 'DocumentStore', '%{public}s failed: code=%{public}s', stage, code);
  return new Error(`${stage}失败：${error.message} (code=${code})`);
}

export enum DocumentKind {
  PDF = 'pdf',
  Markdown = 'md',
  Word = 'word'
}

// 操作类型直接对应 files 下的三个归档目录。
export enum DocumentMode {
  Preview = 'preview',
  Edit = 'edit',
  Convert = 'convert'
}

export async function pickDocumentToSandbox(context: common.UIAbilityContext, kind: DocumentKind,
  mode: DocumentMode = DocumentMode.Preview): Promise<HarmonySandboxDocument | null> {
  const options = new picker.DocumentSelectOptions();
  options.maxSelectNumber = 1;
  options.fileSuffixFilters = kind === DocumentKind.PDF ? ['PDF|.pdf'] :
    kind === DocumentKind.Word ? ['Word|.doc,.docx'] :
      ['Markdown|.md,.markdown'];
  options.defaultFilePathUri = 'file://docs/storage/Users/currentUser';

  const uris = await new picker.DocumentViewPicker(context).select(options);
  if (uris.length === 0) {
    return null;
  }

  const uri = uris[0];
  const name = new fileUri.FileUri(uri).name;
  if (kind === DocumentKind.Word && !name.toLowerCase().endsWith('.doc') && !name.toLowerCase().endsWith('.docx')) {
    throw new Error('请选择 .doc 或 .docx 文件');
  }
  let source: fileIo.File;
  try {
    source = await fileIo.open(uri, fileIo.OpenMode.READ_ONLY);
  } catch (error) {
    throw importError('打开所选文件', error as BusinessError);
  }
  try {
    return await nativeBridge.harmonyImportDocument(
      context.getApplicationContext().filesDir, name, source.fd, kind, mode);
  } finally {
    await fileIo.close(source);
  }
}

// 首页和全部工作区复用分页入口；查询本身不修改任务时间。
export function getRecentDocuments(context: common.UIAbilityContext, limit: number = 5,
  mode: string = 'all', offset: number = 0): Promise<HarmonySandboxDocument[]> {
  return nativeBridge.harmonyRecentDocuments(context.getApplicationContext().filesDir, mode, limit, offset);
}
