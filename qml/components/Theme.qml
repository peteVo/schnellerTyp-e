pragma Singleton

// SPDX-License-Identifier: MIT
//
// One place for every colour, radius and spacing value in the UI.
// schnellerTyp-e is dark-mode only by design: it is a background utility whose
// window is opened for a few seconds at a time, and a single well-tuned palette
// reads better than two half-tuned ones.

import QtQuick

QtObject {
    // --- surfaces -----------------------------------------------------------
    readonly property color background:    "#18181b"   // zinc-900
    readonly property color surface:       "#27272a"   // zinc-800
    readonly property color surfaceRaised: "#323238"
    readonly property color border:        "#3f3f46"   // zinc-700
    readonly property color borderSubtle:  "#2f2f35"

    // --- text ---------------------------------------------------------------
    readonly property color textPrimary:   "#fafafa"   // zinc-50
    readonly property color textSecondary: "#a1a1aa"   // zinc-400
    readonly property color textMuted:     "#71717a"   // zinc-500

    // --- accent -------------------------------------------------------------
    readonly property color accent:        "#8b5cf6"   // violet-500
    readonly property color accentStrong:  "#7c3aed"   // violet-600
    readonly property color accentSoft:    "#312e58"
    // Named accentText rather than onAccent: a QML property whose name starts
    // with "on" followed by a capital is parsed as a signal handler.
    readonly property color accentText:    "#ffffff"

    // --- semantic -----------------------------------------------------------
    readonly property color success:       "#22c55e"
    readonly property color warning:       "#f59e0b"
    readonly property color danger:        "#ef4444"

    // --- metrics ------------------------------------------------------------
    readonly property int radius:      12
    readonly property int radiusSmall: 8
    readonly property int gap:         16
    readonly property int gapSmall:    10
    readonly property int padding:     18

    readonly property int fontSizeTitle:   19
    readonly property int fontSizeSection: 12
    readonly property int fontSizeBody:    14
    readonly property int fontSizeSmall:   12

    function statusColor(state) {
        switch (state) {
        case "running": return success;
        case "starting": return warning;
        case "failed": return danger;
        default: return textMuted;
        }
    }
}
