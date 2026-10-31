// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SchnellerTypE.Backend
import SchnellerTypE.Components

ApplicationWindow {
    id: window

    width: 820
    height: 740
    minimumWidth: 640
    minimumHeight: 560
    visible: false
    title: qsTr("schnellerTyp-e")
    color: Theme.background

    font.family: Theme.fontFamily
    font.pixelSize: 13

    property string page: "typing"
    property string previewOutput: ""
    property string literalWordsDraft: App.literalWords
    property bool literalWordsDirty: false

    palette.window: Theme.background
    palette.windowText: Theme.textPrimary
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceRaised
    palette.text: Theme.textPrimary
    palette.button: Theme.surface
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.placeholderText: Theme.textMuted

    function reveal() {
        window.show()
        window.raise()
        window.requestActivate()
    }

    function updatePreview() {
        previewOutput = App.previewTyping(rawInput.text)
    }

    function revealFocusedControl() {
        const item = window.activeFocusItem
        const view = window.page === "typing" ? typingScroll
                   : window.page === "preferences" ? preferencesScroll : setupScroll
        const flick = view.contentItem
        if (!item || !flick || flick.contentY === undefined)
            return
        let parentItem = item
        while (parentItem && parentItem !== flick.contentItem)
            parentItem = parentItem.parent
        if (!parentItem)
            return
        const top = item.mapToItem(flick.contentItem, 0, 0).y
        const bottom = top + item.height
        const target = top < flick.contentY ? top
                     : bottom > flick.contentY + flick.height ? bottom - flick.height : flick.contentY
        flick.contentY = Math.max(0, Math.min(target, Math.max(0, flick.contentHeight - flick.height)))
    }

    onActiveFocusItemChanged: Qt.callLater(window.revealFocusedControl)

    Connections {
        target: App
        function onSettingsWindowRequested() { window.reveal() }
        function onLanguageChanged() { window.updatePreview() }
        function onVnOptionsChanged() { window.updatePreview() }
        function onDeOptionsChanged() { window.updatePreview() }
        function onAutoOptionsChanged() {
            if (!window.literalWordsDirty)
                window.literalWordsDraft = App.literalWords
            window.updatePreview()
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        color: Theme.background
        border.color: Theme.border
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 5
                TerminalLabel {
                    Layout.fillWidth: true
                    text: qsTr("schnellerTyp-e")
                    font.pixelSize: 21
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                TerminalLabel {
                    Layout.fillWidth: true
                    text: App.enabled && App.hookRunning
                          ? qsTr("Ready · %1").arg(App.languageName)
                          : (App.enabled ? App.hookMessage : qsTr("Paused"))
                    color: App.enabled && App.hookRunning ? Theme.success
                           : App.enabled && App.hookState === "failed" ? Theme.danger : Theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
            TerminalSwitch {
                id: enabledSwitch
                text: App.enabled ? qsTr("On") : qsTr("Off")
                checked: App.enabled
                Accessible.name: qsTr("Enable typing assistance")
                Accessible.description: qsTr("Turn automatic typing assistance on or off.")
                onToggled: App.enabled = checked
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSubtle }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            spacing: 6
            TerminalButton {
                id: typingNav
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("Typing")
                checkable: true
                checked: window.page === "typing"
                Accessible.name: qsTr("Typing page")
                onClicked: window.page = "typing"
            }
            TerminalButton {
                id: preferencesNav
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("Preferences")
                checkable: true
                checked: window.page === "preferences"
                Accessible.name: qsTr("Preferences page")
                onClicked: window.page = "preferences"
            }
            TerminalButton {
                id: setupNav
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("Setup")
                checkable: true
                checked: window.page === "setup"
                Accessible.name: qsTr("Setup page")
                onClicked: window.page = "setup"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            StackLayout {
                id: pages
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.page === "typing" ? 0
                              : window.page === "preferences" ? 1 : 2

                // ---------------------------------------------------------------- Typing
                ScrollView {
                    id: typingScroll
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: Math.min(640, typingScroll.availableWidth - 24)
                        x: (typingScroll.availableWidth - width) / 2
                        spacing: 16

                        TerminalLabel {
                            Layout.fillWidth: true
                            text: qsTr("Typing preview")
                            color: Theme.textPrimary
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }
                        TerminalLabel {
                            Layout.fillWidth: true
                            text: qsTr("English / Tiếng Việt / Deutsch")
                            color: Theme.textSecondary
                            font.pixelSize: 15
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            spacing: 12

                            TerminalLabel {
                                text: qsTr("Mode")
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            TerminalCombo {
                                id: modePicker
                                Layout.preferredWidth: Math.min(260, typingScroll.availableWidth - 80)
                                model: App.languages
                                textRole: "name"
                                currentIndex: {
                                    for (let i = 0; i < App.languages.length; ++i)
                                        if (App.languages[i].id === App.languageId) return i
                                    return 0
                                }
                                Accessible.name: qsTr("Typing mode")
                                Accessible.description: qsTr("Choose automatic or a specific language engine.")
                                onActivated: (index) => App.languageId = App.languages[index].id
                                background: Rectangle {
                                    radius: 0
                                    color: Theme.surface
                                    border.color: modePicker.activeFocus ? Theme.accent : Theme.border
                                    border.width: modePicker.activeFocus ? 2 : 1
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                TerminalLabel {
                                    text: qsTr("Try your typing")
                                    color: Theme.textPrimary
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Item { Layout.fillWidth: true }
                                TerminalLabel {
                                    text: qsTr("INPUT")
                                    color: Theme.textMuted
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 1
                                }
                            }

                            TerminalEditor {
                                id: rawInput
                                font.family: Theme.fontFamily
                                Layout.fillWidth: true
                                Layout.preferredHeight: 104
                                text: "hoom nay tooi ddeens New York"
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                Accessible.name: qsTr("Practice input")
                                Accessible.description: qsTr("Type raw keystrokes to see how the selected mode transforms them.")
                                onTextChanged: window.updatePreview()
                                background: Rectangle {
                                    radius: 0
                                    color: Theme.surface
                                    border.color: rawInput.activeFocus ? Theme.accent : Theme.border
                                    border.width: rawInput.activeFocus ? 2 : 1
                                }
                                color: Theme.textPrimary
                                selectionColor: Theme.accent
                                selectedTextColor: Theme.accentText
                                leftPadding: 14
                                rightPadding: 14
                                topPadding: 13
                                bottomPadding: 13
                                font.pixelSize: 16
                                placeholderText: qsTr("Type here…")
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                TerminalLabel {
                                    text: qsTr("Result")
                                    color: Theme.textPrimary
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Item { Layout.fillWidth: true }
                                TerminalLabel {
                                    text: qsTr("LIVE PREVIEW")
                                    color: Theme.accent
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 1
                                }
                            }

                            TerminalEditor {
                                id: outputArea
                                font.family: Theme.fontFamily
                                Layout.fillWidth: true
                                Layout.preferredHeight: 128
                                text: window.previewOutput
                                readOnly: true
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                Accessible.name: qsTr("Practice result")
                                Accessible.description: qsTr("Read-only preview of the transformed text.")
                                background: Rectangle {
                                    radius: 0
                                    color: Theme.outputSurface
                                    border.color: outputArea.activeFocus ? Theme.accent : Theme.borderSubtle
                                    border.width: outputArea.activeFocus ? 2 : 1
                                }
                                color: Theme.textPrimary
                                selectionColor: Theme.accent
                                selectedTextColor: Theme.accentText
                                leftPadding: 14
                                rightPadding: 14
                                topPadding: 13
                                bottomPadding: 13
                                font.pixelSize: 18
                                font.weight: Font.Medium
                                placeholderText: qsTr("Your result appears here")
                            }
                        }

                        TerminalLabel {
                            Layout.fillWidth: true
                            visible: App.languageId === "auto"
                            text: qsTr("The current word can change as you type. Space settles it. Esc restores its original keys.")
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 12 }
                    }
                }

                // ---------------------------------------------------------- Preferences
                ScrollView {
                    id: preferencesScroll
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: Math.min(640, preferencesScroll.availableWidth - 24)
                        x: (preferencesScroll.availableWidth - width) / 2
                        spacing: 18

                        TerminalLabel {
                            Layout.fillWidth: true
                            text: qsTr("Preferences")
                            color: Theme.textPrimary
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                        TerminalLabel {
                            Layout.fillWidth: true
                            text: qsTr("Shape how each writing mode handles your words.")
                            color: Theme.textSecondary
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: App.languageId === "auto" || App.languageId === "vi"
                            spacing: 12

                            TerminalLabel {
                                text: App.languageId === "auto" ? qsTr("Vietnamese in Automatic") : qsTr("Vietnamese")
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            TerminalLabel {
                                Layout.fillWidth: true
                                text: qsTr("Telex or VNI rules used when Automatic finds a valid Vietnamese candidate.")
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            TerminalCombo {
                                id: vnMethodPicker
                                Layout.preferredWidth: 220
                                model: [qsTr("Telex"), qsTr("VNI")]
                                currentIndex: App.vnMethod === "vni" ? 1 : 0
                                Accessible.name: qsTr("Vietnamese input method")
                                onActivated: (index) => App.vnMethod = index === 1 ? "vni" : "telex"
                                background: Rectangle {
                                    radius: 0
                                    color: Theme.surface
                                    border.color: vnMethodPicker.activeFocus ? Theme.accent : Theme.border
                                    border.width: vnMethodPicker.activeFocus ? 2 : 1
                                }
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Modern tone placement")
                                checked: App.vnModernTone
                                Accessible.name: qsTr("Modern Vietnamese tone placement")
                                Accessible.description: qsTr("Use hoà and thuỷ instead of hòa and thủy.")
                                onToggled: App.vnModernTone = checked
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Spelling check")
                                checked: App.vnSpellCheck
                                Accessible.name: qsTr("Vietnamese spelling check")
                                Accessible.description: qsTr("Reject diacritics that create impossible syllables.")
                                onToggled: App.vnSpellCheck = checked
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Restore non-Vietnamese words")
                                checked: App.vnAutoRestore
                                enabled: App.vnSpellCheck
                                Accessible.name: qsTr("Restore non-Vietnamese words")
                                Accessible.description: qsTr("Put original keystrokes back when a completed word is not Vietnamese.")
                                onToggled: App.vnAutoRestore = checked
                            }
                        }

                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSubtle }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: App.languageId === "auto" || App.languageId === "de"
                            spacing: 12

                            TerminalLabel {
                                text: App.languageId === "auto" ? qsTr("German in Automatic") : qsTr("German")
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            TerminalLabel {
                                Layout.fillWidth: true
                                text: qsTr("German typing rules used when Automatic recognizes German.")
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Umlauts")
                                checked: App.deUmlauts
                                Accessible.name: qsTr("German umlauts")
                                Accessible.description: qsTr("Convert ae, oe, and ue to their umlaut forms.")
                                onToggled: App.deUmlauts = checked
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Sharp S")
                                checked: App.deSharpS
                                Accessible.name: qsTr("German sharp S")
                                Accessible.description: qsTr("Convert ss to sharp S.")
                                onToggled: App.deSharpS = checked
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Capital sharp S")
                                checked: App.deCapitalSharpS
                                enabled: App.deSharpS
                                Accessible.name: qsTr("Capital German sharp S")
                                Accessible.description: qsTr("Use ẞ for uppercase sharp S.")
                                onToggled: App.deCapitalSharpS = checked
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Exception dictionary")
                                checked: App.deUseExceptions
                                Accessible.name: qsTr("German exception dictionary")
                                Accessible.description: qsTr("Keep words such as Dauer and Wasser literal.")
                                onToggled: App.deUseExceptions = checked
                            }
                            TerminalSwitch {
                                Layout.fillWidth: true
                                text: qsTr("Double-key undo")
                                checked: App.deDoubleKeyRevert
                                Accessible.name: qsTr("German double-key undo")
                                Accessible.description: qsTr("Type the changed key again to restore the plain letters.")
                                onToggled: App.deDoubleKeyRevert = checked
                            }
                        }

                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSubtle }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: App.languageId === "auto"
                            spacing: 10

                            TerminalLabel {
                                text: qsTr("Always keep literal")
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            TerminalLabel {
                                Layout.fillWidth: true
                                text: qsTr("Words in this list are left as typed, one per line.")
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            TerminalEditor {
                                id: literalWordsInput
                                font.family: Theme.fontFamily
                                Layout.fillWidth: true
                                Layout.preferredHeight: 110
                                text: window.literalWordsDraft
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                Accessible.name: qsTr("Literal words")
                                Accessible.description: qsTr("Enter one word per line that Automatic should always leave unchanged.")
                                onTextChanged: {
                                    window.literalWordsDraft = text
                                    window.literalWordsDirty = text !== App.literalWords
                                }
                                background: Rectangle {
                                    radius: 0
                                    color: Theme.surface
                                    border.color: literalWordsInput.activeFocus ? Theme.accent : Theme.border
                                    border.width: literalWordsInput.activeFocus ? 2 : 1
                                }
                                color: Theme.textPrimary
                                selectionColor: Theme.accent
                                selectedTextColor: Theme.accentText
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 10
                                bottomPadding: 10
                                font.pixelSize: 14
                                placeholderText: qsTr("e.g. a name or technical term")
                            }
                            TerminalButton {
                                id: saveLiteralWords
                                text: qsTr("Save word list")
                                Accessible.name: qsTr("Save literal words")
                                enabled: window.literalWordsDirty
                                onClicked: {
                                    window.literalWordsDirty = false
                                    App.literalWords = window.literalWordsDraft
                                }
                                leftPadding: 14
                                rightPadding: 14
                                topPadding: 9
                                bottomPadding: 9
                                background: Rectangle {
                                    radius: 0
                                    color: saveLiteralWords.down ? Theme.accentStrong
                                          : saveLiteralWords.hovered ? Theme.accentStrong : Theme.accent
                                    border.color: saveLiteralWords.activeFocus ? Theme.textPrimary : "transparent"
                                    border.width: saveLiteralWords.activeFocus ? 2 : 0
                                }
                                contentItem: TerminalLabel {
                                    text: saveLiteralWords.text
                                    color: Theme.accentText
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSubtle }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            TerminalLabel {
                                text: qsTr("Keyboard shortcut")
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            TerminalLabel {
                                Layout.fillWidth: true
                                text: qsTr("Hold the selected modifiers, then release them without pressing another key to cycle modes and pause.")
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            TerminalCombo {
                                id: shortcutPicker
                                Layout.preferredWidth: Math.min(280, preferencesScroll.availableWidth - 8)
                                model: App.cycleChordOptions
                                textRole: "label"
                                currentIndex: {
                                    for (let i = 0; i < App.cycleChordOptions.length; ++i)
                                        if (App.cycleChordOptions[i].value === App.cycleChord) return i
                                    return 0
                                }
                                Accessible.name: qsTr("Mode cycling shortcut")
                                onActivated: (index) => App.cycleChord = App.cycleChordOptions[index].value
                                background: Rectangle {
                                    radius: 0
                                    color: Theme.surface
                                    border.color: shortcutPicker.activeFocus ? Theme.accent : Theme.border
                                    border.width: shortcutPicker.activeFocus ? 2 : 1
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 16 }
                    }
                }

                // ---------------------------------------------------------------- Setup
                ScrollView {
                    id: setupScroll
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: Math.min(640, setupScroll.availableWidth - 24)
                        x: (setupScroll.availableWidth - width) / 2
                        spacing: 18

                        TerminalLabel {
                            Layout.fillWidth: true
                            text: qsTr("Setup")
                            color: Theme.textPrimary
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                        TerminalLabel {
                            Layout.fillWidth: true
                            text: qsTr("Keyboard access and local rule files.")
                            color: Theme.textSecondary
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            TerminalLabel {
                                text: qsTr("Keyboard access")
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TerminalLabel { Layout.preferredWidth: 132; text: qsTr("Hook"); color: Theme.textSecondary }
                                TerminalLabel {
                                    Layout.fillWidth: true
                                    text: App.hookMessage.length > 0 ? App.hookMessage : App.hookState
                                    color: Theme.textPrimary
                                    wrapMode: Text.WordWrap
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TerminalLabel { Layout.preferredWidth: 132; text: qsTr("Platform"); color: Theme.textSecondary }
                                TerminalLabel { Layout.fillWidth: true; text: App.platformName; color: Theme.textPrimary; wrapMode: Text.WordWrap }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TerminalLabel { Layout.preferredWidth: 132; text: qsTr("Permission"); color: Theme.textSecondary }
                                TerminalLabel { Layout.fillWidth: true; text: App.permissionDetail; color: Theme.textPrimary; wrapMode: Text.WordWrap }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                visible: App.permissionState === "denied"
                                TerminalButton {
                                    id: grantAccess
                                    text: qsTr("Grant access")
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 8
                                    bottomPadding: 8
                                    Accessible.name: qsTr("Grant keyboard access")
                                    onClicked: App.requestPermissions()
                                    background: Rectangle { radius: 0; color: grantAccess.hovered ? Theme.accentStrong : Theme.accent; border.color: grantAccess.activeFocus ? Theme.textPrimary : "transparent"; border.width: grantAccess.activeFocus ? 2 : 0 }
                                    contentItem: TerminalLabel { text: grantAccess.text; color: Theme.accentText; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                                TerminalButton {
                                    id: openSettings
                                    text: qsTr("Open system settings")
                                    enabled: App.permissionActionable
                                    Accessible.name: qsTr("Open operating system permission settings")
                                    onClicked: App.openPermissionSettings()
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSubtle }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            TerminalLabel {
                                text: qsTr("Custom rules")
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            TerminalLabel {
                                Layout.fillWidth: true
                                text: qsTr("Add JSON rules, keyboard layout overrides, or German exception words in the config folder.")
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            TerminalLabel {
                                Layout.fillWidth: true
                                text: App.customRulesSummary
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                TerminalButton {
                                    id: openConfig
                                    text: qsTr("Open config folder")
                                    Accessible.name: qsTr("Open configuration folder")
                                    onClicked: App.openConfigDirectory()
                                }
                                TerminalButton {
                                    id: reloadRules
                                    text: qsTr("Reload rules")
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 8
                                    bottomPadding: 8
                                    Accessible.name: qsTr("Reload custom rules")
                                    onClicked: App.reloadCustomRules()
                                    background: Rectangle { radius: 0; color: reloadRules.hovered ? Theme.accentStrong : Theme.accent; border.color: reloadRules.activeFocus ? Theme.textPrimary : "transparent"; border.width: reloadRules.activeFocus ? 2 : 0 }
                                    contentItem: TerminalLabel { text: reloadRules.text; color: Theme.accentText; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: Theme.background

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.borderSubtle
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 0
                anchors.rightMargin: 0
                spacing: 14

                TerminalLabel {
                    Layout.fillWidth: true
                    text: qsTr("Closing this window keeps the app in the tray.")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                TerminalButton {
                    id: quitButton
                    text: qsTr("Quit")
                    leftPadding: 10
                    rightPadding: 10
                    topPadding: 6
                    bottomPadding: 6
                    Accessible.name: qsTr("Quit schnellerTyp-e")
                    onClicked: App.quit()
                    background: Rectangle {
                        radius: 0
                        color: quitButton.hovered ? Theme.surfaceRaised : "transparent"
                        border.color: quitButton.activeFocus ? Theme.accent : Theme.border
                        border.width: 1
                    }
                    contentItem: TerminalLabel {
                        text: quitButton.text
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    component TerminalLabel: Text {
        font.family: Theme.fontFamily
        font.pixelSize: 13
        color: Theme.textPrimary
        textFormat: Text.PlainText
    }

    component TerminalButton: Button {
        id: control
        implicitHeight: 34
        font.family: Theme.fontFamily
        font.pixelSize: 13
        leftPadding: 12
        rightPadding: 12
        topPadding: 8
        bottomPadding: 8
        opacity: enabled ? 1 : 0.45
        background: Rectangle {
            color: control.checked ? Theme.textPrimary
                   : control.down || control.hovered ? Theme.surfaceRaised : "transparent"
            border.color: control.activeFocus ? Theme.textPrimary : Theme.border
            border.width: control.activeFocus ? 2 : 1
        }
        contentItem: TerminalLabel {
            text: control.text
            font: control.font
            color: control.checked ? Theme.background : Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component TerminalSwitch: Switch {
        id: control
        implicitHeight: Math.max(34, contentItem.implicitHeight + topPadding + bottomPadding)
        font.family: Theme.fontFamily
        font.pixelSize: 13
        leftPadding: 0
        rightPadding: 4
        topPadding: 6
        bottomPadding: 6
        opacity: enabled ? 1 : 0.45
        indicator: Rectangle {
            x: control.leftPadding
            y: Math.round((control.height - height) / 2)
            width: 18
            height: 18
            color: control.hovered ? Theme.surfaceRaised : Theme.background
            border.color: control.activeFocus ? Theme.textPrimary : Theme.border
            border.width: control.activeFocus ? 2 : 1
            Rectangle {
                anchors.centerIn: parent
                width: 8
                height: 8
                color: Theme.textPrimary
                visible: control.checked
            }
        }
        contentItem: TerminalLabel {
            text: control.text
            font: control.font
            leftPadding: control.indicator.width + 10
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }

    component TerminalEditor: TextArea {
        activeFocusOnTab: true
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: event => {
            if ((event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)
                && !(event.modifiers & Qt.ControlModifier)) {
                const forward = event.key !== Qt.Key_Backtab && !(event.modifiers & Qt.ShiftModifier)
                const next = nextItemInFocusChain(forward)
                if (next)
                    next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
                event.accepted = true
            }
        }
    }

    component TerminalCombo: ComboBox {
        id: control
        implicitHeight: 34
        palette.light: Theme.textPrimary
        palette.midlight: Theme.accentStrong
        palette.mid: Theme.border
        palette.dark: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 13
        leftPadding: 10
        rightPadding: 28
        topPadding: 7
        bottomPadding: 7
        opacity: enabled ? 1 : 0.45
        background: Rectangle {
            color: control.hovered ? Theme.surfaceRaised : Theme.surface
            border.color: control.activeFocus ? Theme.textPrimary : Theme.border
            border.width: control.activeFocus ? 2 : 1
        }
    }

    Component.onCompleted: updatePreview()
}
