pragma Singleton
import QtQuick

// Design tokens (docs/ui-redesign.md §4.6). Light and dark derive from the
// same scale; `dark` follows the system colour scheme.
QtObject {
    readonly property bool dark: Application.styleHints.colorScheme === Qt.ColorScheme.Dark

    readonly property color bg: dark ? "#14181D" : "#F3F5F7"
    readonly property color surface: dark ? "#1E242B" : "#FFFFFF"
    readonly property color surfaceAlt: dark ? "#242B33" : "#F8FAFB"
    readonly property color rail: dark ? "#191E24" : "#EDF1F4"
    readonly property color border: dark ? "#2E3740" : "#DDE3E8"
    readonly property color borderSoft: dark ? "#262E36" : "#EEF2F5"
    readonly property color hover: dark ? "#2A323B" : "#E3E9EE"
    readonly property color text: dark ? "#E6EDF3" : "#1B2430"
    readonly property color muted: dark ? "#9AA8B6" : "#5B6B7B"

    readonly property color accent: dark ? "#5FB0D8" : "#236F93"
    readonly property color onAccent: dark ? "#0E1A22" : "#FFFFFF"
    readonly property color accentSoft: dark ? "#1C3542" : "#E3F0F7"
    readonly property color success: dark ? "#8BD34A" : "#4C7A12"
    readonly property color successSoft: dark ? "#213A10" : "#E8F3DA"
    readonly property color warning: dark ? "#F0B35A" : "#8A4B08"
    readonly property color warningSoft: dark ? "#3E2A10" : "#FCEFDC"
    readonly property color danger: dark ? "#F08A80" : "#B42318"
    readonly property color dangerSoft: dark ? "#3F1A17" : "#FBE4E1"
    readonly property color offline: dark ? "#6E7B88" : "#8A99A8"

    readonly property int radius: 12
    readonly property int radiusSmall: 8
    readonly property int pad: 16
    readonly property int gap: 12
    readonly property real fontSize: 13
    readonly property real smallFontSize: 12
    readonly property real titleFontSize: 18
}
