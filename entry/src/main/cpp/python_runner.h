/*
 * 汇文 MCP 的 Python HNP 调用接口。
 * 只执行应用内置的自检脚本，不接收外部命令。
 */
#ifndef HUIWEN_PYTHON_RUNNER_H
#define HUIWEN_PYTHON_RUNNER_H

#include <string>

std::string RunPythonSmoke();

#endif
