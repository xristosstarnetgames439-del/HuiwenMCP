/* 可折叠区块外壳：只管理标题与内容显隐，并从标题栏发出长按拖动事件。 */
#pragma once
#include <QPoint>
#include <QTimer>
#include <QWidget>
class QToolButton;
class QPixmap;
class QGraphicsOpacityEffect;
class QFrame;

class SidebarSection : public QWidget
{
    Q_OBJECT
  public:
    SidebarSection(const QString &title, QWidget *content, QWidget *parent = nullptr);
    bool isExpanded() const
    {
        return expanded_;
    }
    bool isAvailable() const
    {
        return available_;
    }
    void setExpanded(bool expanded);
    void setAvailable(bool available);
    QRect titleBarRect() const;
    QPixmap titleBarPixmap() const;

  signals:
    void stateChanged();
    void dragStarted(const QPoint &globalPosition);
    void dragMoved(const QPoint &globalPosition);
    void dragFinished(bool cancelled);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void finishDrag(bool cancelled);
    QFrame *headerRow_;
    QToolButton *header_;
    QWidget *content_;
    QGraphicsOpacityEffect *headerOpacity_;
    QTimer holdTimer_;
    QPoint pressPosition_;
    QPoint pointerPosition_;
    bool expanded_ = false;
    bool available_ = true;
    bool pressed_ = false;
    bool dragging_ = false;
};
