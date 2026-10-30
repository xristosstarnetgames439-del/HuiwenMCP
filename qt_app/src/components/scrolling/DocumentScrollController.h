/* 可挂接到 Qt 滚动条的滚动能力；CEF 等自行管理滚动的后端不接入本对象。 */
#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QPropertyAnimation>
#include <QTimer>

class QScrollBar;
class QWheelEvent;

class DocumentScrollController final : public QObject
{
  public:
    explicit DocumentScrollController(QScrollBar *bar, QObject *parent = nullptr);
    bool handleWheelEvent(QWheelEvent *event, qreal pixelRatio);
    void setInertiaEnabled(bool enabled);
    void stopScrolling();

  private:
    void startInertia();
    QScrollBar *bar_;
    QPropertyAnimation scrollAnimation_;
    QTimer inertiaTimer_;
    QElapsedTimer velocityClock_;
    qreal touchpadVelocity_ = 0;
    qreal velocityDistance_ = 0;
    qreal scrollRemainder_ = 0;
    bool systemMomentum_ = false;
    bool inertiaEnabled_ = true;
};
