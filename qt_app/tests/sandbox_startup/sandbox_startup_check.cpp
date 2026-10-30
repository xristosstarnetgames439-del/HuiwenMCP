/* 最近工作区启动回归：先在线程中查询、校验沙箱，再创建 QApplication 并分发鼠标事件。 */
#include "sandbox/SandboxStore.h"
#include <QApplication>
#include <QMouseEvent>
#include <QWidget>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

namespace
{
std::atomic<bool> wrongMainThread{false};

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void logMessage(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (message.contains(QStringLiteral("QApplication was not created in the main() thread")))
        wrongMainThread = true;
    std::cerr << message.toStdString() << std::endl;
    if (type == QtFatalMsg)
        std::abort();
}
} // namespace

int main(int argc, char *argv[])
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
                      ("huiwen-startup-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto task = root / "convert" / fs::u8path("20261028_153000_报告");
    try
    {
        fs::create_directories(task / "input");
        fs::create_directories(task / "output");
        const auto input = task / "input" / fs::u8path("报告.pdf");
        std::ofstream(input, std::ios::binary) << "%PDF-1.4\n";
        const auto oldTime = fs::last_write_time(task);
        qInstallMessageHandler(logMessage);
        std::exception_ptr failure;
        std::promise<void> prepared, release;
        auto preparedFuture = prepared.get_future();
        auto releaseFuture = release.get_future();
        // 禁止先创建 QApplication、QTemporaryDir 或 QFile，否则无法复现启动先后顺序。
        auto worker = std::async(
            std::launch::async,
            [&]
            {
                try
                {
                    const QString filesDir = QString::fromStdString(root.u8string());
                    const QString taskPath = QString::fromStdString(task.u8string());
                    const auto recent = SandboxStore::recentDocuments(filesDir, "all", 3);
                    require(recent.size() == 1, "最近任务读取失败");
                    const auto document = SandboxStore::loadTask(filesDir, taskPath);
                    require(document.inputPath.toStdString() == input.u8string(), "未恢复原 PDF");
                    require(!fs::exists(fs::u8path(document.outputPath.toStdString())),
                            "校验不应生成 Markdown");
                    require(fs::last_write_time(task) == oldTime, "校验不应更新时间");
                    fs::resize_file(input, 0);
                    bool rejected = false;
                    try
                    {
                        SandboxStore::loadTask(filesDir, taskPath);
                    }
                    catch (const std::exception &)
                    {
                        rejected = true;
                    }
                    require(rejected, "空 PDF 应拒绝恢复");
                    // 预览复用同一恢复入口，直接读取任务根目录，不创建 input/output。
                    for (const std::string &suffix : {std::string("pdf"), std::string("md")})
                    {
                        const auto preview = root / "preview" / ("20261030_153000_" + suffix);
                        fs::create_directories(preview);
                        const auto file = preview / ("document." + suffix);
                        std::ofstream(file, std::ios::binary)
                            << (suffix == "pdf" ? "%PDF-1.4\n" : "# title\n");
                        const QString path = QString::fromStdString(preview.u8string());
                        const auto previewTime = fs::last_write_time(preview);
                        const auto item = SandboxStore::loadTask(filesDir, path);
                        require(item.mode == "preview" && item.kind.toStdString() == suffix &&
                                    item.inputPath.toStdString() == file.u8string() &&
                                    item.outputPath.isEmpty(),
                                "预览任务应恢复正确格式和原始文件");
                        require(SandboxStore::validateTaskDirectory(filesDir, path) == path &&
                                    !fs::exists(preview / "input") &&
                                    !fs::exists(preview / "output"),
                                "预览目录校验不能创建转化子目录");
                        require(fs::last_write_time(preview) == previewTime,
                                "预览查询不能更新时间");
                        fs::remove(file);
                        bool missing = false;
                        try
                        {
                            SandboxStore::loadTask(filesDir, path);
                        }
                        catch (const std::exception &)
                        {
                            missing = true;
                        }
                        require(missing, "已删除的预览文件应拒绝恢复");
                    }
                }
                catch (...)
                {
                    failure = std::current_exception();
                }
                prepared.set_value();
                // NAPI 线程池在 Qt 启动时仍存活；退出太早会重置主线程标识，掩盖旧错误。
                releaseFuture.wait();
            });
        preparedFuture.wait();
        try
        {
            if (failure)
                std::rethrow_exception(failure);
            QApplication app(argc, argv);
            require(!wrongMainThread, "沙箱预读抢先初始化了 Qt 主线程");
            QWidget widget;
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(1, 1), Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&widget, &press);
        }
        catch (...)
        {
            release.set_value();
            worker.get();
            throw;
        }
        release.set_value();
        worker.get();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << std::endl;
        fs::remove_all(root);
        return 1;
    }
    fs::remove_all(root);
    std::cout << "Sandbox startup check passed" << std::endl;
    return 0;
}
