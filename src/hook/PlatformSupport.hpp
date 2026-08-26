// SPDX-License-Identifier: MIT
//
// The handful of things the hook layer has to ask the operating system, kept
// behind one interface so neither the engines nor the Qt layer ever sees a
// platform header.

#pragma once

#include <string>

namespace st::platform {

enum class SessionType { Unknown, Windows, MacOS, X11, Wayland };

[[nodiscard]] SessionType sessionType();
[[nodiscard]] std::string sessionTypeName();

/// Permission required to observe global keystrokes.
enum class PermissionState {
    Granted,      ///< the hook can run
    Denied,       ///< the user must grant something in system settings
    NotRequired,  ///< nothing to grant on this platform
    Unknown,
};

struct PermissionInfo {
    PermissionState state = PermissionState::Unknown;
    /// One sentence for the status pane, e.g. "Grant Accessibility access to
    /// schnellerTyp-e in System Settings > Privacy & Security."
    std::string detail;
    /// A URL or system settings pane the UI can open, empty when there is none.
    std::string settingsUri;
};

[[nodiscard]] PermissionInfo checkPermissions();

/// Ask the OS to show its permission prompt. On macOS this triggers the
/// Accessibility dialog once per app bundle; elsewhere it is a no-op that
/// returns the current state.
PermissionInfo requestPermissions();

/// True when the platform lets the hook swallow a key event before the focused
/// application sees it.
///
/// This is the single most important platform difference in the whole app:
///   Windows  yes (WH_KEYBOARD_LL returns non-zero)
///   macOS    yes (the CGEventTap can drop the event)
///   X11      NO. XRecord is an observer; the keystroke has already reached the
///            client. schnellerTyp-e compensates by adding one extra backspace
///            to every edit so the character that got through is erased again.
///   Wayland  no global hook at all without a compositor-specific protocol.
[[nodiscard]] bool canSuppressEvents();

} // namespace st::platform
