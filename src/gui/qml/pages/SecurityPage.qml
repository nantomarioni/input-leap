import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

ScrollView {
    id: page

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        x: 24
        y: 20
        width: page.availableWidth - 48
        spacing: 14

        ColumnLayout {
            spacing: 2
            Label { text: qsTr("Security"); font.pixelSize: Theme.titleFontSize; font.weight: Font.DemiBold; color: Theme.text }
            Label { text: qsTr("This computer's TLS identity and the peers it trusts. Trust requests appear here instead of pop-up windows."); color: Theme.muted }
        }

        Repeater {
            model: App.requests
            delegate: Banner {
                required property var modelData
                visible: modelData.kind === "trust"
                request: modelData
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 14

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                spacing: 0
                SectionTitle { text: qsTr("This computer's identity"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("Encrypt connections (TLS)")
                    divider: false
                    Switch { checked: App.settings.cryptoEnabled; onToggled: App.settings.cryptoEnabled = checked }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 14
                    visible: App.settings.cryptoEnabled
                    Label {
                        text: App.identity.randomart
                        font.family: App.monoFontFamily
                        font.pixelSize: 11
                        color: Theme.text
                        padding: 8
                        background: Rectangle { color: Theme.bg; radius: Theme.radiusSmall }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 6
                        Label { Layout.fillWidth: true; text: qsTr("SHA256 fingerprint · show this to the other computer"); color: Theme.muted; font.pixelSize: Theme.smallFontSize; wrapMode: Text.WordWrap }
                        Label {
                            Layout.fillWidth: true
                            text: App.identity.ready ? App.identity.sha256 : qsTr("no certificate yet")
                            font.family: App.monoFontFamily
                            color: Theme.text
                            wrapMode: Text.WrapAnywhere
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Button { text: qsTr("Copy"); enabled: App.identity.ready; onClicked: App.copyText(App.identity.sha256) }
                            Button { text: qsTr("Regenerate certificate"); onClicked: App.identity.regenerateCertificate() }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Regenerating changes this computer's fingerprint: every peer will ask to trust it again.")
                            color: Theme.muted
                            font.pixelSize: Theme.smallFontSize
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Label {
                    visible: App.settings.cryptoEnabled && App.identity.sha1.length > 0
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    text: qsTr("SHA1 (old peers only): %1").arg(App.identity.sha1)
                    font.family: App.monoFontFamily
                    font.pixelSize: 11
                    color: Theme.muted
                    wrapMode: Text.WrapAnywhere
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                spacing: 0
                SectionTitle { text: qsTr("Policy"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("Require client certificates")
                    hint: qsTr("Server role. Clients must present a trusted certificate.")
                    divider: false
                    Switch { checked: App.settings.requireClientCertificate; onToggled: App.settings.requireClientCertificate = checked }
                }
                SettingRow {
                    label: qsTr("Unknown peer fingerprint")
                    Label { text: qsTr("always ask, here and in the tray"); color: Theme.muted }
                }
                SettingRow {
                    label: qsTr("Certificate")
                    stacked: true
                    Label { Layout.fillWidth: true; text: App.identity.certificatePath; font.family: App.monoFontFamily; font.pixelSize: Theme.smallFontSize; color: Theme.muted; wrapMode: Text.WrapAnywhere }
                }
                SettingRow {
                    label: qsTr("Plaintext fallback")
                    Label { text: qsTr("never"); color: Theme.muted }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            padding: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 8
                SectionTitle { text: qsTr("Trusted peers") }
                Item { Layout.fillWidth: true }
                Label { text: qsTr("Clients are checked while this computer is the server, servers while it is a client"); color: Theme.muted; elide: Text.ElideRight }
            }

            Repeater {
                model: App.identity.trustedClients.concat(App.identity.trustedServers)
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 10
                    spacing: 12
                    Label { text: modelData.name.length > 0 ? modelData.name : qsTr("unknown name"); font.family: App.monoFontFamily; font.weight: Font.Medium; color: Theme.text; Layout.preferredWidth: 160; elide: Text.ElideRight }
                    Label { text: modelData.role; color: Theme.text; Layout.preferredWidth: 60 }
                    Label { Layout.fillWidth: true; text: modelData.sha256; font.family: App.monoFontFamily; color: Theme.muted; elide: Text.ElideMiddle }
                    Button { text: qsTr("Forget"); flat: true; onClicked: App.identity.forget(modelData.role === "client", modelData.sha256) }
                }
            }
            Label {
                visible: App.identity.trustedClients.length + App.identity.trustedServers.length === 0
                Layout.margins: 16
                Layout.topMargin: 0
                text: qsTr("No trusted peers yet. The first connection with a new computer asks you to compare fingerprints.")
                color: Theme.muted
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}
