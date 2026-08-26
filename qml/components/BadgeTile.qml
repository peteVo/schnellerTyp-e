// SPDX-License-Identifier: MIT
//
// The large "DE" / "VN" / "OFF" glyph in the window header — the same badge the
// tray icon draws, so the window and the tray always agree at a glance.

import QtQuick

Rectangle {
    id: root

    property string text: "OFF"
    property bool active: false

    implicitWidth: 54
    implicitHeight: 54
    radius: 14
    color: root.active ? Theme.accentStrong : Theme.border

    Behavior on color { ColorAnimation { duration: 160 } }

    Text {
        anchors.centerIn: parent
        text: root.text
        color: root.active ? Theme.accentText : Theme.textSecondary
        font.bold: true
        font.pixelSize: root.text.length > 2 ? 17 : 21
    }
}
