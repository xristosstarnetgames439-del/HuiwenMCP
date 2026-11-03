/* 汇文截图放大取色器：展示鼠标附近像素、选区尺寸和 RGB。 */
#ifndef HUIWEN_CAPTURE_AMPLIFIER_H
#define HUIWEN_CAPTURE_AMPLIFIER_H

#include <QWidget>
#include <memory>

/**
 * @brief 汇文截图放大取色器
 * @note  鼠标位置局部放大 + 色卡取值；作为 CaptureOverlay 的子控件
 */
class CaptureAmplifier : public QWidget
{
    Q_OBJECT
  public:
    explicit CaptureAmplifier(std::shared_ptr<QPixmap> originPainting, QWidget *parent = nullptr);

  public slots:
    void onSizeChange(int w, int h);
    void onPostionChange(int x, int y);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QSize screenSize_;
    QPoint cursorPoint_;
    int sideLength_;
    int imageHeight_;
    std::shared_ptr<QPixmap> originPainting_;
};

#endif // HUIWEN_CAPTURE_AMPLIFIER_H
