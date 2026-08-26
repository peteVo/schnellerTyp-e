// SPDX-License-Identifier: MIT
//
// A titled surface. Children are laid out in a column; the card sizes itself to
// them, so the settings page is just a stack of cards.

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string subtitle: ""
    default property alias content: contentColumn.data

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.padding * 2
    color: Theme.surface
    radius: Theme.radius
    border.width: 1
    border.color: Theme.borderSubtle

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.padding
        spacing: Theme.gapSmall

        Text {
            visible: root.title.length > 0
            Layout.fillWidth: true
            text: root.title.toUpperCase()
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSection
            font.letterSpacing: 1.2
            font.bold: true
        }

        Text {
            visible: root.subtitle.length > 0
            Layout.fillWidth: true
            text: root.subtitle
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            spacing: Theme.gapSmall
        }
    }
}
