/* 汇文截图放大取色器：原图像素网格、HEX／RGB 色值，不显示坐标与尺寸。 */
#ifndef HUIWEN_CAPTURE_AMPLIFIER_H
#define HUIWEN_CAPTURE_AMPLIFIER_H

#include <QImage>
#include <QWidget>

class QPixmap;

/**
 * @brief 汇文截图放大取色器
 * @note  鼠标位置局部放大 + 色卡取值；作为 CaptureOverlay 的子控件
 */
class CaptureAmplifier : public QWidget
{
    Q_OBJECT
  public:
    explicit CaptureAmplifier(const QPixmap &originPainting, QWidget *parent = nullptr);
    QString colorText() const;
    // 历史色块始终使用 HEX；复制文本仍保留用户当前选择的 HEX／RGB 格式。
    QString colorHex() const;
    void toggleColorFormat();

  public slots:
    void onPostionChange(const QPointF &point);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QImage originImage_;
    QPoint sourcePixel_;
    bool hexFormat_ = true;
};

#endif // HUIWEN_CAPTURE_AMPLIFIER_H
