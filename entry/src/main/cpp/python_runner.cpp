/*
 * 汇文 MCP 的私有 HNP Python 进程启动器。
 * 定位应用自带解释器，异步任务中执行固定脚本并收集运行结果。
 */
#include "python_runner.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

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
std::string RunPythonScript(const char *scriptName, const std::vector<std::string> &paths,
                            const char *successMarker, const char *action)
{
    const std::string root = FindPythonRoot();
    const std::string python = root + "/bin/python3";
    const std::string script = root + "/share/huiwen/" + scriptName;
    if (access(script.c_str(), R_OK) != 0)
    {
        throw std::runtime_error("HNP 中缺少脚本: " + script);
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

    // 参数直接传给固定脚本，不经过 Shell。
    std::vector<char *> args = {const_cast<char *>(python.c_str()), const_cast<char *>("-B"),
                                const_cast<char *>(script.c_str())};
    for (const auto &path : paths)
    {
        args.push_back(const_cast<char *>(path.c_str()));
    }
    args.push_back(nullptr);
    pid_t child = -1;
    code = posix_spawn(&child, python.c_str(), &actions, nullptr, args.data(), environ);
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
            output.append(buffer, static_cast<size_t>(length));
            if (output.size() > 4096)
            {
                output.erase(0, output.size() - 4096);
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
        throw std::runtime_error(std::string(action) + "退出码 " + std::to_string(WEXITSTATUS(childStatus)) + ": " + output);
    }
    if (output.find(successMarker) == std::string::npos)
    {
        throw std::runtime_error(std::string(action) + "未返回预期结果: " + output);
    }
    return output;
}
} // namespace

std::string RunPythonSmoke()
{
    return RunPythonScript("python_smoke.py", {}, "HUIWEN_PYTHON_OK", "Python 自检");
}

std::string RunPdfConversion(const std::string &inputPath, const std::string &outputPath)
{
    if (inputPath.empty() || outputPath.empty())
    {
        throw std::runtime_error("PDF 输入或 MD 输出路径为空");
    }
    RunPythonScript("convert_pdf.py", {inputPath, outputPath}, "HUIWEN_CONVERT_OK", "PDF 转换");
    if (access(outputPath.c_str(), R_OK) != 0)
    {
        throw std::runtime_error("转换完成后未找到 MD 文件: " + outputPath);
    }
    return outputPath;
}
