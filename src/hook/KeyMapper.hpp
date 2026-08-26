// SPDX-License-Identifier: MIT
//
// libuiohook event -> st::KeyEvent.
//
// Suppression has to be decided on EVENT_KEY_PRESSED, before the focused
// application sees the key, and libuiohook only fills in `keychar` on the later
// EVENT_KEY_TYPED. The mapper therefore resolves the character itself from the
// virtual key code plus the modifier mask.
//
// The built-in table is US QWERTY. That is enough for every rule the engines
// care about on most layouts, but not all: on QWERTZ the physical Y and Z keys
// are swapped, and on AZERTY most of the alphabet moves. Rather than duplicate
// libuiohook's per-platform scancode tables, the mapper takes an override map
// that the application loads from config/layouts/*.json — see
// AppController::reloadCustomRules(). Overrides are keyed by libuiohook virtual
// code, so a layout file is a dozen lines.

#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace st {

class KeyMapper {
public:
    struct KeyChars {
        char32_t plain = 0;  ///< character without Shift
        char32_t shift = 0;  ///< character with Shift
    };

    KeyMapper();

    /// Translate one libuiohook EVENT_KEY_PRESSED into a platform-neutral event.
    /// `mask` is uiohook's modifier mask.
    [[nodiscard]] KeyEvent fromPressed(std::uint16_t keycode, std::uint16_t mask) const;

    /// Replace the character produced by one virtual key code.
    void setOverride(std::uint16_t keycode, KeyChars chars);
    void clearOverrides();

    /// Look up a libuiohook VC_ name ("VC_Y", "VC_SEMICOLON") so that layout
    /// files can be written with names instead of magic numbers. Returns 0 for
    /// an unknown name.
    [[nodiscard]] static std::uint16_t keycodeFromName(const std::string& name);

    /// Caps Lock is reported by uiohook only as a lock modifier on some
    /// platforms; the mapper tracks it itself for the rest.
    void setCapsLock(bool on) noexcept { capsLock_ = on; }
    [[nodiscard]] bool capsLock() const noexcept { return capsLock_; }

private:
    [[nodiscard]] KeyChars charsFor(std::uint16_t keycode) const;

    std::unordered_map<std::uint16_t, KeyChars> overrides_;
    bool                                        capsLock_ = false;
};

} // namespace st
