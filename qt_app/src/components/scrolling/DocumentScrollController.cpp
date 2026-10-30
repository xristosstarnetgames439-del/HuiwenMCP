/* 文档通用滚动能力：鼠标平滑滚动、触控板速度采样与可选续滑，不创建可视壳层。 */
#include "DocumentScrollController.h"
#include <QApplication>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
// 跟手阶段沿用逻辑像素位移，不随文档总高度放大。
constexpr qreal touchpadSpeed = 2.0;
// 参考鸿蒙 Chromium v114.14.9 的 FlingSpringCurveOhos，惯性初速度乘以 2.25。
// 其初始偏移为 -v0 / 5.04，临界阻尼公式可化简为 v(t) = v0 * exp(-5.04 * t)。
constexpr qreal flingVelocityMultiplier = 1.5 * 1.5;
constexpr qreal flingFriction = 4.2 * 1.2;
} // namespace

DocumentScrollController::DocumentScrollController(QScrollBar *bar, QObject *parent)
    : QObject(parent), bar_(bar), scrollAnimation_(this)
{
    scrollAnimation_.setTargetObject(bar_);
    scrollAnimation_.setPropertyName("value");
    inertiaTimer_.setSingleShot(true);
    inertiaFrameTimer_.setInterval(16);
    inertiaFrameTimer_.setTimerType(Qt::PreciseTimer);
    connect(&inertiaTimer_, &QTimer::timeout, this, &DocumentScrollController::startInertia);
    connect(&inertiaFrameTimer_, &QTimer::timeout, this, &DocumentScrollController::advanceInertia);
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
    inertiaFrameTimer_.stop();
    inertiaClock_.invalidate();
    velocityClock_.invalidate();
    velocityTimestamp_ = 0;
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
    touchpadVelocity_ = velocity * flingVelocityMultiplier;
    inertiaClock_.start();
    inertiaFrameTimer_.start();
}

void DocumentScrollController::advanceInertia()
{
    // 按真实帧间隔积分；单位从逻辑像素/ms 换算为逻辑像素/s，不受文档长度影响。
    const qreal elapsed = inertiaClock_.restart() / 1000.0;
    const qreal decay = -std::expm1(-flingFriction * elapsed);
    const qreal distance = touchpadVelocity_ * 1000 * decay / flingFriction;
    const qreal direction = std::copysign(1.0, touchpadVelocity_);
    touchpadVelocity_ *= 1 - decay;
    scrollRemainder_ += distance;
    const int pixels = qRound(scrollRemainder_);
    scrollRemainder_ -= pixels;
    bar_->setValue(bar_->value() + pixels);
    // 不照搬原曲线在 1% 振幅处补齐终点的行为；余程不足半像素再停止，避免尾段跳动。
    if (std::abs(touchpadVelocity_) * 1000 / flingFriction < 0.5 ||
        (direction < 0 && bar_->value() == bar_->minimum()) ||
        (direction > 0 && bar_->value() == bar_->maximum()))
        stopScrolling();
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
    if (inertiaFrameTimer_.isActive())
        stopScrolling();
    const Qt::ScrollPhase phase = event->phase();
    if (phase == Qt::ScrollBegin)
    {
        stopScrolling();
        velocityClock_.start();
        velocityTimestamp_ = event->timestamp();
    }
    // 优先使用输入发生时间，避免把 PDF 绘制或事件排队耗时算成手指移动速度。
    const auto sampleElapsed = [&]() -> qint64
    {
        if (event->timestamp() && velocityTimestamp_ && event->timestamp() >= velocityTimestamp_)
            return event->timestamp() - velocityTimestamp_;
        return velocityClock_.isValid() ? velocityClock_.elapsed() : 0;
    };
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
            {
                velocityClock_.start();
                velocityTimestamp_ = event->timestamp();
            }
            if (distance * touchpadVelocity_ < 0)
            {
                touchpadVelocity_ = 0;
                velocityDistance_ = 0;
                velocityClock_.restart();
                velocityTimestamp_ = event->timestamp();
            }
            velocityDistance_ += distance;
            const qint64 elapsed = sampleElapsed();
            if (elapsed >= 8)
            {
                const qreal sample = elapsed < 100 ? velocityDistance_ / elapsed : 0;
                const qreal weight = std::min(1.0, elapsed / 40.0);
                touchpadVelocity_ = sample * touchpadVelocity_ > 0
                                        ? sample * weight + touchpadVelocity_ * (1 - weight)
                                        : sample;
                touchpadVelocity_ =
                    qBound(-4.0 * touchpadSpeed, touchpadVelocity_, 4.0 * touchpadSpeed);
                velocityDistance_ = 0;
                velocityClock_.restart();
                velocityTimestamp_ = event->timestamp();
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
        sampleElapsed() < 80 && std::abs(touchpadVelocity_) >= 0.05)
        inertiaTimer_.start(32);
    event->accept();
    return true;
}
