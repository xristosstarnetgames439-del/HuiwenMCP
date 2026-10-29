import { hapTasks } from '@ohos/hvigor-ohos-plugin';
import { execFileSync } from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

// 每次 Run 都先检查 PDFium 源码并增量构建，再收集 libs 和许可证资源。
const prepareNativeDependenciesPlugin = {
  pluginId: 'huiwen.prepare-native-dependencies',
  apply(node: any) {
    node.registerTask({
      name: 'PrepareNativeDependencies',
      dependencies: ['default@PreBuild'],
      postDependencies: ['default@ConfigureCmake', 'default@ProcessLibs', 'default@CompileResource'],
      run: (context: any) => {
        const projectRoot = path.resolve(context.modulePath.toString(), '..');
        const propertiesFile = path.join(projectRoot, 'local.properties');
        const properties = fs.existsSync(propertiesFile) ? fs.readFileSync(propertiesFile, 'utf8') : '';
        const pythonSetting =
          process.env.HOST_PYTHON ?? properties.match(/^\s*python\.host\.path\s*=\s*(.+)$/m)?.[1].trim() ?? 'python3';
        const python = pythonSetting.includes(path.sep) ? path.resolve(projectRoot, pythonSetting) : pythonSetting;
        execFileSync(python, [path.join(projectRoot, 'packaging/ohos/prepare_prebuilt.py')], {
          cwd: projectRoot,
          stdio: 'inherit'
        });
        execFileSync(python, [path.join(projectRoot, 'packaging/ohos/build_pdfium.py')], {
          cwd: projectRoot,
          stdio: 'inherit'
        });
      }
    });
  }
};

// 每次 Run 准备 Markdown 与 Python 业务资源，内容相同则保留产物和时间戳。
const prepareMarkdownResourcesPlugin = {
  pluginId: 'huiwen.prepare-markdown-resources',
  apply(node: any) {
    node.registerTask({
      name: 'PrepareMarkdownResources',
      dependencies: ['default@PreBuild', 'PrepareNativeDependencies'],
      postDependencies: ['default@ConfigureCmake', 'default@CompileResource', 'default@PackageHap'],
      run: (context: any) => {
        const projectRoot = path.resolve(context.modulePath.toString(), '..');
        execFileSync(process.execPath, [path.join(projectRoot, 'packaging/ohos/build_markdown.mjs')], {
          cwd: projectRoot,
          stdio: 'inherit'
        });
        const propertiesFile = path.join(projectRoot, 'local.properties');
        const properties = fs.existsSync(propertiesFile) ? fs.readFileSync(propertiesFile, 'utf8') : '';
        const pythonSetting =
          process.env.HOST_PYTHON ?? properties.match(/^\s*python\.host\.path\s*=\s*(.+)$/m)?.[1].trim() ?? 'python3';
        const python = pythonSetting.includes(path.sep) ? path.resolve(projectRoot, pythonSetting) : pythonSetting;
        // 业务 pyz 独立准备，源码变化不再触发环境 HNP 重打包。
        execFileSync(python, [path.join(projectRoot, 'packaging/ohos/build_python_bundle.py')], {
          cwd: projectRoot,
          stdio: 'inherit'
        });
        execFileSync(python, [path.join(projectRoot, 'packaging/ohos/prepare_cef_preview.py')], {
          cwd: projectRoot,
          stdio: 'inherit'
        });
      }
    });
  }
};

// HNP 输入不变时复用成品；每次仍在签名前注入 HNP 并校验未签名 HAP。
const repackHapWithHnpPlugin = {
  pluginId: 'huiwen.repack-hap-with-hnp',
  apply(node: any) {
    node.registerTask({
      name: 'RepackHapWithHnp',
      dependencies: ['default@PackageHap'],
      postDependencies: ['default@SignHap'],
      run: (context: any) => {
        const modulePath = context.modulePath.toString();
        const projectRoot = path.resolve(modulePath, '..');
        const target = context.targetName ?? 'default';
        const buildRoot = path.join(modulePath, 'build', target);
        const outputRoot = path.join(buildRoot, 'outputs', target);
        const hnpRoot = path.join(projectRoot, 'hnp');
        const hnpEntry = 'hnp/arm64-v8a/huiwen_python.hnp';
        const hnpFile = path.join(hnpRoot, 'arm64-v8a', 'huiwen_python.hnp');
        const outPath = path.join(outputRoot, `${context.moduleName}-${target}-unsigned.hap`);
        const sdkRoots = [
          process.env.DEVECO_SDK_HOME,
          process.env.OHOS_SDK_HOME,
          '/Applications/DevEco-Studio.app/Contents/sdk'
        ].filter((root): root is string => root !== undefined && root !== '');
        const packingTool = sdkRoots
          .flatMap((root) => [
            path.join(root, 'default/openharmony/toolchains/lib/app_packing_tool.jar'),
            path.join(root, 'toolchains/lib/app_packing_tool.jar')
          ])
          .find((candidate) => fs.existsSync(candidate));

        if (!packingTool) {
          throw new Error('缺少 app_packing_tool.jar；请配置 DEVECO_SDK_HOME 或 OHOS_SDK_HOME');
        }
        const propertiesFile = path.join(projectRoot, 'local.properties');
        const properties = fs.existsSync(propertiesFile) ? fs.readFileSync(propertiesFile, 'utf8') : '';
        const runtimeSetting =
          process.env.HUIWEN_PYTHON_RUNTIME ?? properties.match(/^\s*python\.runtime\.dir\s*=\s*(.+)$/m)?.[1].trim();
        const hostPython =
          process.env.HOST_PYTHON ?? properties.match(/^\s*python\.host\.path\s*=\s*(.+)$/m)?.[1].trim();
        if (!runtimeSetting) {
          throw new Error('请在 local.properties 配置 python.runtime.dir，或设置 HUIWEN_PYTHON_RUNTIME');
        }
        const runtimeDir = path.resolve(projectRoot, runtimeSetting);
        const hnpcli =
          process.env.HNPCLI ??
          sdkRoots
            .flatMap((root) => [
              path.join(root, 'default/openharmony/toolchains/hnpcli'),
              path.join(root, 'toolchains/hnpcli')
            ])
            .find((candidate) => fs.existsSync(candidate));
        console.info(`[HUIWEN_HNP] START runtime=${runtimeDir}`);
        execFileSync('bash', [path.join(projectRoot, 'packaging/ohos/pack_python_hnp.sh'), runtimeDir], {
          cwd: projectRoot,
          stdio: 'inherit',
          env: {
            ...process.env,
            ...(hostPython
              ? { HOST_PYTHON: hostPython.includes(path.sep) ? path.resolve(projectRoot, hostPython) : hostPython }
              : {}),
            ...(hnpcli ? { HNPCLI: hnpcli } : {})
          }
        });
        if (!fs.existsSync(hnpFile) || fs.statSync(hnpFile).size === 0) {
          throw new Error('HNP 准备后未找到有效产物，停止 HAP 打包');
        }
        console.info(`[HUIWEN_HNP] DONE file=${hnpFile} bytes=${fs.statSync(hnpFile).size}`);

        const args = [
          '-jar',
          packingTool,
          '--mode',
          'hap',
          '--force',
          'true',
          '--json-path',
          path.join(buildRoot, 'intermediates/package', target, 'module.json'),
          '--resources-path',
          path.join(buildRoot, 'intermediates/res', target, 'resources'),
          '--index-path',
          path.join(buildRoot, 'intermediates/res', target, 'resources.index'),
          '--pack-info-path',
          path.join(outputRoot, 'pack.info'),
          '--lib-path',
          path.join(buildRoot, 'intermediates/stripped_native_libs', target),
          '--ets-path',
          path.join(buildRoot, 'intermediates/loader_out', target, 'ets'),
          '--hnp-path',
          hnpRoot,
          '--out-path',
          outPath
        ];
        const pkgContext = path.join(buildRoot, 'intermediates/loader', target, 'pkgContextInfo.json');
        if (fs.existsSync(pkgContext)) {
          args.push('--pkg-context-path', pkgContext);
        }
        execFileSync('java', args, { cwd: projectRoot, stdio: 'inherit' });

        // 发现 HNP 未进入 HAP 时立刻停止签名，避免再次得到无法安装的产物。
        const entries = execFileSync('unzip', ['-Z1', outPath], { encoding: 'utf8' }).split(/\r?\n/);
        if (!entries.includes(hnpEntry)) {
          throw new Error(`未签名 HAP 中缺少 ${hnpEntry}`);
        }
        const pythonBundleEntry = 'resources/rawfile/python/huiwen_converter.pyz';
        if (!entries.includes(pythonBundleEntry)) {
          throw new Error(`未签名 HAP 中缺少 ${pythonBundleEntry}`);
        }
      }
    });
  }
};

export default {
  system: hapTasks /* Built-in plugin of Hvigor. It cannot be modified. */,
  plugins: [prepareNativeDependenciesPlugin, prepareMarkdownResourcesPlugin, repackHapWithHnpPlugin]
};
