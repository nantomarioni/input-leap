import QtQuick
import QtQuick.Controls
import InputLeap.Ui

// A small rounded state label: "neutral" | "success" | "warning" | "danger" | "info".
Rectangle {
    id: pill

    property string text
    property string tone: "neutral"
    property bool showDot: true

    readonly property color fg: tone === "success" ? Theme.success
                              : tone === "warning" ? Theme.warning
                              : tone === "danger" ? Theme.danger
                              : tone === "info" ? Theme.accent
                              : Theme.muted
    readonly property color bgColor: tone === "success" ? Theme.successSoft
                                   : tone === "warning" ? Theme.warningSoft
                                   : tone === "danger" ? Theme.dangerSoft
                                   : tone === "info" ? Theme.accentSoft
                                   : Theme.borderSoft

    implicitWidth: content.implicitWidth + 20
    implicitHeight: 24
    radius: 12
    color: bgColor

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            visible: pill.showDot
            width: 8
            height: 8
            radius: 4
            color: pill.fg
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            text: pill.text
            color: pill.fg
            font.pixelSize: Theme.smallFontSize
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
