// SPDX-License-Identifier: MIT
#include "hook/KeyMapper.hpp"

#include "core/Unicode.hpp"

#include <uiohook.h>

#include <iterator>

namespace st {
namespace {

struct Entry {
    std::uint16_t     keycode;
    const char*       name;
    KeyMapper::KeyChars chars;
};

// US QWERTY. Only the keys that can ever be part of a word, a modifier key of an
// input method, or a word separator are listed; everything else falls through
// to KeyKind::Other, which the engines treat as a boundary.
constexpr Entry kTable[] = {
    {VC_A, "VC_A", {U'a', U'A'}}, {VC_B, "VC_B", {U'b', U'B'}}, {VC_C, "VC_C", {U'c', U'C'}},
    {VC_D, "VC_D", {U'd', U'D'}}, {VC_E, "VC_E", {U'e', U'E'}}, {VC_F, "VC_F", {U'f', U'F'}},
    {VC_G, "VC_G", {U'g', U'G'}}, {VC_H, "VC_H", {U'h', U'H'}}, {VC_I, "VC_I", {U'i', U'I'}},
    {VC_J, "VC_J", {U'j', U'J'}}, {VC_K, "VC_K", {U'k', U'K'}}, {VC_L, "VC_L", {U'l', U'L'}},
    {VC_M, "VC_M", {U'm', U'M'}}, {VC_N, "VC_N", {U'n', U'N'}}, {VC_O, "VC_O", {U'o', U'O'}},
    {VC_P, "VC_P", {U'p', U'P'}}, {VC_Q, "VC_Q", {U'q', U'Q'}}, {VC_R, "VC_R", {U'r', U'R'}},
    {VC_S, "VC_S", {U's', U'S'}}, {VC_T, "VC_T", {U't', U'T'}}, {VC_U, "VC_U", {U'u', U'U'}},
    {VC_V, "VC_V", {U'v', U'V'}}, {VC_W, "VC_W", {U'w', U'W'}}, {VC_X, "VC_X", {U'x', U'X'}},
    {VC_Y, "VC_Y", {U'y', U'Y'}}, {VC_Z, "VC_Z", {U'z', U'Z'}},

    {VC_0, "VC_0", {U'0', U')'}}, {VC_1, "VC_1", {U'1', U'!'}}, {VC_2, "VC_2", {U'2', U'@'}},
    {VC_3, "VC_3", {U'3', U'#'}}, {VC_4, "VC_4", {U'4', U'$'}}, {VC_5, "VC_5", {U'5', U'%'}},
    {VC_6, "VC_6", {U'6', U'^'}}, {VC_7, "VC_7", {U'7', U'&'}}, {VC_8, "VC_8", {U'8', U'*'}},
    {VC_9, "VC_9", {U'9', U'('}},

    {VC_MINUS, "VC_MINUS", {U'-', U'_'}},
    {VC_EQUALS, "VC_EQUALS", {U'=', U'+'}},
    {VC_OPEN_BRACKET, "VC_OPEN_BRACKET", {U'[', U'{'}},
    {VC_CLOSE_BRACKET, "VC_CLOSE_BRACKET", {U']', U'}'}},
    {VC_BACK_SLASH, "VC_BACK_SLASH", {U'\\', U'|'}},
    {VC_SEMICOLON, "VC_SEMICOLON", {U';', U':'}},
    {VC_QUOTE, "VC_QUOTE", {U'\'', U'"'}},
    {VC_BACKQUOTE, "VC_BACKQUOTE", {U'`', U'~'}},
    {VC_COMMA, "VC_COMMA", {U',', U'<'}},
    {VC_PERIOD, "VC_PERIOD", {U'.', U'>'}},
    {VC_SLASH, "VC_SLASH", {U'/', U'?'}},

    // Numeric keypad — digits matter for the VNI input method.
    {VC_KP_0, "VC_KP_0", {U'0', U'0'}}, {VC_KP_1, "VC_KP_1", {U'1', U'1'}},
    {VC_KP_2, "VC_KP_2", {U'2', U'2'}}, {VC_KP_3, "VC_KP_3", {U'3', U'3'}},
    {VC_KP_4, "VC_KP_4", {U'4', U'4'}}, {VC_KP_5, "VC_KP_5", {U'5', U'5'}},
    {VC_KP_6, "VC_KP_6", {U'6', U'6'}}, {VC_KP_7, "VC_KP_7", {U'7', U'7'}},
    {VC_KP_8, "VC_KP_8", {U'8', U'8'}}, {VC_KP_9, "VC_KP_9", {U'9', U'9'}},
    {VC_KP_SUBTRACT, "VC_KP_SUBTRACT", {U'-', U'-'}},
    {VC_KP_ADD, "VC_KP_ADD", {U'+', U'+'}},
    {VC_KP_DIVIDE, "VC_KP_DIVIDE", {U'/', U'/'}},
    {VC_KP_MULTIPLY, "VC_KP_MULTIPLY", {U'*', U'*'}},
};

[[nodiscard]] bool isModifierKeycode(std::uint16_t keycode) noexcept
{
    switch (keycode) {
    case VC_SHIFT_L: case VC_SHIFT_R:
    case VC_CONTROL_L: case VC_CONTROL_R:
    case VC_ALT_L: case VC_ALT_R:
    case VC_META_L: case VC_META_R:
    case VC_CAPS_LOCK: case VC_NUM_LOCK: case VC_SCROLL_LOCK:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool isNavigationKeycode(std::uint16_t keycode) noexcept
{
    switch (keycode) {
    case VC_UP: case VC_DOWN: case VC_LEFT: case VC_RIGHT:
    case VC_HOME: case VC_END: case VC_PAGE_UP: case VC_PAGE_DOWN:
    case VC_DELETE: case VC_INSERT:
        return true;
    default:
        return false;
    }
}

} // namespace

// ---------------------------------------------------------------------------

KeyMapper::KeyMapper() = default;

void KeyMapper::setOverride(std::uint16_t keycode, KeyChars chars)
{
    overrides_[keycode] = chars;
}

void KeyMapper::clearOverrides() { overrides_.clear(); }

std::uint16_t KeyMapper::keycodeFromName(const std::string& name)
{
    for (const Entry& e : kTable)
        if (name == e.name)
            return e.keycode;
    return 0;
}

KeyMapper::KeyChars KeyMapper::charsFor(std::uint16_t keycode) const
{
    if (const auto it = overrides_.find(keycode); it != overrides_.end())
        return it->second;
    for (const Entry& e : kTable)
        if (e.keycode == keycode)
            return e.chars;
    return {};
}

KeyEvent KeyMapper::fromPressed(std::uint16_t keycode, std::uint16_t mask) const
{
    KeyEvent ev;
    ev.rawCode = keycode;
    ev.shift   = (mask & (MASK_SHIFT_L | MASK_SHIFT_R)) != 0;
    ev.ctrl    = (mask & (MASK_CTRL_L | MASK_CTRL_R)) != 0;
    ev.alt     = (mask & (MASK_ALT_L | MASK_ALT_R)) != 0;
    ev.meta    = (mask & (MASK_META_L | MASK_META_R)) != 0;

    switch (keycode) {
    case VC_BACKSPACE: ev.kind = KeyKind::Backspace; return ev;
    case VC_ENTER:     ev.kind = KeyKind::Enter;     return ev;
    case VC_TAB:       ev.kind = KeyKind::Tab;       return ev;
    case VC_ESCAPE:    ev.kind = KeyKind::Escape;    return ev;
    case VC_SPACE:
        ev.kind      = KeyKind::Space;
        ev.character = U' ';
        return ev;
    default:
        break;
    }

    if (isModifierKeycode(keycode)) {
        ev.kind = KeyKind::Modifier;
        return ev;
    }
    if (isNavigationKeycode(keycode)) {
        ev.kind = KeyKind::Navigation;
        return ev;
    }

    const KeyChars chars = charsFor(keycode);
    if (chars.plain == 0) {
        ev.kind = KeyKind::Other;
        return ev;
    }

    bool upper = ev.shift;
    if (capsLock_ && unicode::isAsciiAlpha(chars.plain))
        upper = !upper;  // Caps Lock only affects letters

    ev.kind      = KeyKind::Character;
    ev.character = upper ? chars.shift : chars.plain;
    return ev;
}

} // namespace st
