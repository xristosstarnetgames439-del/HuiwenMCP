/* 顶部文件与滚动入口；菜单选中项是同步状态的唯一来源，工作区接收变化后控制联动。 */
#include "FunctionBar.h"
#include "FileMenu.h"
#include "SelectionMenu.h"
#include "sidebars/common/SidebarIcons.h"
#include <QAction>
#include <QActionGroup>
#include <QFontMetrics>
#include <QLayout>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <algorithm>

FunctionBar::FunctionBar(const QString &mode, QWidget *parent)
    : QToolBar(parent), fileMenu_(new FileMenu(mode, this))
{
    setObjectName(QStringLiteral("workspaceFunctionBar"));
    setMovable(false);
    setFloatable(false);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setFixedHeight(72);
    setContentsMargins(0, 0, 0, 0);
    setStyleSheet(QStringLiteral(
        "QToolBar#workspaceFunctionBar { background: #F2F3F5; border: none; "
        "margin: 0; padding: 0; spacing: 0; }"
        "QToolBar#workspaceFunctionBar QToolButton { background: transparent; border: none; "
        "margin: 0; padding: 0 20px; color: #34445C; font-size: 28px; font-weight: normal; }"
        "QToolBar#workspaceFunctionBar QToolButton[selected=\"true\"] "
        "{ background: #DCE1E8; }"
        "QToolBar#workspaceFunctionBar QToolButton#agentSidebarToggle, "
        "QToolBar#workspaceFunctionBar QToolButton#navigationSidebarToggle "
        "{ padding: 0; border-radius: 8px; }"
        "QToolBar#workspaceFunctionBar QToolButton#agentSidebarToggle:checked, "
        "QToolBar#workspaceFunctionBar QToolButton#navigationSidebarToggle:checked "
        "{ background: #E5E5E5; }"
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
        connect(menu, &QMenu::aboutToShow, button, [setSelected] { setSelected(true); });
        connect(menu, &QMenu::aboutToHide, button, [setSelected] { setSelected(false); });
        // 先应用继承样式再测量 28px 字体，避免按默认小字号分配宽度而裁切文字。
        button->ensurePolished();
        const int textWidth = std::max(button->fontMetrics().horizontalAdvance(text),
                                       button->fontMetrics().boundingRect(text).width());
        button->setFixedSize(std::max(button->sizeHint().width(), textWidth + 40), height());
        addWidget(button);
    };
    navigationToggle_ = new QToolButton(this);
    navigationToggle_->setObjectName("navigationSidebarToggle");
    navigationToggle_->setIcon(sidebarIcon(SidebarEdge::Left));
    navigationToggle_->setIconSize(QSize(28, 28));
    navigationToggle_->setFixedSize(48, 48);
    navigationToggle_->setCheckable(true);
    navigationToggle_->setCursor(Qt::PointingHandCursor);
    navigationToggle_->setAccessibleName(QStringLiteral("开关工作区导航侧栏"));
    setNavigationSidebarVisible(false);
    connect(navigationToggle_, &QToolButton::toggled, this, [this](bool visible) {
        setNavigationSidebarVisible(visible);
        emit navigationSidebarToggled(visible);
    });
    addWidget(navigationToggle_);
    addMenuButton(QStringLiteral("文件"), fileMenu_);
    // 单栏没有联动目标，保留同一个功能条，但不创建双栏滚动菜单。
    if (mode != "preview")
    {
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
    auto *space = new QWidget(this);
    space->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    trailingSpacer_ = addWidget(space);
    agentToggle_ = new QToolButton(this);
    agentToggle_->setObjectName("agentSidebarToggle");
    agentToggle_->setIcon(sidebarIcon(SidebarEdge::Right));
    agentToggle_->setIconSize(QSize(28, 28));
    agentToggle_->setFixedSize(48, 48);
    agentToggle_->setCheckable(true);
    agentToggle_->setCursor(Qt::PointingHandCursor);
    agentToggle_->setAccessibleName(QStringLiteral("开关文档助手侧栏"));
    setAgentSidebarVisible(true);
    connect(agentToggle_, &QToolButton::toggled, this, [this](bool visible) {
        agentToggle_->setToolTip(visible ? QStringLiteral("收起文档助手")
                                         : QStringLiteral("展开文档助手"));
        emit agentSidebarToggled(visible);
    });
    addWidget(agentToggle_);
}

FileMenu *FunctionBar::fileMenu() const { return fileMenu_; }

QAction *FunctionBar::addCommand(const QString &text)
{
    auto *action = new QAction(text, this);
    insertAction(trailingSpacer_, action); // 后加的保存等命令仍放在左侧，折叠按钮始终位于最右侧。
    return action;
}

void FunctionBar::setAgentSidebarVisible(bool visible)
{
    agentToggle_->setChecked(visible);
    agentToggle_->setToolTip(visible ? QStringLiteral("收起文档助手")
                                     : QStringLiteral("展开文档助手"));
}

bool FunctionBar::isScrollSynchronized() const
{
    return synchronizedAction_ && synchronizedAction_->isChecked();
}

void FunctionBar::setNavigationSidebarVisible(bool visible)
{
    navigationToggle_->setChecked(visible);
    navigationToggle_->setToolTip(visible ? QStringLiteral("收起工作区导航")
                                          : QStringLiteral("展开工作区导航"));
}
