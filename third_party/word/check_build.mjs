// 临时副本验证 Run 资源链路，不启动编辑器、不构建原生库或 HAP。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { appendFileSync, cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, statSync } from 'node:fs';
import { createRequire } from 'node:module';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const sandbox = mkdtempSync(path.join(os.tmpdir(), 'huiwen-word-'));
const directory = path.join(sandbox, 'third_party/word');
const output = path.join(sandbox, 'entry/src/main/resources/rawfile/word');
const readOutput = (name) => readFileSync(path.join(output, name), 'utf8');
function run() {
  return spawnSync(process.execPath, [path.join(sandbox, 'third_party/word/build.mjs')], {
    cwd: sandbox, encoding: 'utf8', timeout: 120000
  });
}

try {
  assert.ok(existsSync(path.join(root, 'third_party/word/node_modules/.huiwen-install.sha256')),
    '先运行 third_party/word/build.mjs 准备锁定依赖');
  mkdirSync(path.join(sandbox, 'third_party/word/source'), { recursive: true });
  for (const name of ['package.json', 'package-lock.json', 'library.ts', 'preview.css', 'preview.html', 'node_modules']) {
    cpSync(path.join(root, 'third_party/word', name), path.join(directory, name), { recursive: true });
  }
  for (const name of ['src', 'package.json', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'tsconfig.json']) {
    cpSync(path.join(root, 'third_party/word/source', name),
      path.join(sandbox, 'third_party/word/source', name), { recursive: true });
  }
  cpSync(path.join(root, 'third_party/word/build.mjs'), path.join(sandbox, 'third_party/word/build.mjs'));
  const first = run();
  assert.equal(first.status, 0, first.stdout + first.stderr);
  assert.match(first.stdout, /DEPENDENCIES cached/);
  for (const name of ['huiwen-word.js', 'huiwen-word.css', 'preview.html', 'THIRD_PARTY_LICENSES.txt', 'build.json']) {
    assert.ok(readOutput(name).length > 0);
  }
  const metadata = JSON.parse(readOutput('build.json'));
  assert.equal(metadata.target, 'chrome114');
  assert.ok(Object.keys(metadata.inputs).some((name) => name.includes('source/src/DocxEditor.tsx')));
  assert.match(readOutput('preview.html'), /word-body/);
  assert.match(readOutput('preview.html'), /window.HuiwenPreview = window.HuiwenWord/);
  assert.doesNotMatch(readOutput('preview.html'), /HUIWEN_WORD_(STYLE|SCRIPT)/);
  assert.match(readOutput('THIRD_PARTY_LICENSES.txt'), /Apache License/);
  assert.match(readOutput('THIRD_PARTY_LICENSES.txt'), /react@18\.3\.1/);
  assert.match(readOutput('THIRD_PARTY_LICENSES.txt'), /prosemirror-view@/);
  assert.match(readOutput('huiwen-word.css'), /\.ProseMirror/);
  const api = vm.runInNewContext(`${readOutput('huiwen-word.js')}\nHuiwenWord;`, {
    console, setTimeout, clearTimeout, TextEncoder, TextDecoder
  });
  assert.equal(typeof api.DocxEditor, 'object'); // React.forwardRef component
  assert.equal(typeof api.createRoot, 'function');
  assert.equal(typeof api.core.importDocx, 'function');
  assert.equal(typeof api.core.exportDocx, 'function');
  assert.equal(typeof api.showPreview, 'function');
  assert.equal(typeof api.scrollPosition, 'function');
  const before = statSync(path.join(output, 'huiwen-word.js')).mtimeMs;
  const second = run();
  assert.equal(second.status, 0, second.stdout + second.stderr);
  assert.match(second.stdout, /CACHED output=/);
  assert.equal(statSync(path.join(output, 'huiwen-word.js')).mtimeMs, before);
  const forkEntry = path.join(sandbox, 'third_party/word/source/src/index.ts');
  appendFileSync(forkEntry, '\nexport const huiwenWordUpdateProbe = "HUIWEN_WORD_UPDATE_CHECK";\n');
  const updated = run();
  assert.equal(updated.status, 0, updated.stdout + updated.stderr);
  assert.match(readOutput('huiwen-word.js'), /HUIWEN_WORD_UPDATE_CHECK/);
  const valid = readOutput('huiwen-word.js');
  appendFileSync(forkEntry, '\nexport const invalid = ;\n');
  const failed = run();
  assert.notEqual(failed.status, 0);
  assert.match(failed.stderr, /HUIWEN_WORD.*FAILED/s);
  assert.equal(readOutput('huiwen-word.js'), valid);

  const require = createRequire(path.join(directory, 'package.json'));
  const transformed = require('esbuild').transformSync(readFileSync(path.join(root, 'entry/hvigorfile.ts'), 'utf8'), {
    loader: 'ts', format: 'cjs'
  });
  const tasks = [];
  const calls = [];
  const injected = new Error('word build failed');
  const module = { exports: {} };
  vm.runInNewContext(transformed.code, {
    module, exports: module.exports, process,
    require: (name) => {
      if (name === '@ohos/hvigor-ohos-plugin') return { hapTasks: {} };
      if (name === 'child_process') return { execFileSync: (...args) => {
        calls.push(args);
        if (args[1][0] === path.join(sandbox, 'third_party/word/build.mjs')) throw injected;
      } };
      return require(name);
    }
  });
  for (const plugin of module.exports.default.plugins) plugin.apply({ registerTask: (task) => tasks.push(task) });
  const task = tasks.find((item) => item.name === 'PrepareMarkdownResources');
  assert.ok(task.dependencies.includes('PrepareNativeDependencies'));
  for (const name of ['default@ConfigureCmake', 'default@CompileResource', 'default@PackageHap']) {
    assert.ok(task.postDependencies.includes(name));
  }
  assert.throws(() => task.run({ modulePath: path.join(sandbox, 'entry') }), (error) => error === injected);
  assert.equal(calls.length, 2); // Word 失败后不执行 Python／CEF 准备。
  assert.equal(calls[1][0], process.execPath);
  assert.equal(calls[1][1][0], path.join(sandbox, 'third_party/word/build.mjs'));
  console.info('WORD_BUILD_OK: 离线库接口、缓存复用、fork 源码更新、失败阻断与任务注册检查通过');
} finally {
  rmSync(sandbox, { recursive: true, force: true });
}
