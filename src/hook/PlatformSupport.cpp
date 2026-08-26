// SPDX-License-Identifier: MIT
#include "hook/PlatformSupport.hpp"

#include <cstdlib>
#include <cstring>

#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
// WIN32_LEAN_AND_MEAN and NOMINMAX come from the target's compile definitions.
#  include <windows.h>
#elif defined(SCHNELLERTYPE_PLATFORM_MACOS)
#  include <ApplicationServices/ApplicationServices.h>
#endif

namespace st::platform {
namespace {

#if defined(SCHNELLERTYPE_PLATFORM_LINUX)
[[nodiscard]] const char* env(const char* name)
{
    const char* v = std::getenv(name);
    return (v != nullptr && *v != '\0') ? v : nullptr;
}
#endif

} // namespace

SessionType sessionType()
{
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
    return SessionType::Windows;
#elif defined(SCHNELLERTYPE_PLATFORM_MACOS)
    return SessionType::MacOS;
#elif defined(SCHNELLERTYPE_PLATFORM_LINUX)
    if (const char* t = env("XDG_SESSION_TYPE")) {
        if (std::strcmp(t, "wayland") == 0)
            return SessionType::Wayland;
        if (std::strcmp(t, "x11") == 0)
            return SessionType::X11;
    }
    if (env("WAYLAND_DISPLAY") != nullptr)
        return SessionType::Wayland;
    if (env("DISPLAY") != nullptr)
        return SessionType::X11;
    return SessionType::Unknown;
#else
    return SessionType::Unknown;
#endif
}

std::string sessionTypeName()
{
    switch (sessionType()) {
    case SessionType::Windows: return "Windows";
    case SessionType::MacOS:   return "macOS";
    case SessionType::X11:     return "X11";
    case SessionType::Wayland: return "Wayland";
    default:                   return "unknown";
    }
}

PermissionInfo checkPermissions()
{
    PermissionInfo info;

#if defined(SCHNELLERTYPE_PLATFORM_MACOS)
    info.state = AXIsProcessTrusted() ? PermissionState::Granted : PermissionState::Denied;
    info.detail =
        info.state == PermissionState::Granted
            ? "Accessibility access granted."
            : "schnellerTyp-e needs Accessibility access to read and inject keystrokes. "
              "Open System Settings > Privacy & Security > Accessibility and enable it.";
    info.settingsUri =
        "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility";

#elif defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
    info.state  = PermissionState::NotRequired;
    info.detail = "No special permission required. Note that keystrokes cannot be "
                  "injected into windows running elevated unless schnellerTyp-e is "
                  "also running elevated.";

#elif defined(SCHNELLERTYPE_PLATFORM_LINUX)
    switch (sessionType()) {
    case SessionType::Wayland:
        info.state  = PermissionState::Denied;
        info.detail = "Wayland does not expose a global keyboard hook. Run the session "
                      "under X11 (or XWayland with a compositor that allows XTest) to "
                      "use schnellerTyp-e.";
        break;
    case SessionType::X11:
        info.state  = PermissionState::Granted;
        info.detail = "X11 session detected. The XTest and XRecord extensions must be "
                      "enabled on the display server (they are by default).";
        break;
    default:
        info.state  = PermissionState::Unknown;
        info.detail = "No display server detected (DISPLAY and WAYLAND_DISPLAY are unset).";
        break;
    }
#else
    info.state  = PermissionState::Unknown;
    info.detail = "Unsupported platform.";
#endif

    return info;
}

PermissionInfo requestPermissions()
{
#if defined(SCHNELLERTYPE_PLATFORM_MACOS)
    // Shows the system prompt the first time; afterwards it is a no-op and the
    // user has to toggle the switch in System Settings themselves.
    const void*   keys[]   = {kAXTrustedCheckOptionPrompt};
    const void*   values[] = {kCFBooleanTrue};
    CFDictionaryRef options =
        CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
#endif
    return checkPermissions();
}

bool canSuppressEvents()
{
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS) || defined(SCHNELLERTYPE_PLATFORM_MACOS)
    return true;
#else
    return false;  // XRecord observes; it cannot consume.
#endif
}

} // namespace st::platform
