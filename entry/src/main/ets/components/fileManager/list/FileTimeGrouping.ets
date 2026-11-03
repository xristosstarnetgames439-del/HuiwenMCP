// 本地日历时间分组：先近三天，再本周、本月，剩余按年月；每个文件只归入一组。
import { FileManagerFile } from '../../../models/FileManagerFile';

export interface FileManagerListEntry {
  key: string;
  title: string;
  file: FileManagerFile | null;
  timeLabel: string;
}

function dayStart(timestamp: number, offset: number = 0): number {
  const date = new Date(timestamp);
  return new Date(date.getFullYear(), date.getMonth(), date.getDate() + offset).getTime();
}

export function fileTimeGroup(timestamp: number, now: number): string {
  const date = new Date(timestamp);
  if (!Number.isFinite(timestamp) || timestamp <= 0 || Number.isNaN(date.getTime())) {
    return '时间未知';
  }
  const current = new Date(now);
  const end = dayStart(now, 1);
  if (timestamp >= dayStart(now, -2) && timestamp < end) {
    return '近3天';
  }
  const monday = dayStart(now, -((current.getDay() + 6) % 7));
  if (timestamp >= monday && timestamp < end) {
    return '本周';
  }
  if (timestamp < end && date.getFullYear() === current.getFullYear() &&
    date.getMonth() === current.getMonth()) {
    return '本月';
  }
  return `${date.getFullYear()}年${date.getMonth() + 1}月`;
}

export function fileTimeLabel(timestamp: number, now: number): string {
  const date = new Date(timestamp);
  if (!Number.isFinite(timestamp) || timestamp <= 0 || Number.isNaN(date.getTime())) {
    return '--';
  }
  const time = `${String(date.getHours()).padStart(2, '0')}:${String(date.getMinutes()).padStart(2, '0')}`;
  const day = dayStart(timestamp);
  if (day === dayStart(now)) {
    return `今天 ${time}`;
  }
  if (day === dayStart(now, -1)) {
    return `昨天 ${time}`;
  }
  if (day === dayStart(now, -2)) {
    return `前天 ${time}`;
  }
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
}

export function groupFileManagerFiles(files: FileManagerFile[], now: number): FileManagerListEntry[] {
  const entries: FileManagerListEntry[] = [];
  let previousGroup = '';
  for (const file of files) {
    const title = fileTimeGroup(file.lastUsedAt, now);
    if (title !== previousGroup) {
      entries.push({ key: `group:${title}`, title, file: null, timeLabel: '' });
      previousGroup = title;
    }
    entries.push({ key: file.path, title: '', file, timeLabel: fileTimeLabel(file.lastUsedAt, now) });
  }
  return entries;
}
