// SPDX-License-Identifier: MIT
//
// A vertical radio group. Used for the language selector and the Vietnamese
// input method: both are short, mutually exclusive lists where laid-out options
// with a one-line hint are faster to read than a dropdown.

import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    /// [{ value: "telex", label: "Telex", hint: "aa → â", badge: "VN" }, …]
    property var options: []
    property string current: ""
    signal selected(string value)

    Layout.fillWidth: true
    spacing: Theme.gapSmall

    Repeater {
        model: root.options

        delegate: Rectangle {
            id: tile
            required property var modelData

            readonly property bool isCurrent: modelData.value === root.current
            readonly property string hint: modelData.hint !== undefined ? modelData.hint : ""
            readonly property string badge: modelData.badge !== undefined ? modelData.badge : ""

            Layout.fillWidth: true
            implicitHeight: row.implicitHeight + 20
            radius: Theme.radiusSmall
            color: tile.isCurrent ? Theme.accentSoft : Theme.surfaceRaised
            border.width: 1
            border.color: tile.isCurrent ? Theme.accent : Theme.borderSubtle

            Behavior on color { ColorAnimation { duration: 120 } }

            RowLayout {
                id: row
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    width: 18
                    height: 18
                    radius: 9
                    color: "transparent"
                    border.width: 2
                    border.color: tile.isCurrent ? Theme.accent : Theme.border

                    Rectangle {
                        anchors.centerIn: parent
                        width: 8
                        height: 8
                        radius: 4
                        color: Theme.accent
                        visible: tile.isCurrent
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: tile.modelData.label
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeBody
                    }

                    Text {
                        visible: tile.hint.length > 0
                        Layout.fillWidth: true
                        text: tile.hint
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    visible: tile.badge.length > 0
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: badgeText.implicitWidth + 14
                    implicitHeight: 22
                    radius: 6
                    color: tile.isCurrent ? Theme.accent : Theme.border

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: tile.badge
                        color: tile.isCurrent ? Theme.accentText : Theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.selected(tile.modelData.value)
            }
        }
    }
}
