// SPDX-License-Identifier: MIT
//
// A label / value pair with an optional coloured dot. The status card is a
// stack of these.

import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string label: ""
    property string value: ""
    property color indicator: "transparent"
    property bool showIndicator: false

    Layout.fillWidth: true
    spacing: Theme.gap

    Text {
        Layout.preferredWidth: 130
        text: root.label
        color: Theme.textMuted
        font.pixelSize: Theme.fontSizeSmall
    }

    Rectangle {
        visible: root.showIndicator
        Layout.alignment: Qt.AlignVCenter
        width: 8
        height: 8
        radius: 4
        color: root.indicator
    }

    Text {
        Layout.fillWidth: true
        text: root.value
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WordWrap
    }
}
