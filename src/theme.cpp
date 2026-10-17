#include "theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QMenu>
#include <QProxyStyle>
#include <QSettings>
#include <QStyleFactory>
#include <QStyleHints>

namespace spacepenguin {
namespace {

bool s_followingSystem = false;

QString rgba(const QColor &color)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(color.alphaF(), 'f', 3));
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

void applyColors(bool dark)
{
    const Theme::Colors colors = Theme::colors(dark);
    QApplication::setPalette(Theme::palette(colors));
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        app->setStyleSheet(Theme::styleSheet(colors));
}

void followSystemScheme()
{
    if (s_followingSystem)
        return;
    QStyleHints *hints = QGuiApplication::styleHints();
    if (!hints)
        return;
    s_followingSystem = true;
    QObject::connect(hints, &QStyleHints::colorSchemeChanged, qApp, [] {
        if (Theme::storedMode() == Theme::Mode::System)
            applyColors(Theme::isDark());
    });
}

class ChromeStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                  QStyleHintReturn *returnData) const override
    {
        switch (hint) {
        case SH_DialogButtonBox_ButtonsHaveIcons:
            return 0;
        case SH_Menu_FlashTriggeredItem:
            return 0;
        default:
            return QProxyStyle::styleHint(hint, option, widget, returnData);
        }
    }

    void polish(QWidget *widget) override
    {
        // Popup menus are windows of their own: without a see-through window behind
        // them, their rounded border shows square corners. The window system's
        // shadow would also be square, so the menus go without it.
        if (qobject_cast<QMenu *>(widget) && !widget->testAttribute(Qt::WA_WState_Created)) {
            widget->setAttribute(Qt::WA_TranslucentBackground);
            widget->setWindowFlag(Qt::FramelessWindowHint);
            widget->setWindowFlag(Qt::NoDropShadowWindowHint);
        }
        QProxyStyle::polish(widget);
    }
    using QProxyStyle::polish;
};

} // namespace

QStyle *Theme::createStyle()
{
    return new ChromeStyle(QStyleFactory::create(QStringLiteral("Fusion")));
}

QList<Theme::Mode> Theme::modes()
{
    return {Mode::System, Mode::Light, Mode::Dark};
}

QString Theme::modeKey(Mode mode)
{
    switch (mode) {
    case Mode::Light:
        return QStringLiteral("light");
    case Mode::Dark:
        return QStringLiteral("dark");
    case Mode::System:
        break;
    }
    return QStringLiteral("system");
}

QString Theme::modeLabel(Mode mode)
{
    switch (mode) {
    case Mode::Light:
        return QCoreApplication::translate("Theme", "Light");
    case Mode::Dark:
        return QCoreApplication::translate("Theme", "Dark");
    case Mode::System:
        break;
    }
    return QCoreApplication::translate("Theme", "Follow system");
}

Theme::Mode Theme::modeFromKey(const QString &key)
{
    if (key == QLatin1String("light"))
        return Mode::Light;
    if (key == QLatin1String("dark"))
        return Mode::Dark;
    return Mode::System;
}

Theme::Mode Theme::storedMode()
{
    const QString key = QSettings().value(QStringLiteral("appearance/theme"),
                                          QStringLiteral("system")).toString();
    return modeFromKey(key);
}

void Theme::storeMode(Mode mode)
{
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/theme"), modeKey(mode));
}

void Theme::apply(Mode mode)
{
    QStyleHints *hints = QGuiApplication::styleHints();
    if (hints) {
        switch (mode) {
        case Mode::Light:
            hints->setColorScheme(Qt::ColorScheme::Light);
            break;
        case Mode::Dark:
            hints->setColorScheme(Qt::ColorScheme::Dark);
            break;
        case Mode::System:
            hints->unsetColorScheme();
            break;
        }
    }

    followSystemScheme();

    bool dark = false;
    switch (mode) {
    case Mode::Light:
        dark = false;
        break;
    case Mode::Dark:
        dark = true;
        break;
    case Mode::System:
        dark = hints && hints->colorScheme() == Qt::ColorScheme::Dark;
        break;
    }
    applyColors(dark);
}

Theme::Mode Theme::effectiveMode()
{
    QStyleHints *hints = QGuiApplication::styleHints();
    if (!hints || hints->colorScheme() == Qt::ColorScheme::Unknown)
        return Mode::System;
    return hints->colorScheme() == Qt::ColorScheme::Dark ? Mode::Dark : Mode::Light;
}

bool Theme::isDark()
{
    switch (storedMode()) {
    case Mode::Light:
        return false;
    case Mode::Dark:
        return true;
    case Mode::System:
        break;
    }
    QStyleHints *hints = QGuiApplication::styleHints();
    return hints && hints->colorScheme() == Qt::ColorScheme::Dark;
}

Theme::Colors Theme::colors(bool dark)
{
    Colors c;
    c.dark = dark;
    if (dark) {
        c.window = QColor(0x22, 0x25, 0x2d);
        c.tabStrip = QColor(0x15, 0x17, 0x1d);
        c.surface = QColor(0x2c, 0x30, 0x3a);
        c.raised = QColor(0x3a, 0x3f, 0x4b);
        c.text = QColor(0xe9, 0xeb, 0xf1);
        c.mutedText = QColor(0x9a, 0xa1, 0xb1);
        c.border = QColor(0x3a, 0x3e, 0x49);
        c.accent = QColor(0x82, 0xa0, 0xff);
        c.accentText = QColor(0x10, 0x14, 0x22);
        c.privateAccent = QColor(0xb4, 0x8c, 0xff);
        c.danger = QColor(0xff, 0x7a, 0x7a);
        c.success = QColor(0x6c, 0xd6, 0x9a);
    } else {
        c.window = QColor(0xf7, 0xf8, 0xfa);
        c.tabStrip = QColor(0xe4, 0xe7, 0xee);
        c.surface = QColor(0xff, 0xff, 0xff);
        c.raised = QColor(0xe6, 0xe9, 0xf0);
        c.text = QColor(0x1b, 0x1e, 0x25);
        c.mutedText = QColor(0x5f, 0x66, 0x75);
        c.border = QColor(0xd3, 0xd7, 0xe0);
        c.accent = QColor(0x2f, 0x5f, 0xe8);
        c.accentText = QColor(0xff, 0xff, 0xff);
        c.privateAccent = QColor(0x7c, 0x4d, 0xdb);
        c.danger = QColor(0xc8, 0x2b, 0x2b);
        c.success = QColor(0x1f, 0x8a, 0x4c);
    }
    return c;
}

Theme::Colors Theme::currentColors()
{
    return colors(isDark());
}

QPalette Theme::palette(const Colors &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.surface);
    p.setColor(QPalette::AlternateBase, c.dark ? c.window.lighter(108) : c.window);
    p.setColor(QPalette::ToolTipBase, c.surface);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.mutedText);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.dark ? c.surface : c.window);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::BrightText, c.danger);
    p.setColor(QPalette::Light, c.dark ? c.raised.lighter(120) : QColor(Qt::white));
    p.setColor(QPalette::Midlight, c.raised);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Dark, c.border.darker(130));
    p.setColor(QPalette::Shadow, QColor(0, 0, 0, c.dark ? 160 : 70));
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::LinkVisited, c.accent.darker(c.dark ? 90 : 120));
    p.setColor(QPalette::Accent, c.accent);

    const QColor disabledText = withAlpha(c.text, 0.38);
    p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::Highlight, c.raised);
    return p;
}

QString Theme::styleSheet(const Colors &c)
{
    QString qss = QStringLiteral(R"(
QToolTip { color: @text; background: @surface; border: 1px solid @border; padding: 5px 10px; border-radius: 10px; }

QMenu { background: @surface; color: @text; border: 1px solid @border; border-radius: 14px; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 12px; border-radius: 9px; }
QMenu::item:selected { background: @hover; }
QMenu::item:disabled { color: @muted; }
QMenu::separator { height: 1px; background: @border; margin: 5px 8px; }
QMenu::icon { padding-left: 8px; }
QMenu::right-arrow { width: 10px; height: 10px; }

QMenuBar { background: @tabStrip; color: @text; border: none; }
QMenuBar::item { padding: 4px 10px; background: transparent; border-radius: 10px; }
QMenuBar::item:selected { background: @hover; }

QLineEdit { background: @surface; color: @text; border: 1px solid @border; border-radius: 14px; padding: 5px 12px; selection-background-color: @accent; selection-color: @accentText; }
QLineEdit:focus { border: 1px solid @accent; }
QLineEdit:read-only { background: @window; }

QPushButton { background: @surface; color: @text; border: 1px solid @border; border-radius: 15px; padding: 6px 16px; }
QPushButton:hover { background: @hover; }
QPushButton:pressed { background: @pressed; }
QPushButton:disabled { color: @muted; }
QPushButton[accent="true"] { background: @accent; color: @accentText; border-color: @accent; }
QPushButton[accent="true"]:disabled { background: @raised; color: @muted; border-color: @border; }
QPushButton[danger="true"] { color: @danger; }

QComboBox { background: @surface; color: @text; border: 1px solid @border; border-radius: 14px; padding: 5px 12px; }
QComboBox:hover { border-color: @muted; }
QComboBox QAbstractItemView { background: @surface; color: @text; border: 1px solid @border; selection-background-color: @hover; selection-color: @text; outline: none; }

QCheckBox, QRadioButton { color: @text; spacing: 8px; }

QTreeView, QListView, QTreeWidget, QListWidget { background: @surface; color: @text; border: 1px solid @border; border-radius: 14px; padding: 4px; outline: none; }
QTreeView::item, QListView::item { padding: 5px 4px; border-radius: 8px; }
QTreeView::item:selected, QListView::item:selected { background: @selection; color: @text; }
QTreeView::item:hover, QListView::item:hover { background: @hover; }
QHeaderView::section { background: @window; color: @muted; border: none; border-bottom: 1px solid @border; padding: 6px 8px; font-weight: 600; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle { background: @scroll; border-radius: 3px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: @muted; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QProgressBar { background: @raised; border: none; border-radius: 3px; max-height: 6px; text-align: center; }
QProgressBar::chunk { background: @accent; border-radius: 3px; }

QGroupBox { color: @text; border: 1px solid @border; border-radius: 16px; margin-top: 14px; padding: 12px 10px 10px 10px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }

/* ---- Browser chrome --------------------------------------------------- */

#tabStrip { background: @tabStrip; }
/* Behind everything: the thin resize border of the window's own frame, and on
   Wayland, where the window is see-through so its corners can be rounded, any gap. */
QMainWindow#browserWindow { background: @tabStrip; }
#windowControls QToolButton { padding: 6px; border-radius: 14px; }
#windowControls QToolButton#windowClose:hover { background: #d93a3a; }
#windowControls QToolButton#windowClose:pressed { background: #b52d2d; }
#tabStrip[private="true"] { background: @privateStrip; }

QTabBar#tabBar { background: transparent; border: none; qproperty-drawBase: 0; }
QTabBar#tabBar::tab {
    background: transparent; color: @muted; border: none;
    padding: 6px 8px 6px 12px; margin: 6px 2px 6px 2px;
    border-radius: 13px;
}
QTabBar#tabBar::tab:hover { background: @tabHover; color: @text; }
QTabBar#tabBar::tab:selected { background: @window; color: @text; border: 1px solid @border; }
QTabBar#tabBar QToolButton { background: @tabStrip; border: none; border-radius: 13px; margin: 6px 0; }
QTabBar#tabBar QToolButton:hover { background: @hover; }
QTabBar#tabBar QToolButton#tabClose { background: transparent; margin: 0; padding: 2px; border-radius: 10px; }
QTabBar#tabBar QToolButton#tabClose:hover { background: @pressed; }

#navBar { background: @window; border-bottom: 1px solid @border; }
#navBar QToolButton, #tabStrip QToolButton, #findBar QToolButton, #bookmarksBar QToolButton {
    background: transparent; color: @text; border: none; border-radius: 14px; padding: 5px;
}
#navBar QToolButton:hover, #tabStrip QToolButton:hover, #findBar QToolButton:hover, #bookmarksBar QToolButton:hover { background: @hover; }
#navBar QToolButton:pressed, #tabStrip QToolButton:pressed, #findBar QToolButton:pressed, #bookmarksBar QToolButton:pressed { background: @pressed; }
#navBar QToolButton:disabled { color: @muted; }
#navBar QToolButton::menu-indicator { image: none; width: 0; }
#navBar QToolButton#shieldButton { padding: 5px 10px; font-weight: 600; color: @muted; }
#navBar QToolButton#shieldButton[active="true"] { color: @success; }
#navBar QToolButton#zoomButton { padding: 4px 10px; border: 1px solid @border; border-radius: 13px; color: @muted; }

#omnibox { background: @omnibox; border: 1px solid transparent; border-radius: 17px; padding: 6px 10px; font-size: 14px; }
#omnibox:hover { border-color: @border; }
#omnibox:focus { background: @surface; border: 2px solid @accent; padding: 5px 9px; }

#bookmarksBar { background: @window; border: none; border-bottom: 1px solid @border; padding: 2px 6px; spacing: 2px; }
#bookmarksBar QToolButton { padding: 3px 10px; color: @text; border-radius: 12px; }
#bookmarksBar QLabel { color: @muted; padding: 3px 8px; }

#loadProgress { background: transparent; border: none; border-radius: 0; max-height: 2px; min-height: 2px; }
#loadProgress::chunk { background: @accent; border-radius: 0; }

#findBar { background: @window; border-top: 1px solid @border; }
#findBar QLabel#findStatus { color: @muted; padding: 0 6px; }
#findBar QLineEdit { border-radius: 14px; min-width: 260px; }

#sandboxBanner { background: @warningBg; border: 1px solid @border; border-radius: 14px; }
#sandboxBanner QLabel { color: @text; }
#linkPreview { background: @surface; color: @muted; border: 1px solid @border; border-radius: 12px; padding: 3px 10px; }

#privateBadge { color: @privateAccent; font-weight: 600; padding: 0 8px; }

/* ---- Dialogs ----------------------------------------------------------- */

#settingsNav { background: transparent; border: none; padding: 6px; font-size: 14px; }
#settingsNav::item { padding: 8px 12px; border-radius: 12px; margin: 1px 0; }
#settingsNav::item:selected { background: @selection; color: @text; }
#pageTitle { font-size: 20px; font-weight: 700; color: @text; }
#pageSubtitle, QLabel[muted="true"] { color: @muted; }
#sectionTitle { font-size: 13px; font-weight: 700; color: @text; margin-top: 8px; }
#card { background: @surface; border: 1px solid @border; border-radius: 18px; }
#downloadRow { background: @surface; border-bottom: 1px solid @border; }
#downloadName { font-weight: 600; color: @text; }
#heroTitle { font-size: 24px; font-weight: 800; color: @text; }
)");

    const QColor hover = c.dark ? withAlpha(QColor(Qt::white), 0.08) : withAlpha(QColor(Qt::black), 0.06);
    const QColor pressed = c.dark ? withAlpha(QColor(Qt::white), 0.14) : withAlpha(QColor(Qt::black), 0.11);
    const QColor tabHover = c.dark ? withAlpha(QColor(Qt::white), 0.06) : withAlpha(QColor(Qt::white), 0.55);
    const QColor selection = withAlpha(c.accent, c.dark ? 0.28 : 0.16);
    const QColor scroll = withAlpha(c.mutedText, 0.45);
    const QColor omnibox = c.dark ? c.tabStrip : c.raised;
    const QColor warningBg = c.dark ? QColor(0x4a, 0x3a, 0x12) : QColor(0xff, 0xf1, 0xc7);
    const QColor privateStrip = c.dark ? QColor(0x23, 0x18, 0x3a) : QColor(0xe6, 0xdc, 0xf7);

    // Longest names first so that @tabStrip is not eaten by a shorter token.
    const QList<std::pair<QString, QString>> tokens = {
        {QStringLiteral("@privateAccent"), rgba(c.privateAccent)},
        {QStringLiteral("@warningBg"), rgba(warningBg)},
        {QStringLiteral("@privateStrip"), rgba(privateStrip)},
        {QStringLiteral("@accentText"), rgba(c.accentText)},
        {QStringLiteral("@selection"), rgba(selection)},
        {QStringLiteral("@tabStrip"), rgba(c.tabStrip)},
        {QStringLiteral("@tabHover"), rgba(tabHover)},
        {QStringLiteral("@omnibox"), rgba(omnibox)},
        {QStringLiteral("@surface"), rgba(c.surface)},
        {QStringLiteral("@pressed"), rgba(pressed)},
        {QStringLiteral("@success"), rgba(c.success)},
        {QStringLiteral("@window"), rgba(c.window)},
        {QStringLiteral("@border"), rgba(c.border)},
        {QStringLiteral("@accent"), rgba(c.accent)},
        {QStringLiteral("@danger"), rgba(c.danger)},
        {QStringLiteral("@scroll"), rgba(scroll)},
        {QStringLiteral("@raised"), rgba(c.raised)},
        {QStringLiteral("@hover"), rgba(hover)},
        {QStringLiteral("@muted"), rgba(c.mutedText)},
        {QStringLiteral("@text"), rgba(c.text)},
    };
    for (const auto &[token, value] : tokens)
        qss.replace(token, value);
    return qss;
}

}
