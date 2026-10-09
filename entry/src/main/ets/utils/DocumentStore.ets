// 从系统文档选择器导入文件，并在应用私有沙箱中安排输入、输出路径。
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { fileIo, fileUri, picker } from '@kit.CoreFileKit';
import hilog from '@ohos.hilog';

function importError(stage: string, error: BusinessError): Error {
  const code = String(error.code ?? 'unknown');
  hilog.error(0xFF00, 'DocumentStore', '%{public}s failed: code=%{public}s', stage, code);
  return new Error(`${stage}失败：${error.message} (code=${code})`);
}

// 沙箱目录会被多次使用，已存在时直接复用。
function ensureDirectory(path: string, stage: string): void {
  try {
    fileIo.mkdirSync(path, true);
  } catch (error) {
    const failure = error as BusinessError;
    if (failure.code !== 13900015) {
      throw importError(stage, failure);
    }
  }
}

export enum DocumentKind {
  PDF = 'pdf',
  Markdown = 'md'
}

export interface PickedDocument {
  name: string;
  uri: string;
  inputPath: string;
  outputPath: string;
}

export async function pickDocumentToSandbox(context: common.UIAbilityContext, kind: DocumentKind,
  forConversion: boolean): Promise<PickedDocument | null> {
  const options = new picker.DocumentSelectOptions();
  options.maxSelectNumber = 1;
  options.fileSuffixFilters = kind === DocumentKind.PDF ? ['PDF|.pdf'] : ['Markdown|.md,.markdown'];
  options.defaultFilePathUri = 'file://docs/storage/Users/currentUser';

  const uris = await new picker.DocumentViewPicker(context).select(options);
  if (uris.length === 0) {
    return null;
  }

  const uri = uris[0];
  const name = new fileUri.FileUri(uri).name;
  const valid = kind === DocumentKind.PDF ? /\.pdf$/i.test(name) : /\.(md|markdown)$/i.test(name);
  if (!valid) {
    throw new Error('请选择对应格式的文件');
  }

  // 使用应用级 filesDir；设备上的物理目录位于当前用户的包名沙箱下。
  const filesDir = context.getApplicationContext().filesDir;
  const id = `${Date.now().toString(36)}-${Math.floor(Math.random() * 0x100000000).toString(36)}`;
  const inputDir = `${filesDir}/inputs/${kind}`;
  const extension = name.toLowerCase().endsWith('.markdown') ? 'markdown' : kind;
  const inputPath = `${inputDir}/${id}.${extension}`;
  ensureDirectory(inputDir, '创建输入目录');

  let outputPath = '';
  if (forConversion) {
    const outputDir = `${filesDir}/outputs/md`;
    ensureDirectory(outputDir, '创建输出目录');
    const stem = name.replace(/\.pdf$/i, '').replace(/[^A-Za-z0-9_\u4e00-\u9fa5-]/g, '_').slice(0, 80);
    outputPath = `${outputDir}/${stem || 'document'}-${id}.md`;
  }

  // Picker 的 URI 只有选取授权；复制后 C++ 和 Python 使用沙箱内的路径。
  let source: fileIo.File;
  try {
    source = await fileIo.open(uri, fileIo.OpenMode.READ_ONLY);
  } catch (error) {
    throw importError('打开所选文件', error as BusinessError);
  }
  try {
    await fileIo.copyFile(source.fd, inputPath);
  } catch (error) {
    try {
      fileIo.unlinkSync(inputPath);
    } catch (_) {
      // 复制失败且目标文件不存在时无需清理。
    }
    throw importError('复制所选文件', error as BusinessError);
  } finally {
    await fileIo.close(source);
  }

  return { name, uri, inputPath, outputPath };
}
