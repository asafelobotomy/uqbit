import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit

// The search plugins: the installed ones (on/off, remove, update all), qBittorrent's own
// plugins to add with one click, and any other plugin by its https link.
Popup {
    id: sheet
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(600, parent ? parent.width - 40 : 600)
    height: Math.min(640, parent ? parent.height - 40 : 640)
    padding: 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: App.refreshPlugins()

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius + 2
        border.color: Theme.border
    }

    component Heading: Label {
        color: Theme.textMuted
        font.family: Theme.fontUi
        font.pointSize: Theme.smallSize
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Label {
                Layout.fillWidth: true
                text: qsTr("Search plugins")
                color: Theme.text
                font.family: Theme.fontUi
                font.pointSize: Theme.bodySize + 3
                font.weight: Font.DemiBold
            }
            BusyIndicator {
                running: App.pluginsBusy
                visible: running
                implicitWidth: 24
                implicitHeight: 24
            }
            PillButton {
                iconName: "refresh"
                text: qsTr("Update all")
                enabled: App.plugins.length > 0 && !App.pluginsBusy
                onClicked: App.updatePlugins()
            }
            IconButton {
                size: 32
                iconName: "close"
                tip: qsTr("Close")
                onClicked: sheet.close()
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: body
                width: parent.width - 6
                spacing: 8

                Heading { text: qsTr("Installed"); visible: App.plugins.length > 0 }
                Repeater {
                    model: App.plugins
                    delegate: Rectangle {
                        id: p
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: pRow.implicitHeight + 16
                        radius: Theme.radius
                        color: Theme.surfaceAlt
                        RowLayout {
                            id: pRow
                            anchors.fill: parent
                            anchors.margins: 8
                            anchors.leftMargin: 12
                            spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: p.modelData.fullName
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: Theme.text
                                    font.family: Theme.fontUi
                                    font.pointSize: Theme.bodySize
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: p.modelData.imported
                                          ? qsTr("Version %1 · from qBittorrent's plugin folder").arg(p.modelData.version)
                                          : qsTr("Version %1 · %2").arg(p.modelData.version).arg(p.modelData.url.replace(/^https?:\/\//, "").replace(/\/$/, ""))
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: Theme.textMuted
                                    font.family: Theme.fontUi
                                    font.pointSize: Theme.smallSize
                                }
                            }
                            ThemedSwitch {
                                checked: p.modelData.enabled
                                Accessible.name: qsTr("Search with %1").arg(p.modelData.fullName)
                                onToggled: App.enablePlugin(p.modelData.name, checked)
                            }
                            IconButton {
                                size: 32
                                iconName: "remove"
                                tip: qsTr("Remove")
                                onClicked: App.uninstallPlugin(p.modelData.name)
                            }
                        }
                    }
                }

                Heading {
                    Layout.topMargin: 8
                    text: qsTr("qBittorrent's plugins")
                    visible: App.officialPlugins.length > 0
                }
                Repeater {
                    model: App.officialPlugins
                    delegate: Rectangle {
                        id: o
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: oRow.implicitHeight + 16
                        radius: Theme.radius
                        color: "transparent"
                        border.color: Qt.alpha(Theme.border, 0.6)
                        RowLayout {
                            id: oRow
                            anchors.fill: parent
                            anchors.margins: 8
                            anchors.leftMargin: 12
                            spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: o.modelData.title
                                    color: Theme.text
                                    font.family: Theme.fontUi
                                    font.pointSize: Theme.bodySize
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: o.modelData.about
                                    elide: Text.ElideRight
                                    color: Theme.textMuted
                                    font.family: Theme.fontUi
                                    font.pointSize: Theme.smallSize
                                }
                            }
                            PillButton {
                                iconName: "add"
                                text: qsTr("Add")
                                enabled: !App.pluginsBusy
                                onClicked: App.installPlugin(o.modelData.name)
                            }
                        }
                    }
                }

                Heading { Layout.topMargin: 8; text: qsTr("Another plugin") }
                RowLayout {
                    spacing: 8
                    TextField {
                        id: url
                        Layout.fillWidth: true
                        placeholderText: "https://…/plugin.py"
                        color: Theme.text
                        font.family: Theme.fontUi
                        selectByMouse: true
                        onAccepted: if (acceptable()) { App.installPlugin(text); text = ""; }
                        function acceptable() { return /^https:\/\/\S+\.py$/.test(text.trim()); }
                        background: Rectangle {
                            implicitHeight: 36
                            color: Theme.bg
                            radius: Theme.radius
                            border.color: url.activeFocus ? Theme.accent : Qt.alpha(Theme.border, 0.6)
                        }
                    }
                    PillButton {
                        text: qsTr("Install")
                        enabled: url.acceptable() && !App.pluginsBusy
                        onClicked: { App.installPlugin(url.text); url.text = ""; }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: (App.settings.import_plugins
                           ? qsTr("Plugins in qBittorrent's plugin folder (put there by qBittorrent or qbit-plugin-dl) show up here by themselves. ")
                           : "")
                          + qsTr("A plugin is a Python program that the engine runs to search a site. Install only plugins you trust.")
                    wrapMode: Text.Wrap
                    color: Theme.textMuted
                    font.family: Theme.fontUi
                    font.pointSize: Theme.smallSize
                }
            }
        }
    }
}
