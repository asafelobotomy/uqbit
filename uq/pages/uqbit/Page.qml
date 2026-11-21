// uQbit's page in uQ Settings (docs/settings-pages.md in the uQ repository), laid out like
// Settings' own pages (uq_api = 2). Every read and change is a `uqbit` command declared in
// page.toml; all text goes through UqLabel. The pure parts live in logic.js.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.uq.settings
import "logic.js" as Logic

SettingsPage {
    id: page

    property var ctx
    property string highlight
    /// What the last reads said: null until the first answer.
    property var config: null
    property var defaults: null
    property var engine: Logic.engineLine(null, null)
    property string configError: ""
    /// The key whose change is running ("" when none), and what each change said, by key.
    property string workingKey: ""
    property var notes: ({})
    property bool dead: false

    pageId: "uqbit"

    Component.onDestruction: dead = true

    onCtxChanged: {
        if (!ctx)
            return;
        ctx.command.watch("config", r => {
            if (page.dead)
                return;
            const c = Logic.parseConfig(r);
            page.configError = c.ok ? "" : c.error;
            if (c.ok)
                page.config = c.values;
        });
        ctx.command.watch("default-status", r => {
            if (!page.dead)
                page.defaults = Logic.parseDefault(r);
        });
        readEngine();
    }

    /// The engine has no file to watch, so it's read now and every few seconds while the page
    /// is open.
    Timer {
        interval: 5000
        repeat: true
        running: !!page.ctx && !page.dead
        onTriggered: page.readEngine()
    }

    function readEngine() {
        ctx.command.run("engine-status", [], s => {
            if (page.dead)
                return;
            if (s && s.output && s.output.running)
                ctx.command.run("list", [], l => {
                    if (!page.dead)
                        page.engine = Logic.engineLine(s, l);
                });
            else
                page.engine = Logic.engineLine(s, null);
        });
    }

    function value(key) {
        return page.config ? page.config[key] : undefined;
    }

    function note(key, text) {
        const n = Object.assign({}, page.notes);
        n[key] = text;
        page.notes = n;
    }

    /// One change; the watched read shows the new value when the file changes.
    function setKey(key, value) {
        if (!ctx || page.workingKey !== "")
            return;
        page.workingKey = key;
        note(key, "");
        const done = r => {
            if (page.dead)
                return;
            page.workingKey = "";
            if (r.error)
                note(key, Logic.failure(r));
            if (key === "start_on_login")
                readEngine();
        };
        if (value === "")
            ctx.command.run("config-reset", [key], done);
        else
            ctx.command.run("config-set", [key, value], done);
    }

    function action(id, key) {
        page.workingKey = key;
        note(key, "");
        ctx.command.run(id, [], r => {
            if (page.dead)
                return;
            page.workingKey = "";
            if (r.error)
                note(key, Logic.failure(r));
            if (key === "engine")
                readEngine();
        });
    }

    readonly property color errorColor: (page.ctx && page.ctx.theme.colors && page.ctx.theme.colors.error) || "red"
    readonly property color mutedColor: (page.ctx && page.ctx.theme.colors && page.ctx.theme.colors.text_muted) || "gray"
    readonly property bool ready: page.config !== null && page.workingKey === ""

    banners: [
        ErrorCard {
            visible: page.configError !== ""
            title: "Couldn't read uQbit's settings"
            detail: page.configError
        }
    ]

    SettingsTab {
        title: "Torrents"
        iconName: "io.github.asafelobotomy.uqbit"

        SettingsCard {
            title: "uQbit"
            iconName: "io.github.asafelobotomy.uqbit"
            help: "uQbit downloads and shares torrents with qBittorrent's engine running in the background."

            SettingRow {
                settingId: "default"
                label: page.defaults && page.defaults.ok ? page.defaults.summary : "Checking…"
                highlight: page.highlight
                QQC2.Button {
                    text: "Make uQbit the default"
                    visible: !!page.defaults && page.defaults.ok && !page.defaults.isDefault
                    enabled: page.workingKey === ""
                    onClicked: page.action("default-set", "default")
                }
                QQC2.Button {
                    text: "Open uQbit"
                    onClicked: page.ctx.openApp("io.github.asafelobotomy.uqbit.desktop")
                }
            }
            UqLabel {
                visible: text !== ""
                text: page.notes["default"] || ""
                color: page.errorColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        SettingsCard {
            title: "Engine"
            iconName: "network-transmit-receive"

            SettingRow {
                settingId: "engine"
                label: page.engine.line
                highlight: page.highlight
                QQC2.Button {
                    text: "Start"
                    visible: page.engine.installed && !page.engine.running
                    enabled: page.workingKey === ""
                    onClicked: page.action("engine-start", "engine")
                }
                QQC2.Button {
                    text: "Stop"
                    visible: page.engine.running
                    enabled: page.workingKey === ""
                    onClicked: page.action("engine-stop", "engine")
                }
            }
            UqLabel {
                visible: text !== ""
                text: page.notes["engine"] || ""
                color: page.errorColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            SwitchRow {
                settingId: "keep_running"
                label: "Keep downloading and seeding after closing uQbit"
                highlight: page.highlight
                value: page.value("keep_running") === true
                enabled: page.ready
                onChosen: v => page.setKey("keep_running", Logic.boolArg(v))
            }
            SwitchRow {
                settingId: "start_on_login"
                label: "Start the engine when you log in"
                highlight: page.highlight
                value: page.value("start_on_login") === true
                enabled: page.ready && page.engine.canLogin
                onChosen: v => page.setKey("start_on_login", Logic.boolArg(v))
                HelpTip { tip: "Torrents resume seeding without opening uQbit. Uses systemd's user service uqbit-engine." }
            }
            UqLabel {
                visible: text !== ""
                text: page.notes["start_on_login"] || page.notes["keep_running"] || ""
                color: page.errorColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        SettingsCard {
            title: "Adding torrents"
            iconName: "folder-download"

            SettingRow {
                settingId: "download_folder"
                label: "Save new torrents in"
                highlight: page.highlight
                QQC2.TextField {
                    id: folder
                    placeholderText: "~/Downloads"
                    text: page.value("download_folder") || ""
                    Layout.preferredWidth: 200
                    onAccepted: saveFolder.clicked()
                }
                QQC2.Button {
                    id: saveFolder
                    text: "Save"
                    enabled: page.workingKey === "" && Logic.cleanFolder(folder.text) !== null
                             && Logic.cleanFolder(folder.text) !== page.value("download_folder")
                    onClicked: page.setKey("download_folder", Logic.cleanFolder(folder.text))
                }
            }
            UqLabel {
                visible: text !== ""
                text: page.notes["download_folder"] || ""
                color: page.errorColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            SwitchRow {
                settingId: "ask_before_adding"
                label: "Show the torrent and its folder before adding it"
                highlight: page.highlight
                value: page.value("ask_before_adding") === true
                enabled: page.ready
                onChosen: v => page.setKey("ask_before_adding", Logic.boolArg(v))
            }
            SwitchRow {
                settingId: "start_added"
                label: "Start new torrents right away"
                highlight: page.highlight
                value: page.value("start_added") === true
                enabled: page.ready
                onChosen: v => page.setKey("start_added", Logic.boolArg(v))
            }
            SwitchRow {
                settingId: "notify_finished"
                label: "Notify me when a download finishes"
                highlight: page.highlight
                value: page.value("notify_finished") === true
                enabled: page.ready
                onChosen: v => page.setKey("notify_finished", Logic.boolArg(v))
            }
        }

        SettingsCard {
            title: "Speed and sharing"
            iconName: "speedometer"
            help: "0 means no limit."

            Repeater {
                model: [
                    { key: "download_limit", label: "Download limit (KiB/s)", max: 1000000 },
                    { key: "upload_limit", label: "Upload limit (KiB/s)", max: 1000000 },
                    { key: "seed_ratio", label: "Stop seeding at ratio", max: 100 }
                ]
                delegate: ColumnLayout {
                    id: numberRow
                    required property var modelData
                    readonly property string current: String(page.value(modelData.key) === undefined ? "" : page.value(modelData.key))
                    readonly property var typed: Logic.cleanNumber(field.text, modelData.max)
                    Layout.fillWidth: true
                    SettingRow {
                        Layout.fillWidth: true
                        settingId: numberRow.modelData.key
                        label: numberRow.modelData.label
                        highlight: page.highlight
                        QQC2.TextField {
                            id: field
                            text: numberRow.current
                            placeholderText: "0"
                            inputMethodHints: Qt.ImhDigitsOnly
                            Layout.preferredWidth: 100
                            onAccepted: save.clicked()
                        }
                        QQC2.Button {
                            id: save
                            text: "Save"
                            enabled: page.workingKey === "" && numberRow.typed !== null && numberRow.typed !== numberRow.current
                            onClicked: page.setKey(numberRow.modelData.key, numberRow.typed)
                        }
                    }
                    UqLabel {
                        visible: text !== ""
                        text: page.notes[numberRow.modelData.key] || ""
                        color: page.errorColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
            UqLabel {
                text: "Now: down " + Logic.limitLabel(page.value("download_limit")) + ", up "
                      + Logic.limitLabel(page.value("upload_limit")) + ", "
                      + (Number(page.value("seed_ratio")) > 0 ? "seeding stops at ratio " + page.value("seed_ratio") : "seeding never stops by itself") + "."
                color: page.mutedColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }
}
