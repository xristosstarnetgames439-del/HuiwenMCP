// 构建可离线加载的浏览器资源；不启动页面或修改原生库。
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  existsSync, mkdirSync, mkdtempSync, readFileSync, renameSync, rmSync, writeFileSync
} from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const directory = path.join(root, 'third_party/markdown');
const output = path.join(root, 'entry/src/main/resources/rawfile/markdown');
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
  if (!complete || !existsSync(stamp) || readFileSync(stamp, 'utf8') !== fingerprint) {
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
    console.info('[HUIWEN_MD] DEPENDENCIES install');
    execFileSync(process.execPath, [npm, 'ci', '--include=dev', '--include=optional',
      '--ignore-scripts', '--no-audit', '--no-fund', '--registry=https://registry.npmjs.org'], {
      cwd: directory,
      stdio: 'inherit',
      env: { ...process.env, PATH: `${nodeDirectory}${path.delimiter}${process.env.PATH ?? ''}` }
    });
    writeFileSync(stamp, fingerprint);
  } else {
    console.info('[HUIWEN_MD] DEPENDENCIES cached');
  }
}

async function main() {
  console.info('[HUIWEN_MD] START');
  prepareDependencies();
  const require = createRequire(path.join(directory, 'package.json'));
  const { build } = require('esbuild');
  const result = await build({
    absWorkingDir: directory,
    entryPoints: ['library.js'],
    outfile: path.join(output, 'huiwen-markdown.js'),
    bundle: true,
    platform: 'browser',
    format: 'iife',
    globalName: 'HuiwenMarkdown',
    target: ['chrome114'],
    minify: true,
    legalComments: 'inline',
    metafile: true,
    write: false
  });
  const assets = new Map(result.outputFiles.map((file) => [path.basename(file.path), file.contents]));
  for (const name of ['huiwen-markdown.js', 'huiwen-markdown.css']) {
    if (!assets.get(name)?.length) throw new Error(`Markdown 构建缺少产物：${name}`);
  }
  if (Object.values(result.metafile.outputs).some((file) => file.imports.some((item) => item.external))) {
    throw new Error('Markdown 浏览器资源仍引用外部模块');
  }
  assets.set('THIRD_PARTY_LICENSES.txt', readFileSync(path.join(directory, 'licenses/THIRD_PARTY_LICENSES.txt')));
  const page = readFileSync(path.join(directory, 'preview.html'), 'utf8')
    .replace('/* HUIWEN_PREVIEW_STYLE */', () => Buffer.from(assets.get('huiwen-markdown.css')).toString('utf8'))
    .replace('/* HUIWEN_PREVIEW_SCRIPT */', () => Buffer.from(assets.get('huiwen-markdown.js')).toString('utf8')
      .replace(/<\/script/gi, '<\\/script'));
  assets.set('preview.html', Buffer.from(page));
  const metadata = {
    format: 'iife', global: 'HuiwenMarkdown', target: 'chrome114',
    lock_sha256: sha256(readFileSync(path.join(directory, 'package-lock.json'))),
    inputs: Object.fromEntries([...Object.keys(result.metafile.inputs), 'preview.html'].sort().map((name) => [
      name, sha256(readFileSync(path.resolve(directory, name)))
    ])),
    files: Object.fromEntries([...assets].map(([name, contents]) => [name, {
      bytes: contents.length, sha256: sha256(contents)
    }]))
  };
  assets.set('build.json', Buffer.from(`${JSON.stringify(metadata, null, 2)}\n`));
  // 内容相同的文件保留时间戳，避免 Qt RCC 和 HAP 资源处理无意义地重新执行。
  const changed = [...assets].filter(([name, contents]) => {
    const target = path.join(output, name);
    return !existsSync(target) || !Buffer.from(contents).equals(readFileSync(target));
  });
  if (changed.length === 0) {
    console.info(`[HUIWEN_MD] CACHED output=${output}`);
    return;
  }
  mkdirSync(output, { recursive: true });
  const cache = path.join(root, '.native-tools');
  mkdirSync(cache, { recursive: true });
  const staging = mkdtempSync(path.join(cache, 'markdown-'));
  try {
    // 所有变化文件写入成功后才逐个原子替换；失败继续阻止 HAP 打包。
    for (const [name, contents] of changed) writeFileSync(path.join(staging, name), contents);
    for (const [name] of changed) renameSync(path.join(staging, name), path.join(output, name));
  } finally {
    rmSync(staging, { recursive: true, force: true });
  }
  console.info(`[HUIWEN_MD] GENERATED output=${output} files=${changed.length}`);
}

main().catch((error) => {
  console.error(`[HUIWEN_MD] FAILED ${error.message}`);
  process.exitCode = 1;
});
