/*
 * 汇文 MCP 的私有 HNP Python 进程启动器。
 * 定位应用自带解释器，执行固定脚本并收集输出供 ArkTS 展示。
 */
#include "python_runner.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace
{
constexpr char kBundleName[] = "cn.com.HuiWenMCP";
constexpr char kPackageDir[] = "huiwen_python.org/huiwen_python_1.0.0";

std::string FindPythonRoot()
{
    const char *home = std::getenv("HNP_PRIVATE_HOME");
    const std::string base = (home != nullptr && home[0] != '\0') ? home : "/data/app";
    const std::array<std::string, 2> roots = {
        base + "/" + kBundleName + "/" + kPackageDir,
        base + "/" + kPackageDir,
    };
    for (const auto &root : roots)
    {
        if (access((root + "/bin/python3").c_str(), X_OK) == 0)
        {
            return root;
        }
    }
    throw std::runtime_error("未找到私有 HNP 解释器；请确认 huiwen_python.hnp 已随 HAP 安装。HNP_PRIVATE_HOME=" + base);
}
} // namespace

std::string RunPythonSmoke()
{
    const std::string root = FindPythonRoot();
    const std::string python = root + "/bin/python3";
    const std::string script = root + "/share/huiwen/python_smoke.py";
    if (access(script.c_str(), R_OK) != 0)
    {
        throw std::runtime_error("HNP 中缺少自检脚本: " + script);
    }

    int outputPipe[2];
    if (pipe(outputPipe) != 0)
    {
        throw std::runtime_error("创建 Python 输出管道失败: " + std::string(std::strerror(errno)));
    }
    posix_spawn_file_actions_t actions;
    int code = posix_spawn_file_actions_init(&actions);
    if (code != 0)
    {
        close(outputPipe[0]);
        close(outputPipe[1]);
        throw std::runtime_error("准备 Python 进程失败: " + std::string(std::strerror(code)));
    }
    posix_spawn_file_actions_addclose(&actions, outputPipe[0]);
    posix_spawn_file_actions_adddup2(&actions, outputPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, outputPipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, outputPipe[1]);

    char *const args[] = {const_cast<char *>(python.c_str()), const_cast<char *>("-B"),
                          const_cast<char *>(script.c_str()), nullptr};
    pid_t child = -1;
    code = posix_spawn(&child, python.c_str(), &actions, nullptr, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(outputPipe[1]);
    if (code != 0)
    {
        close(outputPipe[0]);
        throw std::runtime_error("启动 HNP Python 失败: " + std::string(std::strerror(code)));
    }

    std::string output;
    char buffer[512];
    std::string readError;
    for (;;)
    {
        const ssize_t length = read(outputPipe[0], buffer, sizeof(buffer));
        if (length > 0)
        {
            if (output.size() < 4096)
            {
                output.append(buffer, std::min(static_cast<size_t>(length), 4096 - output.size()));
            }
        }
        else if (length == 0)
        {
            break;
        }
        else if (errno != EINTR)
        {
            readError = std::strerror(errno);
            break;
        }
    }
    close(outputPipe[0]);
    int childStatus = 0;
    while (waitpid(child, &childStatus, 0) == -1)
    {
        if (errno != EINTR)
        {
            throw std::runtime_error("等待 Python 退出失败: " + std::string(std::strerror(errno)));
        }
    }
    if (!readError.empty())
    {
        throw std::runtime_error("读取 Python 输出失败: " + readError);
    }
    if (!WIFEXITED(childStatus))
    {
        throw std::runtime_error("Python 进程未正常退出: " + output);
    }
    if (WEXITSTATUS(childStatus) != 0)
    {
        throw std::runtime_error("Python 自检退出码 " + std::to_string(WEXITSTATUS(childStatus)) + ": " + output);
    }
    if (output.find("HUIWEN_PYTHON_OK") == std::string::npos)
    {
        throw std::runtime_error("Python 未返回预期结果: " + output);
    }
    return output;
}
