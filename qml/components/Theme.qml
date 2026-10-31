pragma Singleton

// SPDX-License-Identifier: MIT

import QtQuick

QtObject {
    readonly property SystemPalette systemPalette: SystemPalette {}
    readonly property bool dark: systemPalette.window.hsvValue < 0.48

    // Cyberghost/Omarchy's restrained terminal grammar, adapted to Qt Controls.
    readonly property string fontFamily: Qt.platform.os === "windows" ? "Consolas" : "monospace"
    readonly property color background: dark ? "#101010" : "#f2f2ee"
    readonly property color rail: dark ? "#101010" : "#f2f2ee"
    readonly property color surface: dark ? "#171717" : "#fafaf7"
    readonly property color surfaceRaised: dark ? "#292929" : "#e2e2dc"
    readonly property color outputSurface: dark ? "#101010" : "#f2f2ee"
    readonly property color border: dark ? "#686868" : "#777770"
    readonly property color borderSubtle: dark ? "#383838" : "#cdcdc5"

    readonly property color textPrimary: dark ? "#e4e4df" : "#20201e"
    readonly property color textSecondary: dark ? "#b7b7af" : "#4c4c46"
    readonly property color textMuted: dark ? "#999991" : "#62625a"

    readonly property color accent: textPrimary
    readonly property color accentStrong: dark ? "#c4c4bb" : "#393933"
    readonly property color accentSoft: surfaceRaised
    readonly property color accentText: background

    readonly property color success: dark ? "#ffcf36" : "#795900"
    readonly property color warning: dark ? "#e9ba68" : "#785112"
    readonly property color danger: dark ? "#f29b91" : "#a33025"

    readonly property int radius: 0
    readonly property int radiusSmall: 0
    readonly property int gap: 16
    readonly property int gapSmall: 8
    readonly property int padding: 16

    readonly property int fontSizeTitle: 22
    readonly property int fontSizeSection: 12
    readonly property int fontSizeBody: 14
    readonly property int fontSizeSmall: 12

    function statusColor(state) {
        switch (state) {
        case "running": return success
        case "starting": return warning
        case "failed": return danger
        default: return textMuted
        }
    }
}
