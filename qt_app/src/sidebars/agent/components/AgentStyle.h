/* 助手及其对话框共用浅色样式和调色板，不改变文档工作区主题。 */
#pragma once
#include <QPalette>
#include <QWidget>

inline void applyAgentStyle(QWidget *widget)
{
    // 显式覆盖原生控件和视口使用的调色板，系统处于深色时也保持同一套浅色。
    QPalette colors = widget->palette();
    colors.setColor(QPalette::Window, QColor("#FFFFFF"));
    colors.setColor(QPalette::Base, QColor("#FFFFFF"));
    colors.setColor(QPalette::AlternateBase, QColor("#F5F5F5"));
    colors.setColor(QPalette::WindowText, QColor("#333333"));
    colors.setColor(QPalette::Text, QColor("#333333"));
    colors.setColor(QPalette::Button, QColor("#EEEEEE"));
    colors.setColor(QPalette::ButtonText, QColor("#333333"));
    colors.setColor(QPalette::PlaceholderText, QColor("#888888"));
    colors.setColor(QPalette::Highlight, QColor("#DDDDDD"));
    colors.setColor(QPalette::HighlightedText, QColor("#333333"));
    colors.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#AAAAAA"));
    colors.setColor(QPalette::Disabled, QPalette::Text, QColor("#999999"));
    widget->setPalette(colors);
    widget->setAttribute(Qt::WA_StyledBackground);
    widget->setAutoFillBackground(true);
    widget->setStyleSheet(QStringLiteral(
        "QWidget { color: #333333; font-size: 18px; }"
        "QWidget#agentSidebar, QDialog#agentDialog { background: #FFFFFF; }"
        "QWidget#agentComposer { background: #E8E8E8; border: none; "
        "border-top-left-radius: 16px; border-top-right-radius: 16px; }"
        "QToolButton { background: transparent; color: #666666; border: none; "
        "border-radius: 7px; padding: 3px; }"
        "QToolButton:hover { background: #EAEAEA; }"
        "QToolButton:pressed, QToolButton:checked { background: #DDDDDD; }"
        "QToolButton:disabled { color: #AAAAAA; }"
        "QWidget#agentComposer QLabel, QWidget#agentComposer QToolButton, "
        "QWidget#agentComposer QPlainTextEdit { font-size: 28px; }"
        "QWidget#agentComposer QToolButton { padding: 6px; }"
        "QWidget#agentComposerActions QToolButton { font-size: 22px; padding: 4px; }"
        "QWidget#agentComposer QLabel#agentComposerStatus { font-size: 24px; color: #777777; }"
        "QFrame#agentWorkspaceBar { background: #E8E8E8; border: none; "
        "border-top-left-radius: 16px; border-top-right-radius: 16px; }"
        "QWidget#agentComposer QLabel#agentWorkspaceName { font-size: 20px; }"
        "QToolButton#agentSendButton { background: #DDDDDD; border-radius: 22px; }"
        "QToolButton#agentSendButton:hover { background: #CCCCCC; }"
        "QToolButton#agentSendButton:pressed { background: #BBBBBB; }"
        "QToolButton#agentSendButton:disabled { background: #E8E8E8; }"
        "QFrame#agentComposerCard { background: #F5F5F5; border: 1px solid #D6D6D6; "
        "border-top-left-radius: 16px; border-top-right-radius: 16px; }"
        "QPlainTextEdit { background: #F5F5F5; color: #333333; border: none; }"
        "QLineEdit { background: #F5F5F5; color: #333333; border: 1px solid #DDDDDD; "
        "border-radius: 7px; padding: 7px; selection-background-color: #DDDDDD; "
        "selection-color: #333333; }"
        "QTextBrowser { background: #FFFFFF; border: none; color: #333333; }"
        "QPushButton { color: #333333; background: #EEEEEE; border: none; border-radius: 7px; "
        "padding: 8px 16px; }"
        "QPushButton:default { background: #DDDDDD; }"
        "QPushButton:hover { background: #E0E0E0; }"
        "QPushButton:pressed, QPushButton:checked { background: #CCCCCC; }"
        "QPushButton:disabled { color: #AAAAAA; background: #F5F5F5; }"
        "QListWidget { background: #FFFFFF; color: #333333; border: none; "
        "outline: none; }"
        "QListWidget::item { padding: 12px; border-radius: 7px; }"
        "QListWidget::item:hover { background: #F5F5F5; }"
        "QListWidget::item:selected { background: #E5E5E5; color: #333333; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #C6C6C6; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; "
        "}"));
}
