import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// An inline request (join, trust, permission, error): what used to be a
// pop-up. `request` is one entry of App.requests.
Card {
    id: banner

    required property var request
    readonly property string kind: request.kind

    Layout.fillWidth: true
    padding: 14
    color: kind === "error" ? Theme.dangerSoft : kind === "permission" ? Theme.accentSoft : Theme.warningSoft
    border.color: kind === "error" ? Theme.danger : kind === "permission" ? Theme.accent : Theme.warning

    Accessible.role: Accessible.AlertMessage
    Accessible.name: kind === "join" ? qsTr("%1 wants to join").arg(request.name || "")
                   : kind === "trust" ? qsTr("Unknown fingerprint from %1").arg(request.name || qsTr("a client"))
                   : kind === "permission" ? qsTr("Accessibility permission needed")
                   : (request.text || qsTr("Something needs attention"))

    // ---- join ---------------------------------------------------------------
    RowLayout {
        visible: banner.kind === "join"
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: qsTr("%1 wants to join").arg(banner.request.name)
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Choose where it sits next to %1. It becomes part of the layout and the server restarts to pick it up.").arg(App.screenName)
                color: Theme.muted
                wrapMode: Text.WordWrap
            }
        }
        Button { text: qsTr("Ignore"); onClicked: App.ignoreJoin(banner.request.name) }
        Button { text: qsTr("Left"); onClicked: App.placeJoin(banner.request.name, LayoutModel.Left) }
        Button { text: qsTr("Above"); onClicked: App.placeJoin(banner.request.name, LayoutModel.Up) }
        Button { text: qsTr("Below"); onClicked: App.placeJoin(banner.request.name, LayoutModel.Down) }
        Button { text: qsTr("Right"); highlighted: true; onClicked: App.placeJoin(banner.request.name, LayoutModel.Right) }
    }

    // ---- trust --------------------------------------------------------------
    ColumnLayout {
        visible: banner.kind === "trust"
        spacing: 10
        Label {
            text: banner.request.peerRole === "server"
                  ? qsTr("The server %1 presents a fingerprint you have not seen before").arg(banner.request.name)
                  : banner.request.reason === "settings"
                    ? qsTr("%1 wants to change settings from its window").arg(banner.request.name)
                  : qsTr("A client presents a fingerprint you have not seen before")
            font.weight: Font.DemiBold
            color: Theme.text
        }
        RowLayout {
            spacing: 16
            Label {
                text: banner.request.randomart || ""
                font.family: App.monoFontFamily
                font.pixelSize: 11
                color: Theme.text
                padding: 8
                background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: Theme.radiusSmall }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    text: banner.request.sha256 || ""
                    font.family: App.monoFontFamily
                    color: Theme.text
                    wrapMode: Text.WrapAnywhere
                }
                Label {
                    Layout.fillWidth: true
                    text: banner.request.peerRole === "server"
                          ? qsTr("Compare it with the fingerprint shown on the server's Security page. If they differ, reject: something else on the network is pretending to be the server.")
                          : qsTr("Compare it with the fingerprint shown on that computer's Security page. If they differ, reject: something else on the network is pretending to be it.")
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            Button { text: qsTr("Reject"); onClicked: App.rejectTrust(banner.request.id) }
            Button { text: banner.request.reason === "settings" ? qsTr("Trust it") : qsTr("Trust and connect"); highlighted: true; onClicked: App.acceptTrust(banner.request.id) }
        }
    }

    // ---- permission ---------------------------------------------------------
    RowLayout {
        visible: banner.kind === "permission"
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label { text: qsTr("Accessibility permission needed"); font.weight: Font.DemiBold; color: Theme.text }
            Label { Layout.fillWidth: true; text: banner.request.text || ""; color: Theme.muted; wrapMode: Text.WordWrap }
        }
        Button { text: qsTr("Check again"); onClicked: App.refreshAccessibility() }
        Button { text: qsTr("Open System Settings"); highlighted: true; onClicked: App.requestAccessibility() }
    }

    // ---- error --------------------------------------------------------------
    RowLayout {
        visible: banner.kind === "error"
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label { text: qsTr("Something needs attention"); font.weight: Font.DemiBold; color: Theme.text }
            Label { Layout.fillWidth: true; text: banner.request.text || ""; color: Theme.muted; wrapMode: Text.WordWrap }
        }
        Button { text: qsTr("Dismiss"); onClicked: App.dismissRequest(banner.request.id) }
    }
}
