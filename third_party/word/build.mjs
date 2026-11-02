// 从 PortOne fork 源码构建离线库，不启动页面或重编 Qt/CEF。
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync
} from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const directory = path.join(root, 'third_party/word');
const source = path.join(root, 'third_party/word/source');
const output = path.join(root, 'entry/src/main/resources/rawfile/word');
const readJson = (file) => JSON.parse(readFileSync(file, 'utf8'));
const sha256 = (data) => createHash('sha256').update(data).digest('hex');

function prepareDependencies() {
  const manifest = path.join(directory, 'package.json');
  const lockfile = path.join(directory, 'package-lock.json');
  const lock = readJson(lockfile);
  const fingerprint = sha256(Buffer.concat([
    readFileSync(manifest), readFileSync(lockfile), Buffer.from(`${process.platform}-${process.arch}`)
  ]));
  const stamp = path.join(directory, 'node_modules/.huiwen-install.sha256');
  const binary = path.join(directory, 'node_modules', `@esbuild/${process.platform}-${process.arch}`,
    process.platform === 'win32' ? 'esbuild.exe' : 'bin/esbuild');
  const complete = existsSync(binary) && Object.entries(lock.packages).every(([name, info]) => {
    if (!name || info.optional) return true;
    const installed = path.join(directory, name, 'package.json');
    return existsSync(installed) && readJson(installed).version === info.version;
  });
  if (complete && existsSync(stamp) && readFileSync(stamp, 'utf8') === fingerprint) {
    console.info('[HUIWEN_WORD] DEPENDENCIES cached');
    return;
  }
  const nodeDirectory = path.dirname(process.execPath);
  const candidates = [
    path.resolve(nodeDirectory, '../lib/node_modules/npm/bin/npm-cli.js'),
    path.join(nodeDirectory, 'node_modules/npm/bin/npm-cli.js')
  ];
  if (process.env.NODE_HOME) {
    candidates.push(path.join(process.env.NODE_HOME, 'lib/node_modules/npm/bin/npm-cli.js'),
      path.join(process.env.NODE_HOME, 'node_modules/npm/bin/npm-cli.js'));
  }
  const npm = candidates.find(existsSync);
  if (!npm) throw new Error('找不到随 Node 提供的 npm，请使用 DevEco 自带的 Node 运行构建');
  console.info('[HUIWEN_WORD] DEPENDENCIES install');
  execFileSync(process.execPath, [npm, 'ci', '--include=dev', '--include=optional',
    '--ignore-scripts', '--no-audit', '--no-fund', '--registry=https://registry.npmjs.org'], {
    cwd: directory, stdio: 'inherit',
    env: { ...process.env, PATH: `${nodeDirectory}${path.delimiter}${process.env.PATH ?? ''}` }
  });
  writeFileSync(stamp, fingerprint);
}

function collectLicenses(inputs) {
  const sections = [
    `PortOne @portone/docx-editor ${readJson(path.join(source, 'package.json')).version}\n` +
      readFileSync(path.join(source, 'LICENSE'), 'utf8'),
    readFileSync(path.join(source, 'THIRD_PARTY_NOTICES.md'), 'utf8')
  ];
  const packages = new Set();
  for (const input of inputs) {
    const relative = path.relative(path.join(directory, 'node_modules'), path.resolve(directory, input));
    if (relative.startsWith('..') || path.isAbsolute(relative)) continue;
    const parts = relative.split(path.sep);
    packages.add(parts.slice(0, parts[0].startsWith('@') ? 2 : 1).join('/'));
  }
  for (const name of [...packages].sort()) {
    const location = path.join(directory, 'node_modules', name);
    const manifest = readJson(path.join(location, 'package.json'));
    const notices = readdirSync(location).filter((file) => /^(licen[cs]e|copying|notice)([.-]|$)/i.test(file));
    if (!notices.some((file) => /^(licen[cs]e|copying)/i.test(file))) {
      throw new Error(`缺少运行时依赖许可证：${name}`);
    }
    sections.push(`${name}@${manifest.version} (${manifest.license})\n` +
      notices.sort().map((file) => readFileSync(path.join(location, file), 'utf8')).join('\n'));
  }
  return sections.join('\n\n------------------------------------------------------------\n\n');
}

async function main() {
  console.info('[HUIWEN_WORD] START');
  if (!existsSync(path.join(source, 'src/index.ts'))) {
    throw new Error('缺少 PortOne 源码，请在项目根目录执行 git submodule update --init --recursive');
  }
  prepareDependencies();
  const require = createRequire(path.join(directory, 'package.json'));
  const { build } = require('esbuild');
  const result = await build({
    absWorkingDir: directory, entryPoints: ['library.ts'],
    outfile: path.join(output, 'huiwen-word.js'),
    nodePaths: [path.join(directory, 'node_modules')],
    bundle: true, platform: 'browser', format: 'iife', globalName: 'HuiwenWord',
    target: ['chrome114'], jsx: 'automatic', define: { 'process.env.NODE_ENV': '"production"' },
    minify: true, legalComments: 'inline', metafile: true, write: false
  });
  if (Object.values(result.metafile.outputs).some((file) => file.imports.some((item) => item.external))) {
    throw new Error('Word 浏览器资源仍引用外部模块');
  }
  const assets = new Map(result.outputFiles.map((file) => [path.basename(file.path), file.contents]));
  for (const name of ['huiwen-word.js', 'huiwen-word.css']) {
    if (!assets.get(name)?.length) throw new Error(`Word 构建缺少产物：${name}`);
  }
  const inputs = Object.keys(result.metafile.inputs).sort();
  assets.set('THIRD_PARTY_LICENSES.txt', Buffer.from(collectLicenses(inputs)));
  const page = readFileSync(path.join(directory, 'preview.html'), 'utf8')
    .replace('/* HUIWEN_WORD_STYLE */', () => Buffer.from(assets.get('huiwen-word.css')).toString('utf8'))
    .replace('/* HUIWEN_WORD_SCRIPT */', () => Buffer.from(assets.get('huiwen-word.js')).toString('utf8')
      .replace(/<\/script/gi, '<\\/script'));
  assets.set('preview.html', Buffer.from(page));
  const metadata = {
    format: 'iife', global: 'HuiwenWord', target: 'chrome114',
    source: 'third_party/word/source', version: readJson(path.join(source, 'package.json')).version,
    lock_sha256: sha256(readFileSync(path.join(directory, 'package-lock.json'))),
    inputs: Object.fromEntries(inputs.map((name) => [name, sha256(readFileSync(path.resolve(directory, name)))])),
    files: Object.fromEntries([...assets].map(([name, contents]) => [name, {
      bytes: contents.length, sha256: sha256(contents)
    }]))
  };
  assets.set('build.json', Buffer.from(`${JSON.stringify(metadata, null, 2)}\n`));
  const changed = [...assets].filter(([name, contents]) => {
    const target = path.join(output, name);
    return !existsSync(target) || !Buffer.from(contents).equals(readFileSync(target));
  });
  if (!changed.length) {
    console.info(`[HUIWEN_WORD] CACHED output=${output}`);
    return;
  }
  mkdirSync(output, { recursive: true });
  const cache = path.join(root, '.native-tools');
  mkdirSync(cache, { recursive: true });
  const staging = mkdtempSync(path.join(cache, 'word-'));
  try {
    for (const [name, contents] of changed) writeFileSync(path.join(staging, name), contents);
    for (const [name] of changed) renameSync(path.join(staging, name), path.join(output, name));
  } finally {
    rmSync(staging, { recursive: true, force: true });
  }
  console.info(`[HUIWEN_WORD] GENERATED output=${output} files=${changed.length}`);
}

main().catch((error) => {
  console.error(`[HUIWEN_WORD] FAILED ${error.message}`);
  process.exitCode = 1;
});
