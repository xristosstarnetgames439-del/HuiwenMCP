/*
 * 汇文 MCP 的 Python HNP 调用接口。
 * 只执行应用内置的自检与 PDF 转换脚本，不接收外部命令。
 */
#ifndef HUIWEN_PYTHON_RUNNER_H
#define HUIWEN_PYTHON_RUNNER_H

#include <string>

std::string RunPythonSmoke();
// 调用 HNP 内置的 PDF 转换命令；成功返回可读取的 MD 输出路径。
std::string RunPdfConversion(const std::string &inputPath, const std::string &outputPath);

#endif
