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

    // Tab group colours, the desktop's (src/tabgroups.cpp): strong on light, soft on dark.
    readonly property var groupColorNames: [qsTr("Grey"), qsTr("Blue"), qsTr("Red"), qsTr("Yellow"),
                                            qsTr("Green"), qsTr("Pink"), qsTr("Purple"), qsTr("Cyan"),
                                            qsTr("Orange")]
    readonly property var groupColorsLight: ["#5f6368", "#1a73e8", "#d93025", "#e8a200", "#1e8e3e",
                                             "#d01884", "#9334e6", "#007b83", "#e8710a"]
    readonly property var groupColorsDark: ["#dadce0", "#8ab4f8", "#f28b82", "#fdd663", "#81c995",
                                            "#ff8bcb", "#c58af9", "#78d9ec", "#fcad70"]
    function groupColor(index) { return (dark ? groupColorsDark : groupColorsLight)[index] || groupColorsLight[0]; }
    // Dark text on the soft dark-theme colours and on yellow and orange.
    function groupText(index) { return dark || index === 3 || index === 8 ? "#202124" : "#ffffff"; }

    readonly property int radius: 18
    readonly property int touch: 48   // smallest comfortable touch target
}
