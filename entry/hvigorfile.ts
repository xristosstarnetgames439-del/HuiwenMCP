import { hapTasks } from '@ohos/hvigor-ohos-plugin';
import { execFileSync } from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

// 默认 PackageHap 不带 HNP 路径；在签名前重打未签名 HAP，供设备安装器解包。
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
        const packingTool = sdkRoots.flatMap((root) => [
          path.join(root, 'default/openharmony/toolchains/lib/app_packing_tool.jar'),
          path.join(root, 'toolchains/lib/app_packing_tool.jar')
        ]).find((candidate) => fs.existsSync(candidate));

        if (!packingTool || !fs.existsSync(hnpFile) || fs.statSync(hnpFile).size === 0) {
          throw new Error('缺少 app_packing_tool.jar 或 huiwen_python.hnp；请先运行 packaging/ohos/pack_python_hnp.sh');
        }

        const args = [
          '-jar', packingTool, '--mode', 'hap', '--force', 'true',
          '--json-path', path.join(buildRoot, 'intermediates/package', target, 'module.json'),
          '--resources-path', path.join(buildRoot, 'intermediates/res', target, 'resources'),
          '--index-path', path.join(buildRoot, 'intermediates/res', target, 'resources.index'),
          '--pack-info-path', path.join(outputRoot, 'pack.info'),
          '--lib-path', path.join(buildRoot, 'intermediates/stripped_native_libs', target),
          '--ets-path', path.join(buildRoot, 'intermediates/loader_out', target, 'ets'),
          '--hnp-path', hnpRoot, '--out-path', outPath
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
      }
    });
  }
};

export default {
  system: hapTasks, /* Built-in plugin of Hvigor. It cannot be modified. */
  plugins: [repackHapWithHnpPlugin]
}
