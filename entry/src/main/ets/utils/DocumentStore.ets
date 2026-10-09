// 从系统文档选择器导入文件，并在应用私有沙箱中安排输入、输出路径。
import { common } from '@kit.AbilityKit';
import { fileIo, fileUri, picker } from '@kit.CoreFileKit';

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
  fileIo.mkdirSync(inputDir, true);

  let outputPath = '';
  if (forConversion) {
    const outputDir = `${filesDir}/outputs/md`;
    fileIo.mkdirSync(outputDir, true);
    const stem = name.replace(/\.pdf$/i, '').replace(/[^A-Za-z0-9_\u4e00-\u9fa5-]/g, '_').slice(0, 80);
    outputPath = `${outputDir}/${stem || 'document'}-${id}.md`;
  }

  // Picker 的 URI 只有选取授权；复制后 C++ 和 Python 使用沙箱内的路径。
  const source = await fileIo.open(uri, fileIo.OpenMode.READ_ONLY);
  try {
    await fileIo.copyFile(source.fd, inputPath);
  } catch (error) {
    try {
      fileIo.unlinkSync(inputPath);
    } catch (_) {
      // 复制失败且目标文件不存在时无需清理。
    }
    throw new Error((error as Error).message);
  } finally {
    await fileIo.close(source);
  }

  return { name, uri, inputPath, outputPath };
}
