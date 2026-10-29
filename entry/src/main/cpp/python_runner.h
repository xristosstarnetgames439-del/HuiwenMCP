/*
 * 汇文MCP 的 Python 环境与业务包调用接口。
 * 只执行 HNP 自检和应用内置的转换 pyz，不接收外部命令。
 */
#ifndef HUIWEN_PYTHON_RUNNER_H
#define HUIWEN_PYTHON_RUNNER_H

#include <string>
#include <napi/native_api.h>

// 使用应用 ResourceManager 部署内置 pyz 到缓存；不接受外部业务脚本路径。
void ConfigurePythonBundle(napi_env env, napi_value resources, const std::string &cacheDir);

std::string RunPythonSmoke();
struct PdfConversionResult
{
    int code;
    std::string message;
    std::string outputPath;
};

PdfConversionResult RunPdfConversionResult(const std::string &inputPath, const std::string &outputPath);
// 使用 HNP 解释器执行当前应用的业务包；成功返回可读取的 MD 输出路径。
std::string RunPdfConversion(const std::string &inputPath, const std::string &outputPath);

#endif
