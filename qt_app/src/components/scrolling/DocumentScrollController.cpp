/* 文档通用滚动能力：鼠标平滑滚动、触控板速度采样与可选续滑，不创建可视壳层。 */
#include "DocumentScrollController.h"
#include <QApplication>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
// 与当前 Markdown 手感保持一致的速度和减速度（逻辑像素/ms²）。
constexpr qreal touchpadSpeed = 1.0;
constexpr qreal touchpadDeceleration = 0.005;
} // namespace

DocumentScrollController::DocumentScrollController(QScrollBar *bar, QObject *parent)
    : QObject(parent), bar_(bar), scrollAnimation_(this)
{
    scrollAnimation_.setTargetObject(bar_);
    scrollAnimation_.setPropertyName("value");
    inertiaTimer_.setSingleShot(true);
    connect(&inertiaTimer_, &QTimer::timeout, this, &DocumentScrollController::startInertia);
    connect(bar_, &QScrollBar::sliderPressed, this, &DocumentScrollController::stopScrolling);
    connect(bar_, &QScrollBar::actionTriggered, this, &DocumentScrollController::stopScrolling);
}

void DocumentScrollController::setInertiaEnabled(bool enabled)
{
    inertiaEnabled_ = enabled;
    stopScrolling();
}

void DocumentScrollController::stopScrolling()
{
    scrollAnimation_.stop();
    inertiaTimer_.stop();
    velocityClock_.invalidate();
    touchpadVelocity_ = 0;
    velocityDistance_ = 0;
    scrollRemainder_ = 0;
    systemMomentum_ = false;
}

void DocumentScrollController::startInertia()
{
    const qreal velocity = touchpadVelocity_;
    stopScrolling();
    if (!inertiaEnabled_ || std::abs(velocity) < 0.05)
        return;
    auto *bar = bar_;
    const qreal duration = std::min(600.0, std::abs(velocity) / touchpadDeceleration);
    const int start = bar->value();
    const int target = qRound(
        qBound(qreal(bar->minimum()), start + velocity * duration / 2, qreal(bar->maximum())));
    if (start == target)
        return;
    // OutQuad 对应匀减速；接近边界时缩短时长，保持起始速度并在边界停止。
    scrollAnimation_.setDuration(
        std::max(1, qRound(2.0 * std::abs(target - start) / std::abs(velocity))));
    scrollAnimation_.setEasingCurve(QEasingCurve::OutQuad);
    scrollAnimation_.setStartValue(start);
    scrollAnimation_.setEndValue(target);
    scrollAnimation_.start();
}

bool DocumentScrollController::handleWheelEvent(QWheelEvent *event, qreal pixelRatio)
{
#ifndef Q_OS_OPENHARMONY
    Q_UNUSED(pixelRatio);
#endif
    if (event->modifiers() != Qt::NoModifier)
    {
        stopScrolling();
        return false;
    }
    const Qt::ScrollPhase phase = event->phase();
    if (phase == Qt::ScrollBegin)
    {
        stopScrolling();
        velocityClock_.start();
    }
    if (phase == Qt::ScrollMomentum)
    {
        // 系统已经提供续滑，取消待启动或正在执行的自有惯性。
        stopScrolling();
        systemMomentum_ = true;
    }
    auto *bar = bar_;
    if (!event->pixelDelta().isNull())
    {
        if (phase == Qt::NoScrollPhase)
            stopScrolling();
        qreal delta = event->pixelDelta().y();
#ifdef Q_OS_OPENHARMONY
        // 鸿蒙触控板上报物理像素且方向相反，换算成 Qt 逻辑像素；鼠标不取反。
        if (phase != Qt::NoScrollPhase)
            delta = -delta / pixelRatio;
#endif
        scrollAnimation_.stop();
        inertiaTimer_.stop();
        const qreal distance = -delta * (phase == Qt::NoScrollPhase ? 1.0 : touchpadSpeed);
        if ((phase == Qt::ScrollBegin || phase == Qt::ScrollUpdate) && !systemMomentum_ && distance)
        {
            if (!velocityClock_.isValid())
                velocityClock_.start();
            if (distance * touchpadVelocity_ < 0)
            {
                touchpadVelocity_ = 0;
                velocityDistance_ = 0;
                velocityClock_.restart();
            }
            velocityDistance_ += distance;
            const qint64 elapsed = velocityClock_.elapsed();
            if (elapsed >= 8)
            {
                const qreal sample = elapsed < 100 ? velocityDistance_ / elapsed : 0;
                const qreal weight = std::min(1.0, elapsed / 40.0);
                touchpadVelocity_ = sample * touchpadVelocity_ > 0
                                        ? sample * weight + touchpadVelocity_ * (1 - weight)
                                        : sample;
                touchpadVelocity_ = qBound(-4.0, touchpadVelocity_, 4.0);
                velocityDistance_ = 0;
                velocityClock_.restart();
            }
        }
        // 保留缩放后的不足一像素位移，慢速滑动也不会逐次丢失精度。
        scrollRemainder_ += distance;
        const int pixels = qRound(scrollRemainder_);
        scrollRemainder_ -= pixels;
        bar->setValue(bar->value() + pixels);
        if ((distance < 0 && bar->value() == bar->minimum()) ||
            (distance > 0 && bar->value() == bar->maximum()))
            stopScrolling();
    }
    else if (event->angleDelta().y())
    {
        const int delta = qRound(event->angleDelta().y() / 120.0 *
                                 QApplication::wheelScrollLines() * bar->singleStep());
        const int pending = scrollAnimation_.state() == QAbstractAnimation::Running &&
                                    scrollAnimation_.easingCurve().type() == QEasingCurve::OutCubic
                                ? scrollAnimation_.endValue().toInt() - bar->value()
                                : 0;
        const int start = bar->value();
        // 同方向继续累积目标；反向时从当前位置立即转向。
        const int target =
            qBound(bar->minimum(), start + (pending * double(delta) < 0 ? pending : 0) - delta,
                   bar->maximum());
        stopScrolling();
        scrollAnimation_.setDuration(100);
        scrollAnimation_.setEasingCurve(QEasingCurve::OutCubic);
        scrollAnimation_.setStartValue(start);
        scrollAnimation_.setEndValue(target);
        scrollAnimation_.start();
    }
    else if (phase == Qt::NoScrollPhase)
    {
        stopScrolling();
        event->ignore();
        return false;
    }
    // ScrollEnd 通常没有位移；短暂等待系统惯性事件，避免重复续滑。
    if (inertiaEnabled_ && phase == Qt::ScrollEnd && !systemMomentum_ && velocityClock_.isValid() &&
        velocityClock_.elapsed() < 80 && std::abs(touchpadVelocity_) >= 0.05)
        inertiaTimer_.start(32);
    event->accept();
    return true;
}
