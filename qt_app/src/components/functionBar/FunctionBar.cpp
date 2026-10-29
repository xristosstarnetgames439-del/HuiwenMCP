/* 顶部文件与滚动入口；菜单选中项是同步状态的唯一来源，工作区接收变化后控制联动。 */
#include "FunctionBar.h"
#include "FileMenu.h"
#include "SelectionMenu.h"
#include <QAction>
#include <QActionGroup>
#include <QFontMetrics>
#include <QLayout>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <algorithm>

FunctionBar::FunctionBar(QWidget *parent) : QToolBar(parent), fileMenu_(new FileMenu(this))
{
    setObjectName(QStringLiteral("workspaceFunctionBar"));
    setMovable(false);
    setFloatable(false);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setFixedHeight(72);
    setContentsMargins(0, 0, 0, 0);
    setStyleSheet(
        QStringLiteral("QToolBar#workspaceFunctionBar { background: #F2F3F5; border: none; "
                       "margin: 0; padding: 0; spacing: 0; }"
                       "QToolBar#workspaceFunctionBar QToolButton { background: transparent; border: none; "
                       "margin: 0; padding: 0 20px; color: #34445C; font-size: 28px; font-weight: normal; }"
                       "QToolBar#workspaceFunctionBar QToolButton[selected=\"true\"] "
                       "{ background: #DCE1E8; }"
                       "QToolBar#workspaceFunctionBar QToolButton::menu-indicator "
                       "{ image: none; width: 0; height: 0; }"));
    // 同时清除工具条布局的原生默认留白，按钮背景才能覆盖整个高度。
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSpacing(0);

    const auto addMenuButton = [this](const QString &text, QMenu *menu)
    {
        auto *button = new QToolButton(this);
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setCursor(Qt::PointingHandCursor);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setMenu(menu);
        button->setProperty("selected", false);
        // 背景只取决于菜单是否展开，避免关闭后残留 hover/pressed 高亮。
        const auto setSelected = [button](bool selected)
        {
            button->setProperty("selected", selected);
            button->style()->unpolish(button);
            button->style()->polish(button);
            button->update();
        };
        connect(menu, &QMenu::aboutToShow, button,
                [setSelected]
                {
                    setSelected(true);
                });
        connect(menu, &QMenu::aboutToHide, button,
                [setSelected]
                {
                    setSelected(false);
                });
        // 先应用继承样式再测量 28px 字体，避免按默认小字号分配宽度而裁切文字。
        button->ensurePolished();
        const int textWidth = std::max(button->fontMetrics().horizontalAdvance(text),
                                       button->fontMetrics().boundingRect(text).width());
        button->setFixedSize(std::max(button->sizeHint().width(), textWidth + 40), height());
        addWidget(button);
    };
    addMenuButton(QStringLiteral("文件"), fileMenu_);
    auto *menu = new SelectionMenu(this);
    auto *modes = new QActionGroup(menu);
    modes->setExclusive(true);
    synchronizedAction_ = menu->addAction(QStringLiteral("同步滚动"));
    auto *oneWay = menu->addAction(QStringLiteral("单向滚动"));
    for (auto *action : {synchronizedAction_, oneWay})
    {
        action->setCheckable(true);
        modes->addAction(action);
    }
    synchronizedAction_->setChecked(true);
    connect(synchronizedAction_, &QAction::toggled, this, &FunctionBar::scrollSyncChanged);
    addMenuButton(QStringLiteral("滚动设置"), menu);
}

FileMenu *FunctionBar::fileMenu() const
{
    return fileMenu_;
}

bool FunctionBar::isScrollSynchronized() const
{
    return synchronizedAction_->isChecked();
}
