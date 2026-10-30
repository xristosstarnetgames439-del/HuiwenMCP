/* 无 QtTest 依赖的桌面检查：真实 PDF 加载、定位、通用区域切换与按需启用能力。 */
#include "documentView/DocumentView.h"
#include "documentView/renderers/pdf/PdfPreviewWidget.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThread>
#include <cstdlib>
#include <functional>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        std::exit(1);
    }
}

void waitFor(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    do
    {
        QApplication::processEvents();
        if (condition())
            return;
        QThread::msleep(10);
    } while (timer.elapsed() < 10000);
    require(false, "等待预览结果超时");
}

void writePdf(const QString &path, const QByteArray &firstColor)
{
    const QByteArray first = firstColor + " rg 0 0 200 300 re f\n";
    const QByteArray second = "0 0 1 rg 0 0 200 300 re f\n";
    const QList<QByteArray> objects = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 300] /Resources << >> /Contents 4 0 R >>",
        "<< /Length " + QByteArray::number(first.size()) + " >>\nstream\n" + first + "endstream",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 300] /Resources << >> /Contents 6 0 R >>",
        "<< /Length " + QByteArray::number(second.size()) + " >>\nstream\n" + second + "endstream"};
    QByteArray pdf = "%PDF-1.4\n";
    QList<int> offsets;
    for (int i = 0; i < objects.size(); ++i)
    {
        offsets.append(pdf.size());
        pdf += QByteArray::number(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    const int xref = pdf.size();
    pdf += "xref\n0 7\n0000000000 65535 f \n";
    for (int offset : offsets)
        pdf += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    pdf +=
        "trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "无法创建 PDF 样本");
    require(file.write(pdf) == pdf.size(), "PDF 样本写入失败");
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "临时目录创建失败");
    const QString first = directory.filePath(QStringLiteral("中文红色.pdf"));
    const QString second = directory.filePath(QStringLiteral("绿色.pdf"));
    writePdf(first, "1 0 0");
    writePdf(second, "0 1 0");
    auto *view = new PdfPreviewWidget;
    DocumentView source(view, QStringLiteral("请选择 PDF"), true);
    source.resize(480, 320);
    source.show();
    auto *card = source.findChild<QPushButton *>();
    require(card && card->isEnabled() && !source.hasDocument(), "空输入区域应可选择文件");
    int selections = 0;
    QObject::connect(&source, &DocumentView::cardActivated, [&] { ++selections; });
    source.setDocument(first, QStringLiteral("pdf"));
    require(card->isVisible() && !view->isVisible(), "占位图模式应显示文件卡片");
    source.setBusy(true);
    card->click();
    require(!card->isEnabled() && selections == 0, "忙碌时不能触发文件选择");
    source.setBusy(false);
    card->click();
    require(selections == 1, "卡片点击应交给调用方处理");
    source.setPreviewMode(true);
    waitFor([&] { return source.isPreviewReady(); });
    const int oldRange = view->verticalScrollBar()->maximum();
    require(oldRange > 0, "多页 PDF 应可滚动");
    source.resize(720, 320);
    waitFor([&]
            { return view->verticalScrollBar()->maximum() > oldRange && source.isPreviewReady(); });
    require(view->verticalScrollBar()->singleStep() == 40, "PDF 滚轮步长应使用文档阅读尺度");
    int reports = 0;
    int page = 0;
    double fraction = 0;
    QObject::connect(&source, &DocumentView::scrollPositionChanged,
                     [&](int current, double part, double)
                     {
                         ++reports;
                         page = current;
                         fraction = part;
                     });
    source.setScrollPosition(2, 0.25, 0.5);
    require(reports == 0, "同步带动的 PDF 滚动不能立即反向回传");
    source.publishScrollPosition();
    require(page == 2 && qAbs(fraction - 0.25) < 0.01, "PDF 应按页码和页内比例定位");
    source.setScrollPosition(0, 0, 0.5);
    require(qAbs(view->verticalScrollBar()->value() - view->verticalScrollBar()->maximum() * 0.5) <=
                1,
            "无页码时应按总进度定位 PDF");
    source.setViewOptions({false, false});
    require(source.scrollOwner() == DocumentScrollOwner::View &&
                view->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
            "Qt 渲染器应支持按需关闭自有惯性和滚动条");
    source.setViewOptions({true, true});
    require(view->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded, "滚动条应能恢复显示");
    source.setPreviewMode(false);
    require(card->isVisible() && !source.isPreviewReady(), "占位图模式不能转发预览滚动");
    source.setPreviewMode(true);
    source.setDocument(second, QStringLiteral("pdf"));
    waitFor([&] { return source.isPreviewReady(); });
    require(view->verticalScrollBar()->value() == 0, "更换文件后应从第一页开始");
    source.setDocument({}, QStringLiteral("pdf"));
    require(!source.hasDocument() && card->isVisible() && !source.isPreviewReady(),
            "清空文档应恢复空状态");
    DocumentView result(new PdfPreviewWidget, QStringLiteral("待生成文件"), false);
    require(!result.findChild<QPushButton *>()->isEnabled(),
            "同一个 View 应支持不可点击的空结果状态");
    result.setDocument(first, QStringLiteral("pdf"));
    require(result.hasDocument() && result.findChild<QPushButton *>()->isEnabled(),
            "同一个 View 应能承载不同角色和文档渲染器");
    std::cout << "PDF_PREVIEW_CHECK_OK" << std::endl;
}
