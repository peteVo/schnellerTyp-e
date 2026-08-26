// SPDX-License-Identifier: MIT
//
// A labelled switch. Deliberately not QtQuick.Controls' Switch: the stock
// indicator does not follow a custom palette cleanly and this is twenty lines.

import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string label: ""
    property string description: ""
    property bool checked: false
    property bool enabled: true
    signal toggled(bool value)

    Layout.fillWidth: true
    spacing: Theme.gap
    opacity: root.enabled ? 1.0 : 0.45

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text {
            Layout.fillWidth: true
            text: root.label
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeBody
        }

        Text {
            visible: root.description.length > 0
            Layout.fillWidth: true
            text: root.description
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }

    Rectangle {
        id: track
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: 46
        implicitHeight: 26
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.border
        border.width: 1
        border.color: root.checked ? Theme.accentStrong : Theme.border

        Behavior on color { ColorAnimation { duration: 130 } }

        Rectangle {
            width: 20
            height: 20
            radius: height / 2
            color: root.checked ? Theme.accentText : Theme.textSecondary
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? parent.width - width - 3 : 3

            Behavior on x { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: 130 } }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            enabled: root.enabled
            onClicked: root.toggled(!root.checked)
        }
    }
}
