// One folder setting on uQbit's uQ Settings page: a path field with Save (and Turn off when the
// setting can be empty). Changes go through the page's setKey().
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
    property string placeholder: "~/Downloads"
    /// Empty turns the setting off.
    property bool optional: false
    property string tip

    readonly property string current: page.value(key) || ""
    readonly property var typed: optional ? Logic.optionalFolder(field.text) : Logic.cleanFolder(field.text)
    Layout.fillWidth: true
    spacing: 2

    SettingRow {
        Layout.fillWidth: true
        settingId: row.key
        label: row.label
        highlight: row.page.highlight
        QQC2.TextField {
            id: field
            objectName: row.key + "-field"
            placeholderText: row.placeholder
            text: row.current
            Layout.preferredWidth: 200
            onAccepted: save.clicked()
        }
        QQC2.Button {
            id: save
            objectName: row.key + "-save"
            text: "Save"
            enabled: row.page.workingKey === "" && row.typed !== null && row.typed !== row.current
            onClicked: row.page.setKey(row.key, row.typed)
        }
        QQC2.Button {
            objectName: row.key + "-off"
            text: "Turn off"
            visible: row.optional && row.current !== ""
            enabled: row.page.workingKey === ""
            onClicked: {
                field.text = "";
                row.page.setKey(row.key, "");
            }
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
