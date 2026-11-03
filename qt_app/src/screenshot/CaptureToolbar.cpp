/* 汇文截图工具栏：形状、文字、颜色、字号与完成／取消操作。 */
#include "CaptureToolbar.h"
#include "ui_CaptureToolbar.h"
#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QDesktopWidget>
#include <QEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

// ---- FontPopup：自绘直角字号选择弹窗（替代 QMenu，绕过鸿蒙 popup 系统圆角）----
FontPopup::FontPopup(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
}

void FontPopup::showAt(const QPoint &globalPos, int width)
{
    const int itemCount = static_cast<int>(sizeof(vecFont) / sizeof(vecFont[0]));
    resize(width, itemCount * kItemHeight);
    move(globalPos);
    qApp->installEventFilter(this);
    show();
    raise();
}

int FontPopup::indexAt(const QPoint &pos) const
{
    if (pos.x() < 0 || pos.x() >= width() || pos.y() < 0)
        return -1;
    const int idx = pos.y() / kItemHeight;
    const int count = static_cast<int>(sizeof(vecFont) / sizeof(vecFont[0]));
    return idx < count ? idx : -1;
}

void FontPopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(0xba, 0xba, 0xbc));
    const int count = static_cast<int>(sizeof(vecFont) / sizeof(vecFont[0]));
    for (int i = 0; i < count; ++i)
    {
        const QRect itemRect(0, i * kItemHeight, width(), kItemHeight);
        if (i == pressedIdx_ || i == hoveredIdx_)
            p.fillRect(itemRect, QColor(0x27, 0x86, 0xff));
        p.setPen(Qt::white);
        p.drawText(itemRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   QString("%1pt").arg(vecFont[i]));
    }
}

void FontPopup::mouseMoveEvent(QMouseEvent *e)
{
    const int idx = indexAt(e->pos());
    if (idx != hoveredIdx_)
    {
        hoveredIdx_ = idx;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void FontPopup::mousePressEvent(QMouseEvent *e)
{
    const int idx = indexAt(e->pos());
    if (idx >= 0)
    {
        emit fontSelected(idx);
        hide();
    }
    QWidget::mousePressEvent(e);
}

void FontPopup::mouseReleaseEvent(QMouseEvent *e)
{
    QWidget::mouseReleaseEvent(e);
}

void FontPopup::hideEvent(QHideEvent *e)
{
    qApp->removeEventFilter(this);
    hoveredIdx_ = -1;
    QWidget::hideEvent(e);
}

bool FontPopup::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress)
    {
        if (obj != this && !isAncestorOf(qobject_cast<QWidget *>(obj)))
            hide();
    }
    return QWidget::eventFilter(obj, e);
}
// ---- FontPopup end ----

namespace
{
constexpr int kToolbarScale = 2;

// 编辑按钮提示文本（按钮已隐藏，setToolTip 不生效，由悬停时 QToolTip::showText 显示）
QString ToolButtonTipText(QAbstractButton *button, Ui::CaptureToolbar *ui)
{
    if (button == ui->chkRectangle)
        return QStringLiteral("矩形工具");
    if (button == ui->chkEllipse)
        return QStringLiteral("椭圆工具");
    if (button == ui->chkArrow)
        return QStringLiteral("箭头工具");
    if (button == ui->chkText)
        return QStringLiteral("文字工具");
    if (button == ui->btnUndo)
        return QStringLiteral("撤销");
    if (button == ui->btnSave)
        return QStringLiteral("保存");
    if (button == ui->btnQuit)
        return QStringLiteral("退出");
    if (button == ui->btnFinish)
        return QStringLiteral("完成");
    return QString();
}

void ScaleChildren(QWidget *parent)
{
    const auto children = parent->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children)
    {
        const QRect geometry = child->geometry();
        child->setMinimumSize(0, 0);
        child->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        child->setGeometry(geometry.x() * kToolbarScale, geometry.y() * kToolbarScale,
                           geometry.width() * kToolbarScale, geometry.height() * kToolbarScale);
        QString style = child->styleSheet();
        style.replace("20px", "40px");
        style.replace("8px", "16px");
        child->setStyleSheet(style);
    }
}

} // namespace

CaptureToolbar::CaptureToolbar(QWidget *parent) : QWidget(parent), ui(new Ui::CaptureToolbar)
{
    ui->setupUi(this);
    ScaleChildren(ui->widTool);
    ScaleChildren(ui->widToolColor);
    ui->widTool->setGeometry(0, 0, 306 * kToolbarScale, 28 * kToolbarScale);
    ui->widToolColor->setGeometry(0, 28 * kToolbarScale, 306 * kToolbarScale, 28 * kToolbarScale);
    ui->widTool->setStyleSheet(QStringLiteral(
        "#widTool{background:transparent;background-image:none;border-image:none;}"));
    ui->widToolColor->setStyleSheet(QStringLiteral(
        "#widToolColor{background:transparent;background-image:none;border-image:none;}"
        "QRadioButton::indicator{width:16px;height:16px;border:0;"
        "background:transparent;background-image:none;border-image:none;}"));
    ui->widTool->installEventFilter(this);
    ui->widToolColor->installEventFilter(this);
    // 按钮已隐藏并手动绘制，需开启鼠标跟踪以在 MouseMove 中维护悬停状态
    ui->widTool->setMouseTracking(true);

    const QList<QAbstractButton *> toolButtons = {ui->chkRectangle, ui->chkEllipse, ui->chkArrow,
                                                  ui->chkText,      ui->btnUndo,    ui->btnSave,
                                                  ui->btnQuit,      ui->btnFinish};
    for (QAbstractButton *button : toolButtons)
    {
        button->hide();
        button->setFocusPolicy(Qt::NoFocus);
    }
    connect(ui->btnFinish, SIGNAL(clicked()), this, SIGNAL(finishScreen()));
    connect(ui->btnQuit, SIGNAL(clicked()), this, SIGNAL(quitScreen()));
    connect(ui->btnSave, SIGNAL(clicked()), this, SIGNAL(saveScreen()));
    connect(ui->btnUndo, SIGNAL(clicked()), this, SLOT(onUndo()));
    connect(ui->chkRectangle, SIGNAL(stateChanged(int)), this, SLOT(onRectangle(int)));
    connect(ui->chkEllipse, SIGNAL(stateChanged(int)), this, SLOT(onEllipse(int)));
    connect(ui->chkArrow, SIGNAL(stateChanged(int)), this, SLOT(onArrow(int)));
    connect(ui->chkText, SIGNAL(stateChanged(int)), this, SLOT(onText(int)));
    QButtonGroup *btnGroup = new QButtonGroup(this);
    btnGroup->addButton(ui->radColor1, 0);
    btnGroup->addButton(ui->radColor2, 1);
    btnGroup->addButton(ui->radColor3, 2);
    btnGroup->addButton(ui->radColor4, 3);
    btnGroup->addButton(ui->radColor5, 4);
    btnGroup->addButton(ui->radColor6, 5);
    btnGroup->addButton(ui->radColor7, 6);
    btnGroup->addButton(ui->radColor8, 7);
    btnGroup->addButton(ui->radColor9, 8);
    btnGroup->addButton(ui->radColor10, 9);
    btnGroup->addButton(ui->radColor11, 10);
    btnGroup->addButton(ui->radColor12, 11);
    btnGroup->addButton(ui->radColor13, 12);
    btnGroup->addButton(ui->radColor14, 13);
    connect(btnGroup, SIGNAL(buttonClicked(int)), this, SLOT(onChangeColor(int)));
    // 选色窗口（颜色/字体按钮）：悬停显示手型光标（Qt 原版同款交互）
    const auto colorButtons = ui->widToolColor->findChildren<QAbstractButton *>();
    for (QAbstractButton *button : colorButtons)
    {
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
    }
    ui->labColor->setStyleSheet(QString("background-color:%1;").arg(vecColor[9]));
    ui->btnFont->setText(QString("%1pt").arg(vecFont[3]));
    ui->btnFont->setFocusPolicy(Qt::NoFocus);
    connect(ui->btnFont, SIGNAL(clicked()), this, SLOT(onClickFont()));
    changeHeight(false);
    hide();
}

CaptureToolbar::~CaptureToolbar()
{
    delete ui;
}

void CaptureToolbar::setClipRect(const QRect &rc)
{
    m_rcClip = rc;
    move(CalculatePos(m_rcClip));
}

void CaptureToolbar::onPostionChange(int x, int y)
{
    m_rcClip.moveTo(x, y);
    move(CalculatePos(m_rcClip));
}

void CaptureToolbar::onSizeChange(int w, int h)
{
    m_rcClip.setSize(QSize(w, h));
    move(CalculatePos(m_rcClip));
}

void CaptureToolbar::onUndo()
{
    emit changeEditState(CES_UNDO);
}

void CaptureToolbar::onRectangle(int state)
{
    if (state == Qt::Checked)
    {
        ui->chkEllipse->setCheckState(Qt::Unchecked);
        ui->chkArrow->setCheckState(Qt::Unchecked);
        ui->chkText->setCheckState(Qt::Unchecked);
        emit changeEditState(CES_RECTANGLE);
    }
    else
    {
        emit changeEditState(CES_NULL);
    }
    changeHeight(state == Qt::Checked);
}

void CaptureToolbar::onEllipse(int state)
{
    if (state == Qt::Checked)
    {
        ui->chkRectangle->setCheckState(Qt::Unchecked);
        ui->chkArrow->setCheckState(Qt::Unchecked);
        ui->chkText->setCheckState(Qt::Unchecked);
        emit changeEditState(CES_ELLIPSE);
    }
    else
    {
        emit changeEditState(CES_NULL);
    }
    changeHeight(state == Qt::Checked);
}

void CaptureToolbar::onArrow(int state)
{
    if (state == Qt::Checked)
    {
        ui->chkRectangle->setCheckState(Qt::Unchecked);
        ui->chkEllipse->setCheckState(Qt::Unchecked);
        ui->chkText->setCheckState(Qt::Unchecked);
        emit changeEditState(CES_ARROW);
    }
    else
    {
        emit changeEditState(CES_NULL);
    }
    changeHeight(state == Qt::Checked);
}

// 复用 overlay 开始新一次截图时复位工具状态：全部取消勾选、收起字体弹窗。
void CaptureToolbar::resetTools()
{
    ui->chkRectangle->setCheckState(Qt::Unchecked);
    ui->chkEllipse->setCheckState(Qt::Unchecked);
    ui->chkArrow->setCheckState(Qt::Unchecked);
    ui->chkText->setCheckState(Qt::Unchecked);
    if (m_fontPopup)
    {
        m_fontPopup->hide();
    }
    changeHeight(false);
}

void CaptureToolbar::onText(int state)
{
    if (state == Qt::Checked)
    {
        ui->chkRectangle->setCheckState(Qt::Unchecked);
        ui->chkEllipse->setCheckState(Qt::Unchecked);
        ui->chkArrow->setCheckState(Qt::Unchecked);
        emit changeEditState(CES_TEXT);
    }
    else
    {
        emit changeEditState(CES_NULL);
    }
    changeHeight(state == Qt::Checked);
}

void CaptureToolbar::onChangeColor(int colorIdx)
{
    ui->labColor->setStyleSheet(QString("background-color:%1;").arg(vecColor[colorIdx]));
    emit changeColor(colorIdx);
}

void CaptureToolbar::onClickFont()
{
    if (m_fontPopup.get() == nullptr)
    {
        FontPopup *popup = new FontPopup(this);
        connect(popup, &FontPopup::fontSelected, this, &CaptureToolbar::onFontSelected);
        m_fontPopup.reset(popup);
    }
    const QPoint pos = ui->btnFont->mapToGlobal(ui->btnFont->rect().bottomLeft());
    m_fontPopup->showAt(pos, ui->btnFont->width());
}

void CaptureToolbar::onFontSelected(int fontIdx)
{
    ui->btnFont->setText(QString("%1pt").arg(vecFont[fontIdx]));
    emit changeFont(fontIdx);
}

QPoint CaptureToolbar::CalculatePos(const QRect &rc) const
{
    const int toolBarFloatingGap = 5 * kToolbarScale;
    int cyScreen = QApplication::desktop()->height();

    QPoint pos;
    if ((rc.bottom() + height() + toolBarFloatingGap) <= cyScreen)
    {
        // 选择框的下方可以浮动
        pos.setX(rc.right() - width());
        pos.setY(rc.bottom() + toolBarFloatingGap);
    }
    else if ((rc.top() - height() - toolBarFloatingGap) >= 0)
    {
        // 下方不够地方了，上方有空可以浮动
        pos.setX(rc.right() - width());
        pos.setY(rc.top() - height() - toolBarFloatingGap);
    }
    else
    {
        // 实在没地方了，只能放到框里面了,贴到右下角去，同时支持用户自己拖动浮动工具条
        pos.setX(rc.right() - width() - 5);
        pos.setY(rc.bottom() - height() - 5);
    }

    return pos;
}

void CaptureToolbar::changeHeight(bool bShowColor)
{
    setFixedSize(306 * kToolbarScale, (bShowColor ? 56 : 28) * kToolbarScale);
    move(CalculatePos(m_rcClip));
}

QList<QAbstractButton *> CaptureToolbar::toolButtons() const
{
    return {ui->chkRectangle, ui->chkEllipse, ui->chkArrow, ui->chkText,
            ui->btnUndo,      ui->btnSave,    ui->btnQuit,  ui->btnFinish};
}

// widTool 绘制：从工具图标精灵图中按按钮状态绘制 normal/hover 图标
bool CaptureToolbar::onToolPaint()
{
    static const QImage normal(QStringLiteral(":/huiwen/screenshot/toolbar_normal.png"));
    static const QImage hover(QStringLiteral(":/huiwen/screenshot/toolbar_hover.png"));
    struct ToolIcon
    {
        QAbstractButton *button;
        int sourceX;
    };
    const ToolIcon icons[] = {{ui->chkRectangle, 17}, {ui->chkEllipse, 51}, {ui->chkArrow, 85},
                              {ui->chkText, 119},     {ui->btnUndo, 158},   {ui->btnSave, 192},
                              {ui->btnQuit, 230},     {ui->btnFinish, 266}};

    QPainter painter(ui->widTool);
    painter.fillRect(ui->widTool->rect(), QColor(0, 0, 0, 140));
    for (const ToolIcon &icon : icons)
    {
        // 按钮已隐藏，underMouse() 恒为 false；用 MouseMove 维护的 m_hoveredButton 判断悬停
        const QImage &sprite =
            (icon.button->isChecked() || icon.button->isDown() || icon.button == m_hoveredButton)
                ? hover
                : normal;
        painter.drawImage(QRect(icon.sourceX * kToolbarScale, 4 * kToolbarScale, 20 * kToolbarScale,
                                20 * kToolbarScale),
                          sprite, QRect(icon.sourceX, 4, 20, 20));
    }
    return true;
}

// widTool 鼠标释放：命中按钮区域则触发 click
bool CaptureToolbar::onToolButtonRelease(QEvent *event)
{
    const auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton)
    {
        for (QAbstractButton *button : toolButtons())
        {
            if (button->geometry().contains(mouseEvent->pos()))
            {
                button->click();
                ui->widTool->update();
                break;
            }
        }
    }
    return true;
}

// widToolColor 绘制：颜色条
bool CaptureToolbar::onColorPaint()
{
    static const QImage colorBar(QStringLiteral(":/huiwen/screenshot/toolbar_color.png"));
    QPainter painter(ui->widToolColor);
    painter.drawImage(ui->widToolColor->rect(), colorBar, colorBar.rect());
    return true;
}

// widTool 鼠标移动：手动维护悬停状态（按钮已 hide，underMouse 不可用）
bool CaptureToolbar::onToolMouseMove(QEvent *event)
{
    const auto *mouseEvent = static_cast<QMouseEvent *>(event);
    QAbstractButton *hovered = nullptr;
    for (QAbstractButton *button : toolButtons())
    {
        if (button->geometry().contains(mouseEvent->pos()))
        {
            hovered = button;
            break;
        }
    }
    if (hovered != m_hoveredButton)
    {
        m_hoveredButton = hovered;
        ui->widTool->update();
        if (hovered)
        {
            const QString tip = ToolButtonTipText(hovered, ui);
            if (!tip.isEmpty())
                QToolTip::showText(mouseEvent->globalPos() + QPoint(8, 8), tip, ui->widTool);
        }
        else
        {
            QToolTip::hideText();
        }
    }
    ui->widTool->setCursor(hovered ? Qt::PointingHandCursor : Qt::ArrowCursor);
    return true;
}

bool CaptureToolbar::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    const bool onTool = (watched == ui->widTool);
    const bool onColor = (watched == ui->widToolColor);

    // widTool Paint：工具图标精灵
    if (onTool && type == QEvent::Paint)
        return onToolPaint();
    // widToolColor Paint：颜色条
    if (onColor && type == QEvent::Paint)
        return onColorPaint();
    // widTool 鼠标释放：命中按钮 -> click
    if (onTool && type == QEvent::MouseButtonRelease)
        return onToolButtonRelease(event);
    // widTool 鼠标移动：悬停高亮 + 提示
    if (onTool && type == QEvent::MouseMove)
        return onToolMouseMove(event);
    // 拦截 widTool 按下（仅释放才触发 click）
    if (onTool && type == QEvent::MouseButtonPress)
        return true;
    // 拦截工具栏区域双击：防止冒泡到 CaptureOverlay 误触发"完成"
    if (type == QEvent::MouseButtonDblClick && (onTool || onColor))
        return true;
    // 离开 widTool 时清理悬停状态；Enter/Leave/Press/Release 触发重绘
    if (type == QEvent::Enter || type == QEvent::Leave || type == QEvent::MouseButtonPress ||
        type == QEvent::MouseButtonRelease)
    {
        if (onTool && type == QEvent::Leave)
        {
            m_hoveredButton = nullptr;
            ui->widTool->setCursor(Qt::ArrowCursor);
            QToolTip::hideText();
        }
        ui->widTool->update();
    }
    return QWidget::eventFilter(watched, event);
}
