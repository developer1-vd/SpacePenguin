pragma Singleton
import QtQuick

// The desktop app's palette, following the phone's light or dark setting.
QtObject {
    readonly property bool dark: Qt.styleHints.colorScheme === Qt.ColorScheme.Dark

    readonly property color background: dark ? "#0f1116" : "#f4f6fb"
    readonly property color bar: dark ? "#15171d" : "#ffffff"
    readonly property color field: dark ? "#22252e" : "#eceff5"
    readonly property color surface: dark ? "#1b1e26" : "#ffffff"
    readonly property color text: dark ? "#e8ebf3" : "#171a21"
    readonly property color muted: dark ? "#9aa2b5" : "#5b6272"
    readonly property color border: dark ? "#2a2e39" : "#d9deea"
    readonly property color accent: dark ? "#7b9cff" : "#2f5fe0"
    readonly property color accentText: dark ? "#0f1116" : "#ffffff"
    readonly property color pressed: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.07)
    readonly property color danger: dark ? "#ff7b7b" : "#c7362f"

    readonly property int radius: 18
    readonly property int touch: 48   // smallest comfortable touch target
}
