/* 无 QtTest 依赖的桌面检查：真实 PDF 加载、定位、通用区域切换与按需启用能力。 */
#include "documentView/DocumentView.h"
#include "documentView/renderers/pdf/PdfPreviewWidget.h"
#include "components/rendering/RasterRenderWindow.h"
#include "components/scrolling/DocumentScrollController.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QScroller>
#include <QTemporaryDir>
#include <QThread>
#include <QTouchEvent>
#include <QWheelEvent>
#include <cstdlib>
#include <functional>
#include <iostream>

namespace
{
// 通过真实原生绘图窗口注入事件，检查转发链，而不是直接调用滚动控制器。
class PdfPreviewProbe : public PdfPreviewWidget
{
  public:
    RasterRenderWindow *renderSurface() const { return renderWindow_; }
};

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

void checkTouchpadInertia()
{
    QScrollBar bar;
    bar.setRange(0, 100000);
    bar.setValue(50000);
    DocumentScrollController controller(&bar);
    const auto wheel = [&](Qt::ScrollPhase phase, int pixels, ulong timestamp)
    {
        QWheelEvent event(QPointF(12, 12), QPointF(12, 12), QPoint(0, pixels), {}, Qt::NoButton,
                          Qt::NoModifier, phase, false);
        event.setTimestamp(timestamp);
        controller.handleWheelEvent(&event, 1.0);
    };
    // 无需真实等待输入采样间隔：速度必须由事件时间戳得出，而非测试执行速度。
    wheel(Qt::ScrollBegin, 0, 1000);
    wheel(Qt::ScrollUpdate, -64, 1016);
    const int released = bar.value();
    const int direction = released > 50000 ? 1 : -1;
    wheel(Qt::ScrollEnd, 0, 1016);
    QElapsedTimer clock;
    clock.start();
    waitFor([&] { return clock.elapsed() >= 650; });
    const int duringTail = bar.value();
    waitFor([&] { return clock.elapsed() >= 2000; });
    require(direction * (duringTail - released) > 0, "松手后应沿原方向续滑");
    require(direction * (bar.value() - duringTail) > 0, "快速滑动的尾段不能在 600ms 被截断");
    require(qAbs(qAbs(bar.value() - released) - 3571) <= 2,
            "速率乘 2 后经鸿蒙 2.25 倍惯性增益应续滑约 3571px，不随文档总高度放大");
    const int stopped = bar.value();
    waitFor([&] { return clock.elapsed() >= 2100; });
    require(bar.value() == stopped, "尾段收敛后应停止续滑");
}

void checkTouchscreen(PdfPreviewProbe *view)
{
    QTouchDevice device;
    device.setType(QTouchDevice::TouchScreen);
    device.setCapabilities(QTouchDevice::Position);
    const auto touch = [&](QEvent::Type type, qreal y, ulong timestamp)
    {
        const auto state = type == QEvent::TouchBegin ? Qt::TouchPointPressed
                           : type == QEvent::TouchEnd ? Qt::TouchPointReleased
                                                      : Qt::TouchPointMoved;
        QTouchEvent::TouchPoint point(0);
        point.setState(state);
        point.setPos(QPointF(80, y));
        point.setScreenPos(view->renderSurface()->mapToGlobal(point.pos().toPoint()));
        QTouchEvent event(type, &device, Qt::NoModifier, state, {point});
        event.setTimestamp(timestamp);
        QApplication::sendEvent(view->renderSurface(), &event);
        require(event.isAccepted(), "原生绘图窗口必须转发并接收触屏事件");
    };
    view->setScrollPosition(0, 0, 0.25);
    const int before = view->verticalScrollBar()->value();
    touch(QEvent::TouchBegin, 260, 1000);
    touch(QEvent::TouchUpdate, 210, 1016);
    touch(QEvent::TouchUpdate, 160, 1032);
    waitFor([&] { return view->verticalScrollBar()->value() > before; });
    touch(QEvent::TouchUpdate, 110, 1048);
    touch(QEvent::TouchEnd, 60, 1064);
    auto *scroller = QScroller::scroller(view->viewport());
    require(scroller->state() == QScroller::Scrolling, "触屏松手后应进入惯性滚动");
    const int released = view->verticalScrollBar()->value();
    waitFor([&] { return view->verticalScrollBar()->value() > released; });
    touch(QEvent::TouchCancel, 60, 1080);
    require(scroller->state() == QScroller::Inactive, "取消触摸必须停止触屏惯性");
    touch(QEvent::TouchBegin, 200, 1100);
    touch(QEvent::TouchUpdate, 150, 1116);
    QWheelEvent wheel(QPointF(12, 12), QPointF(12, 12), {}, {}, Qt::NoButton, Qt::NoModifier,
                      Qt::ScrollBegin, false);
    QApplication::sendEvent(view->renderSurface(), &wheel);
    require(scroller->state() == QScroller::Inactive, "触控板输入必须结束旧触屏滚动");
    view->setScrollPosition(0, 0, 0);
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
    checkTouchpadInertia();
    QTemporaryDir directory;
    require(directory.isValid(), "临时目录创建失败");
    const QString first = directory.filePath(QStringLiteral("中文红色.pdf"));
    const QString second = directory.filePath(QStringLiteral("绿色.pdf"));
    writePdf(first, "1 0 0");
    writePdf(second, "0 1 0");
    auto *view = new PdfPreviewProbe;
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
    checkTouchscreen(view);
    source.setScrollPosition(0, 0, 0.5);
    const int beforeGesture = view->verticalScrollBar()->value();
    QWheelEvent begin(QPointF(12, 12), QPointF(12, 12), {}, {}, Qt::NoButton, Qt::NoModifier,
                      Qt::ScrollBegin, false);
    QApplication::sendEvent(view->renderSurface(), &begin);
    QWheelEvent update(QPointF(12, 12), QPointF(12, 12), QPoint(0, -40), {}, Qt::NoButton,
                       Qt::NoModifier, Qt::ScrollUpdate, false);
    QApplication::sendEvent(view->renderSurface(), &update);
    require(view->verticalScrollBar()->value() != beforeGesture,
            "原生绘图窗口收到的触控板事件必须驱动 PDF 滚动");
    QWheelEvent end(QPointF(12, 12), QPointF(12, 12), {}, {}, Qt::NoButton, Qt::NoModifier,
                    Qt::ScrollEnd, false);
    QApplication::sendEvent(view->renderSurface(), &end);
    source.setScrollPosition(0, 0, 0.25);
    const int beforeKey = view->verticalScrollBar()->value();
    QKeyEvent pageDown(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier);
    QApplication::sendEvent(view->renderSurface(), &pageDown);
    require(view->verticalScrollBar()->value() > beforeKey, "改用 viewport 转发后仍应支持键盘翻页");
    source.setScrollPosition(0, 0, 0);
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
