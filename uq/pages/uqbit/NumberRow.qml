// One whole-number setting on uQbit's uQ Settings page: a field with Save, and what a failed
// change said under it. Changes go through the page's setKey().
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.uq.settings
import "logic.js" as Logic

ColumnLayout {
    id: row
    property var page
    property string key
    property string label
    property int max: 1000000
    property string tip
    /// Words after the field, e.g. what the number means now.
    property string after

    readonly property string current: String(page.value(key) === undefined ? "" : page.value(key))
    readonly property var typed: Logic.cleanNumber(field.text, max)
    Layout.fillWidth: true
    spacing: 2

    SettingRow {
        Layout.fillWidth: true
        settingId: row.key
        label: row.label
        highlight: row.page.highlight
        UqLabel {
            visible: text !== ""
            text: row.after
            color: row.page.mutedColor
        }
        QQC2.TextField {
            id: field
            objectName: row.key + "-field"
            text: row.current
            placeholderText: "0"
            inputMethodHints: Qt.ImhDigitsOnly
            Layout.preferredWidth: 100
            onAccepted: save.clicked()
        }
        QQC2.Button {
            id: save
            objectName: row.key + "-save"
            text: "Save"
            enabled: row.page.workingKey === "" && row.typed !== null && row.typed !== row.current
            onClicked: row.page.setKey(row.key, row.typed)
        }
        HelpTip { tip: row.tip }
    }
    UqLabel {
        visible: text !== ""
        text: row.page.notes[row.key] || ""
        color: row.page.errorColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
