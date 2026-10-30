pragma Singleton

// SPDX-License-Identifier: MIT

import QtQuick

QtObject {
    readonly property SystemPalette systemPalette: SystemPalette {}
    readonly property bool dark: systemPalette.window.hsvValue < 0.48

    // A warm paper surface in daylight and a deep green-black surface at night.
    readonly property color background: dark ? "#14211e" : "#f5f2e9"
    readonly property color rail: dark ? "#101b18" : "#ede9df"
    readonly property color surface: dark ? "#1b2b27" : "#fffdf8"
    readonly property color surfaceRaised: dark ? "#253833" : "#e9e5da"
    readonly property color outputSurface: dark ? "#192a25" : "#eeece3"
    readonly property color border: dark ? "#3d514a" : "#c9c8bb"
    readonly property color borderSubtle: dark ? "#2a3c36" : "#ddd9cd"

    readonly property color textPrimary: dark ? "#edf4ee" : "#1d302b"
    readonly property color textSecondary: dark ? "#b5c8bf" : "#4a6058"
    readonly property color textMuted: dark ? "#91a69c" : "#64746a"

    readonly property color accent: dark ? "#a5e5d3" : "#006d62"
    readonly property color accentStrong: dark ? "#80cbb7" : "#00574f"
    readonly property color accentSoft: dark ? "#263f38" : "#dcebe3"
    readonly property color accentText: dark ? "#10221d" : "#ffffff"

    readonly property color success: dark ? "#8dd2a4" : "#286c42"
    readonly property color warning: dark ? "#f0c77b" : "#805914"
    readonly property color danger: dark ? "#f29e91" : "#a13b30"

    readonly property int radius: 12
    readonly property int radiusSmall: 7
    readonly property int gap: 18
    readonly property int gapSmall: 10
    readonly property int padding: 20

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
