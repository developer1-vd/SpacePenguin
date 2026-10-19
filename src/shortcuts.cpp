#include "shortcuts.h"

namespace spacepenguin {
namespace {

#ifdef Q_OS_MACOS
constexpr bool kMac = true;
#else
constexpr bool kMac = false;
#endif

// Qt::CTRL is Command on macOS; Qt::META is the Control key there.
QKeySequence key(QKeyCombination combination)
{
    return QKeySequence(combination);
}

} // namespace

QList<QKeySequence> Shortcuts::keys(Command command)
{
    switch (command) {
    case NewTab:
        return {key(Qt::CTRL | Qt::Key_T)};
    case NewWindow:
        return {key(Qt::CTRL | Qt::Key_N)};
    case NewPrivateWindow:
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_N)};
    case CloseTab:
        return {key(Qt::CTRL | Qt::Key_W)};
    case CloseWindow:
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_W)};
    case ReopenClosedTab:
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_T)};
    case NextTab:
        if constexpr (kMac)
            return {key(Qt::META | Qt::Key_Tab), key(Qt::CTRL | Qt::ALT | Qt::Key_Right),
                    key(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight)};
        return {key(Qt::CTRL | Qt::Key_Tab), key(Qt::CTRL | Qt::Key_PageDown)};
    case PreviousTab:
        if constexpr (kMac)
            return {key(Qt::META | Qt::SHIFT | Qt::Key_Tab), key(Qt::CTRL | Qt::ALT | Qt::Key_Left),
                    key(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft)};
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), key(Qt::CTRL | Qt::Key_PageUp)};
    case Quit:
        return {key(Qt::CTRL | Qt::Key_Q)};
    case Back:
        return QKeySequence::keyBindings(QKeySequence::Back);
    case Forward:
        return QKeySequence::keyBindings(QKeySequence::Forward);
    case Reload: {
        QList<QKeySequence> bindings = QKeySequence::keyBindings(QKeySequence::Refresh);
        const QKeySequence ctrlR = key(Qt::CTRL | Qt::Key_R);
        if (!bindings.contains(ctrlR))
            bindings.append(ctrlR);
        return bindings;
    }
    case Stop:
        return {key(Qt::Key_Escape)};
    case Home:
        if constexpr (kMac)
            return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_H)};
        return {key(Qt::ALT | Qt::Key_Home)};
    case FocusAddressBar:
        if constexpr (kMac)
            return {key(Qt::CTRL | Qt::Key_L)};
        return {key(Qt::CTRL | Qt::Key_L), key(Qt::ALT | Qt::Key_D), key(Qt::Key_F6)};
    case Find:
        return {key(Qt::CTRL | Qt::Key_F)};
    case ZoomIn:
        return {key(Qt::CTRL | Qt::Key_Plus), key(Qt::CTRL | Qt::Key_Equal)};
    case ZoomOut:
        return {key(Qt::CTRL | Qt::Key_Minus)};
    case ZoomReset:
        return {key(Qt::CTRL | Qt::Key_0)};
    case FullScreen:
        if constexpr (kMac)
            return {key(Qt::CTRL | Qt::META | Qt::Key_F)};
        return {key(Qt::Key_F11)};
    case BookmarkPage:
        return {key(Qt::CTRL | Qt::Key_D)};
    case BookmarksBar:
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_B)};
    case SideTabs:
        // Edge's binding.
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_Comma)};
    case Bookmarks:
        if constexpr (kMac)
            return {key(Qt::CTRL | Qt::ALT | Qt::Key_B)};
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_O)};
    case History:
        if constexpr (kMac)
            return {key(Qt::CTRL | Qt::Key_Y)};
        return {key(Qt::CTRL | Qt::Key_H)};
    case Downloads:
        if constexpr (kMac)
            return {key(Qt::CTRL | Qt::ALT | Qt::Key_L)};
        return {key(Qt::CTRL | Qt::Key_J)};
    case Extensions:
        return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_E)};
    case Settings:
        return {key(Qt::CTRL | Qt::Key_Comma)};
    case KeyboardShortcuts:
        if constexpr (kMac)
            return QKeySequence::keyBindings(QKeySequence::HelpContents);
        return {key(Qt::Key_F1)};
    }
    return {};
}

QString Shortcuts::text(Command command)
{
    const QList<QKeySequence> bindings = keys(command);
    return bindings.isEmpty() ? QString() : bindings.first().toString(QKeySequence::NativeText);
}

QString Shortcuts::primaryModifier()
{
    return kMac ? QStringLiteral("⌘") : QStringLiteral("Ctrl");
}

}
