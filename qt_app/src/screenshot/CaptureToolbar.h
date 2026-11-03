/* 汇文截图工具栏：形状、文字、颜色、字号与完成／取消操作。 */
#ifndef HUIWEN_CAPTURE_TOOLBAR_H
#define HUIWEN_CAPTURE_TOOLBAR_H

#include <QWidget>
#include <memory>

namespace Ui
{
class CaptureToolbar;
}

const QString vecColor[] = {"#000000", "#80807f", "#800000", "#807f01", "#008100",
                            "#000082", "#800080", "#ffffff", "#c0c0c0", "#fe0002",
                            "#ffff02", "#12fc0b", "#0000ff", "#fe02ff"};
const QString vecFont[] = {"9", "10", "11", "12", "14", "16", "18", "20"};

// 字号选择弹窗：替代 QMenu，自绘直角背景以绕过鸿蒙系统 popup 窗口强制圆角
class FontPopup : public QWidget
{
    Q_OBJECT
  public:
    explicit FontPopup(QWidget *parent = nullptr);
    void showAt(const QPoint &globalPos, int width);
  signals:
    void fontSelected(int idx);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void hideEvent(QHideEvent *) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

  private:
    int indexAt(const QPoint &pos) const;
    static constexpr int kItemHeight = 32;
    int hoveredIdx_ = -1;
    int pressedIdx_ = -1;
};

class QAbstractButton;
class QEvent;
class CaptureToolbar : public QWidget
{
    Q_OBJECT

  public:
    enum CaptureEditState
    {
        CES_NULL = 0,
        CES_RECTANGLE,
        CES_ELLIPSE,
        CES_ARROW,
        CES_TEXT,
        CES_UNDO,
    };
    explicit CaptureToolbar(QWidget *parent = nullptr);
    ~CaptureToolbar();

    void setClipRect(const QRect &rc);
    // 复用 overlay 开始新一次截图时复位工具勾选/字体弹窗（避免上一轮状态残留）
    void resetTools();

  signals:
    void saveScreen();
    void quitScreen();
    void finishScreen();
    void changeEditState(CaptureToolbar::CaptureEditState);
    void changeColor(int);
    void changeFont(int);
  public slots:
    void onPostionChange(int x, int y);
    void onSizeChange(int w, int h);
    void onUndo();
    void onRectangle(int state);
    void onEllipse(int state);
    void onArrow(int state);
    void onText(int state);
    void onChangeColor(int colorIdx);
    void onClickFont();
    void onFontSelected(int idx);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    QPoint CalculatePos(const QRect &rc) const;
    void changeHeight(bool bShowColor);
    // 事件过滤子处理器：将 eventFilter 的大块逻辑拆分，每个约 50 行
    bool onToolPaint();                           // widTool 绘制（工具图标精灵）
    bool onToolButtonRelease(QEvent *event);      // widTool 鼠标释放（命中按钮 -> click）
    bool onColorPaint();                          // widToolColor 绘制（颜色条）
    bool onToolMouseMove(QEvent *event);          // widTool 鼠标移动（悬停高亮 + 提示）
    QList<QAbstractButton *> toolButtons() const; // 工具按钮列表（复用）
  private:
    Ui::CaptureToolbar *ui;
    QRect m_rcClip;
    std::shared_ptr<FontPopup> m_fontPopup;
    // 当前悬停的工具按钮（按钮已 hide，underMouse() 不可用，由 MouseMove 维护）
    QAbstractButton *m_hoveredButton = nullptr;
};

#endif // HUIWEN_CAPTURE_TOOLBAR_H
