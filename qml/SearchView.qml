import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

// Search: qBittorrent's search plugins, run by the engine. Results come in while the plugins
// work, best seeded first; "Add" goes through the usual add sheet.
ColumnLayout {
    id: view
    spacing: 12
    signal pluginsRequested()

    function focusField() { field.forceActiveFocus(); }
    function go() {
        if (field.text.trim() !== "")
            App.search(field.text, Logic.categories[category.currentIndex].id);
    }

    readonly property int enabledPlugins: App.plugins.filter(p => p.enabled).length

    RowLayout {
        spacing: 10
        Label {
            text: qsTr("Search")
            color: Theme.text
            font.family: Theme.fontUi
            font.pointSize: Theme.headingSize
            font.weight: Font.DemiBold
        }
        Item { Layout.fillWidth: true }
        PillButton {
            iconName: "plugin"
            text: view.enabledPlugins > 0 ? qsTr("Plugins (%1)").arg(view.enabledPlugins) : qsTr("Plugins")
            onClicked: view.pluginsRequested()
        }
    }

    RowLayout {
        spacing: 8
        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Find torrents on the sites your plugins search")
            leftPadding: 34
            color: Theme.text
            font.family: Theme.fontUi
            selectByMouse: true
            onAccepted: view.go()
            Keys.onEscapePressed: { if (App.searchState === "running") App.stopSearch(); else text = ""; }
            background: Rectangle {
                implicitHeight: 38
                color: Theme.surface
                radius: Theme.radius
                border.color: field.activeFocus ? Theme.accent : Qt.alpha(Theme.border, 0.6)
                Impl.IconImage {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 10
                    source: "qrc:/qt/qml/Uqbit/icons/search.svg"
                    sourceSize: Qt.size(16, 16)
                    color: Theme.textMuted
                }
            }
        }
        ComboBox {
            id: category
            model: Logic.categories.map(c => c.label)
            font.family: Theme.fontUi
            implicitHeight: 38
            Layout.preferredWidth: 140
            background: Rectangle {
                color: Theme.surface
                radius: Theme.radius
                border.color: category.visualFocus ? Theme.accent : Qt.alpha(Theme.border, 0.6)
            }
            contentItem: Label {
                leftPadding: 12
                text: category.displayText
                color: Theme.text
                font.family: Theme.fontUi
                font.pointSize: Theme.bodySize
                verticalAlignment: Text.AlignVCenter
            }
        }
        PillButton {
            primary: App.searchState !== "running"
            iconName: App.searchState === "running" ? "stop" : "search"
            text: App.searchState === "running" ? qsTr("Stop") : qsTr("Search")
            enabled: App.status === "ready" && (App.searchState === "running" || field.text.trim() !== "")
            onClicked: App.searchState === "running" ? App.stopSearch() : view.go()
        }
    }

    Label {
        Layout.fillWidth: true
        visible: App.searchState === "running" || App.searchState === "done"
        text: App.searchState === "running"
              ? qsTr("Searching… %1 found so far").arg(App.searchTotal)
              : App.searchTotal === 0 ? qsTr("Nothing found") : App.searchTotal > App.searchResults.length
                ? qsTr("%1 found, showing the best seeded %2").arg(App.searchTotal).arg(App.searchResults.length)
                : qsTr("%1 found").arg(App.searchTotal)
        color: Theme.textMuted
        font.family: Theme.fontUi
        font.pointSize: Theme.smallSize
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: results
            anchors.fill: parent
            visible: count > 0
            clip: true
            spacing: 6
            model: App.searchResults
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width - 4
                height: rowLayout.implicitHeight + 18
                radius: Theme.radius
                color: hover.hovered ? Qt.alpha(Theme.text, 0.05) : Theme.surface
                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                border.color: Qt.alpha(Theme.border, 0.5)
                HoverHandler { id: hover }
                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.margins: 9
                    anchors.leftMargin: 14
                    spacing: 12
                    Rectangle {
                        // How healthy: green with seeds, amber with a few, grey with none.
                        implicitWidth: 8
                        implicitHeight: 8
                        radius: 4
                        color: row.modelData.seeds >= 10 ? Theme.success : row.modelData.seeds > 0 ? Theme.warning : Theme.textMuted
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: Theme.text
                            font.family: Theme.fontUi
                            font.pointSize: Theme.bodySize
                        }
                        Label {
                            Layout.fillWidth: true
                            text: Logic.resultLine(row.modelData)
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: Theme.textMuted
                            font.family: Theme.fontUi
                            font.pointSize: Theme.smallSize
                        }
                    }
                    IconButton {
                        size: 32
                        iconName: "globe"
                        tip: qsTr("Open its page")
                        visible: row.modelData.page !== ""
                        onClicked: App.openPage(row.index)
                    }
                    PillButton {
                        iconName: "add"
                        text: qsTr("Add")
                        onClicked: App.addResult(row.index)
                    }
                }
            }
        }

        EmptyState {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 460)
            visible: !results.visible
            busy: App.searchState === "running"
            icon: App.searchState === "error" || App.searchState === "unavailable" ? "error" : view.enabledPlugins === 0 ? "plugin" : "search"
            title: App.searchState === "running" ? qsTr("Searching")
                   : App.searchState === "unavailable" ? qsTr("Search needs Python")
                   : App.searchState === "error" ? qsTr("The search didn't work")
                   : view.enabledPlugins === 0 ? qsTr("Add a search plugin")
                   : App.searchState === "done" ? qsTr("Nothing found") : qsTr("Search for torrents")
            detail: App.searchState === "unavailable" || App.searchState === "error" ? App.searchError
                    : App.searchState === "running" ? qsTr("Your plugins are asking their sites.")
                    : view.enabledPlugins === 0 ? qsTr("Plugins teach uQbit where to search. qBittorrent's own plugins are one click away.")
                    : App.searchState === "done" ? qsTr("Try fewer or other words, or another category.")
                    : qsTr("Type what you're looking for. %1 plugins will search for it.").arg(view.enabledPlugins)
            PillButton {
                visible: view.enabledPlugins === 0 && App.searchState !== "running"
                primary: true
                iconName: "plugin"
                text: qsTr("Choose plugins")
                onClicked: view.pluginsRequested()
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: App.protection.required === true
        text: qsTr("Searches go straight to the sites, like a web browser does. Only the torrents themselves are held to the VPN.")
        wrapMode: Text.Wrap
        color: Theme.textMuted
        font.family: Theme.fontUi
        font.pointSize: Theme.smallSize
    }
}
