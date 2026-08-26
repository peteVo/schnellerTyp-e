// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string text: ""
    property bool primary: false
    property bool enabled: true
    signal clicked()

    implicitWidth: label.implicitWidth + 32
    implicitHeight: 34
    radius: Theme.radiusSmall
    opacity: root.enabled ? 1.0 : 0.45
    color: root.primary
           ? (mouse.containsPress ? Theme.accentStrong : Theme.accent)
           : (mouse.containsPress ? Theme.border : Theme.surfaceRaised)
    border.width: root.primary ? 0 : 1
    border.color: Theme.border

    Behavior on color { ColorAnimation { duration: 100 } }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.primary ? Theme.accentText : Theme.textPrimary
        font.pixelSize: Theme.fontSizeSmall
        font.bold: root.primary
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
