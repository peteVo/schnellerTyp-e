// SPDX-License-Identifier: MIT
//
// The settings window.
//
// It is a panel, not the application: closing it hides it and schnellerTyp-e
// keeps running in the tray. Everything it shows is a binding onto the App
// singleton (src/app/AppController.cpp), so the window and the tray can never
// disagree about state.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SchnellerTypE.Backend
import SchnellerTypE.Components

ApplicationWindow {
    id: window

    width: 520
    height: 760
    minimumWidth: 460
    minimumHeight: 560
    visible: false
    title: qsTr("schnellerTyp-e")
    color: Theme.background

    function reveal() {
        window.show();
        window.raise();
        window.requestActivate();
    }

    Connections {
        target: App
        function onSettingsWindowRequested() { window.reveal(); }
    }

    // ------------------------------------------------------------------ header
    header: Rectangle {
        implicitHeight: 96
        color: Theme.background

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 22
            spacing: Theme.gap

            BadgeTile {
                text: App.trayBadge
                active: App.enabled && App.hookRunning
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("schnellerTyp-e")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: App.enabled && App.hookRunning
                          ? qsTr("Active — %1").arg(App.languageName)
                          : (App.enabled ? App.hookMessage : qsTr("Paused"))
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideRight
                }
            }

            ToggleSwitch {
                Layout.preferredWidth: 46
                Layout.fillWidth: false
                checked: App.enabled
                onToggled: (value) => App.enabled = value
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.borderSubtle
        }
    }

    // -------------------------------------------------------------------- body
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: window.width
            spacing: Theme.gap

            Item { Layout.preferredHeight: 4 }

            // --- language ---------------------------------------------------
            Card {
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                title: qsTr("Language")
                subtitle: qsTr("Left-click the tray icon to pause, middle-click to switch.")

                ChoiceRow {
                    current: App.languageId
                    options: App.languages.map(function (entry) {
                        return { value: entry.id, label: entry.name, badge: entry.badge };
                    })
                    onSelected: (value) => App.languageId = value
                }
            }

            // --- Vietnamese options -----------------------------------------
            Card {
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                visible: App.languageId === "vi"
                title: qsTr("Vietnamese")

                ChoiceRow {
                    current: App.vnMethod
                    options: [
                        { value: "telex", label: qsTr("Telex"),
                          hint: "aa → â   aw → ă   dd → đ   s f r x j" },
                        { value: "vni", label: qsTr("VNI"),
                          hint: "a6 → â   a8 → ă   d9 → đ   1 2 3 4 5" }
                    ]
                    onSelected: (value) => App.vnMethod = value
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                ToggleSwitch {
                    label: qsTr("Modern tone placement")
                    description: qsTr("hoà / thuỳ instead of hòa / thùy.")
                    checked: App.vnModernTone
                    onToggled: (value) => App.vnModernTone = value
                }

                ToggleSwitch {
                    label: qsTr("Spelling check")
                    description: qsTr("Refuse a diacritic that would produce an impossible syllable.")
                    checked: App.vnSpellCheck
                    onToggled: (value) => App.vnSpellCheck = value
                }

                ToggleSwitch {
                    label: qsTr("Restore non-Vietnamese words")
                    description: qsTr("Put the original keystrokes back when a finished word is not Vietnamese.")
                    checked: App.vnAutoRestore
                    enabled: App.vnSpellCheck
                    onToggled: (value) => App.vnAutoRestore = value
                }
            }

            // --- German options ---------------------------------------------
            Card {
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                visible: App.languageId === "de"
                title: qsTr("German")

                ToggleSwitch {
                    label: qsTr("Umlauts")
                    description: "ae → ä    oe → ö    ue → ü"
                    checked: App.deUmlauts
                    onToggled: (value) => App.deUmlauts = value
                }

                ToggleSwitch {
                    label: qsTr("Sharp S")
                    description: "ss → ß"
                    checked: App.deSharpS
                    onToggled: (value) => App.deSharpS = value
                }

                ToggleSwitch {
                    label: qsTr("Capital sharp S")
                    description: qsTr("Type SS as ẞ (U+1E9E) rather than ß.")
                    checked: App.deCapitalSharpS
                    enabled: App.deSharpS
                    onToggled: (value) => App.deCapitalSharpS = value
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                ToggleSwitch {
                    label: qsTr("Exception dictionary")
                    description: qsTr("Keep the digraph literal in Dauer, Duell, Wasser, aussehen …")
                    checked: App.deUseExceptions
                    onToggled: (value) => App.deUseExceptions = value
                }

                ToggleSwitch {
                    label: qsTr("Double-key undo")
                    description: qsTr("Press the second key again to get the plain letters back: ue → ü → ue.")
                    checked: App.deDoubleKeyRevert
                    onToggled: (value) => App.deDoubleKeyRevert = value
                }
            }

            // --- status -----------------------------------------------------
            Card {
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                title: qsTr("Status")

                StatusRow {
                    label: qsTr("Keyboard hook")
                    value: App.hookMessage.length > 0 ? App.hookMessage : App.hookState
                    showIndicator: true
                    indicator: Theme.statusColor(App.hookState)
                }

                StatusRow {
                    label: qsTr("Platform")
                    value: App.platformName
                }

                StatusRow {
                    label: qsTr("Injection")
                    value: App.injectorBackend
                           + (App.canSuppress
                              ? qsTr(" — original keystroke suppressed")
                              : qsTr(" — original keystroke erased (X11)"))
                }

                StatusRow {
                    label: qsTr("Permission")
                    value: App.permissionDetail
                    showIndicator: true
                    indicator: App.permissionState === "denied" ? Theme.danger
                             : App.permissionState === "granted" ? Theme.success
                             : Theme.textMuted
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: Theme.gapSmall
                    visible: App.permissionState === "denied"

                    AccentButton {
                        text: qsTr("Grant access")
                        primary: true
                        onClicked: App.requestPermissions()
                    }

                    AccentButton {
                        text: qsTr("Open system settings")
                        enabled: App.permissionActionable
                        onClicked: App.openPermissionSettings()
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            // --- extensibility ----------------------------------------------
            Card {
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                title: qsTr("Custom rules")
                subtitle: qsTr("Drop JSON rule files, keyboard layout overrides and extra German exception words into the config folder, then reload.")

                Text {
                    Layout.fillWidth: true
                    text: App.customRulesSummary
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gapSmall

                    AccentButton {
                        text: qsTr("Open config folder")
                        onClicked: App.openConfigDirectory()
                    }

                    AccentButton {
                        text: qsTr("Reload")
                        primary: true
                        onClicked: App.reloadCustomRules()
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            // --- footer -----------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                Layout.bottomMargin: 22
                spacing: Theme.gapSmall

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Closing this window keeps schnellerTyp-e running in the tray.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                AccentButton {
                    text: qsTr("Quit")
                    onClicked: App.quit()
                }
            }
        }
    }
}
