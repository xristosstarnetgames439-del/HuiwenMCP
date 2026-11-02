// 在临时目录检查 MD 构建链路，不调用原生编译、HAP 打包或设备运行。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  appendFileSync, cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync
} from 'node:fs';
import { createRequire } from 'node:module';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const sandbox = mkdtempSync(path.join(os.tmpdir(), 'huiwen-md-'));
const relativeOutput = 'entry/src/main/resources/rawfile/markdown';
const sha256 = (data) => createHash('sha256').update(data).digest('hex');
const readOutput = (name) => readFileSync(path.join(sandbox, relativeOutput, name));
function run() {
  return spawnSync(process.execPath, [path.join(sandbox, 'third_party/markdown/build.mjs')], {
    cwd: sandbox, encoding: 'utf8', timeout: 180000
  });
}

try {
  const source = path.join(sandbox, 'third_party/markdown');
  mkdirSync(source, { recursive: true });
  for (const name of ['package.json', 'package-lock.json', 'library.js', 'library.css', 'preview.html']) {
    cpSync(path.join(root, 'third_party/markdown', name), path.join(source, name), { recursive: true });
  }
  const core = path.join(root, 'third_party/markdown/source');
  cpSync(core, path.join(source, 'source'), {
    recursive: true,
    filter: (file) => !['.git', 'node_modules', 'dist'].includes(path.relative(core, file).split(path.sep)[0])
  });
  cpSync(path.join(root, 'third_party/markdown/build.mjs'), path.join(sandbox, 'third_party/markdown/build.mjs'));
  assert.equal(existsSync(path.join(source, 'node_modules')), false);
  const first = run();
  assert.equal(first.status, 0, first.stdout + first.stderr);
  assert.match(first.stdout, /DEPENDENCIES install/);
  const context = vm.createContext({
    console, setTimeout, clearTimeout
  });
  const api = vm.runInContext(`${readOutput('huiwen-markdown.js')}\nHuiwenMarkdown;`, context);
  assert.equal(api.EditorState.create({ doc: '# 标题' }).doc.toString(), '# 标题');
  assert.equal(typeof api.EditorView, 'function');
  assert.equal(typeof api.markdown, 'function');
  assert.ok(api.parser.parse('# 标题').toString().includes('ATXHeading1'));
  assert.ok(api.parser.configure(api.GFM).parse('| a |\n| --- |\n| b |').toString().includes('Table'));
  const html = api.renderMarkdown('# 标题\n\n**粗体**\n\n| a |\n| --- |\n| b |\n\n<script>alert(1)</script>');
  assert.match(html, /<h1>标题<\/h1>/);
  assert.match(html, /<strong>粗体<\/strong>/);
  assert.match(html, /<table>/);
  assert.ok(!html.includes('<script>'));
  assert.ok(!api.renderMarkdown('[危险链接](javascript:alert(1))').includes('href="javascript:'));
  const marked = '<!-- PDF page 1 -->\n\n第一页\n\n<!-- PDF page 2 -->\n\n第二页' +
    '\n\n<!-- PDF page 3 -->\n\n第三页';
  const markedHtml = api.renderPreview(marked);
  assert.match(markedHtml, /data-pdf-page="1"/);
  assert.match(markedHtml, /data-pdf-page="2"/);
  assert.ok(!markedHtml.includes('&lt;!-- PDF page'));
  assert.ok(!api.renderPreview('```md\n<!-- PDF page 1 -->\n```').includes('data-pdf-page'));
  assert.ok(!api.renderPreview('- <!-- PDF page 1 -->').includes('data-pdf-page'));
  assert.ok(!api.renderPreview(marked + '\n\n<!-- PDF page 1 -->').includes('data-pdf-page'));
  assert.ok(!api.renderPreview('<!-- PDF page 1 -->\n\n<script>alert(1)</script>').includes('<script>'));

  // 在隔离 DOM 中检查页锚点、百分比回退和异步 scroll 的回传抑制。
  const events = new Map();
  const frames = [];
  const notices = [];
  let pageTops = [24, 700, 1400];
  const browser = {
    scrollY: 0, innerHeight: 400,
    addEventListener: (name, callback) => events.set(name, callback),
    requestAnimationFrame: (callback) => frames.push(callback),
    huiwenReportScroll: (...args) => notices.push(args),
    scrollTo: (value, y) => {
      browser.scrollY = typeof value === 'object' ? value.top : y;
      events.get('scroll')?.();
    }
  };
  const body = {
    innerHTML: '',
    querySelectorAll: () => pageTops.map((_, i) => ({
      dataset: { pdfPage: String(i + 1) },
      getBoundingClientRect: () => ({ top: pageTops[i] - browser.scrollY, height: 450 })
    }))
  };
  context.window = browser;
  context.document = { documentElement: { scrollHeight: 2000 }, getElementById: () => body };
  context.ResizeObserver = class {
    constructor(callback) { this.callback = callback; }
    observe() { this.callback(); }
    disconnect() {}
  };
  const flushFrames = () => { while (frames.length) frames.shift()(); };
  api.showPreview(marked);
  flushFrames();
  assert.equal(notices.length, 1, 'ResizeObserver 不能吞掉首次正文就绪通知');
  api.setScrollPosition(2, 0.5, 0.3);
  flushFrames();
  assert.equal(browser.scrollY, 1050);
  assert.equal(notices.length, 1, '程序滚动不能回传造成循环');
  browser.scrollY = 1120;
  events.get('scroll')();
  flushFrames();
  assert.deepEqual(notices.at(-1), [2, 0.6, 0.7]);
  api.setScrollPosition(3, 0.4, 1);
  assert.equal(browser.scrollY, 1600);
  api.setScrollPosition(2, 0.5, 0);
  assert.equal(browser.scrollY, 0);
  pageTops = [];
  api.showPreview('无页码的旧文档');
  flushFrames();
  api.setScrollPosition(7, 0.1, 0.5);
  flushFrames();
  assert.equal(browser.scrollY, 800);
  assert.equal(api.scrollPosition().page, 0);
  assert.equal(api.scrollPosition().progress, 0.5);
  const page = readOutput('preview.html').toString();
  assert.match(page, /Content-Security-Policy/);
  assert.ok(!page.includes('HUIWEN_PREVIEW_SCRIPT'));
  assert.match(page, /id="markdown-body"/);
  assert.match(readOutput('huiwen-markdown.css').toString(), /\.cm-editor/);
  const metadata = JSON.parse(readOutput('build.json'));
  for (const [name, expected] of Object.entries(metadata.files)) {
    const contents = readOutput(name);
    assert.equal(contents.length, expected.bytes);
    assert.equal(sha256(contents), expected.sha256);
  }
  assert.match(readOutput('THIRD_PARTY_LICENSES.txt').toString(), /markdown-it@14\.3\.3/);
  assert.ok(Object.keys(metadata.inputs).some((name) => name.startsWith('source/vendor/codemirror-state/src/')));
  assert.ok(!Object.keys(metadata.inputs).some((name) => name.startsWith('node_modules/')));

  rmSync(path.join(source, 'node_modules', `@esbuild/${process.platform}-${process.arch}`), { recursive: true });
  const repaired = run();
  assert.equal(repaired.status, 0, repaired.stdout + repaired.stderr);
  assert.match(repaired.stdout, /DEPENDENCIES install/);

  const initialHash = sha256(readOutput('huiwen-markdown.js'));
  appendFileSync(path.join(source, 'library.js'), '\nexport const buildProbe = "HUIWEN_MD_UPDATE_CHECK";\n');
  appendFileSync(path.join(source, 'library.css'), '\n.huiwen-md-update-check { color: red; }\n');
  const second = run();
  assert.equal(second.status, 0, second.stdout + second.stderr);
  assert.match(second.stdout, /DEPENDENCIES cached/);
  assert.notEqual(sha256(readOutput('huiwen-markdown.js')), initialHash);
  assert.match(readOutput('huiwen-markdown.js').toString(), /HUIWEN_MD_UPDATE_CHECK/);
  assert.match(readOutput('huiwen-markdown.css').toString(), /huiwen-md-update-check/);
  appendFileSync(path.join(source, 'source/vendor/codemirror-state/src/index.ts'),
    '\nexport const huiwenCoreProbe = "HUIWEN_MD_CORE_UPDATE_CHECK";\n');
  appendFileSync(path.join(source, 'library.js'), '\nexport { huiwenCoreProbe } from "@codemirror/state";\n');
  const coreUpdated = run();
  assert.equal(coreUpdated.status, 0, coreUpdated.stdout + coreUpdated.stderr);
  assert.match(readOutput('huiwen-markdown.js').toString(), /HUIWEN_MD_CORE_UPDATE_CHECK/);
  const validHash = sha256(readOutput('huiwen-markdown.js'));
  appendFileSync(path.join(source, 'library.js'), '\nexport const invalid = ;\n');
  const failed = run();
  assert.equal(failed.status, 1);
  assert.match(failed.stderr, /HUIWEN_MD.*FAILED/);
  assert.equal(sha256(readOutput('huiwen-markdown.js')), validHash);

  // 验证真实 hvigorfile 的任务注册、先后依赖以及未吞掉构建异常。
  const require = createRequire(path.join(source, 'package.json'));
  const transformed = require('esbuild').transformSync(readFileSync(path.join(root, 'entry/hvigorfile.ts'), 'utf8'), {
    loader: 'ts', format: 'cjs'
  });
  const tasks = [];
  const calls = [];
  const injected = new Error('build failed');
  const module = { exports: {} };
  vm.runInNewContext(transformed.code, {
    module, exports: module.exports, process,
    require: (name) => {
      if (name === '@ohos/hvigor-ohos-plugin') return { hapTasks: {} };
      if (name === 'child_process') return { execFileSync: (...args) => { calls.push(args); throw injected; } };
      return require(name);
    }
  });
  for (const plugin of module.exports.default.plugins) plugin.apply({ registerTask: (task) => tasks.push(task) });
  const task = tasks.find((item) => item.name === 'PrepareMarkdownResources');
  assert.ok(task);
  assert.ok(task.dependencies.includes('default@PreBuild'));
  assert.ok(task.dependencies.includes('PrepareNativeDependencies'));
  for (const name of ['default@ConfigureCmake', 'default@CompileResource', 'default@PackageHap']) {
    assert.ok(task.postDependencies.includes(name));
  }
  assert.throws(() => task.run({ modulePath: path.join(sandbox, 'entry') }), (error) => error === injected);
  assert.equal(calls[0][0], process.execPath);
  assert.equal(calls[0][1][0], path.join(sandbox, 'third_party/markdown/build.mjs'));
  console.info('MARKDOWN_BUILD_OK: 首次还原、缺包修复、离线库接口、源码更新、失败阻断、任务注册检查通过');
} finally {
  rmSync(sandbox, { recursive: true, force: true });
}
