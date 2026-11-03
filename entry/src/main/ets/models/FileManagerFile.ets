// 文件行共享模型：role 为 input/output，kind 为 word/pdf/md，点击时回传完整对象。
import { HarmonySandboxFile } from 'libhuiwen_native.so';

export interface FileManagerFile extends HarmonySandboxFile {
  formatLabel: string;
  sizeLabel: string;
}
