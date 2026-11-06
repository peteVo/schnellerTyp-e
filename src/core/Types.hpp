// SPDX-License-Identifier: MIT
//
// schnellerTyp-e — core value types.
//
// Nothing in src/core/ may include Qt, libuiohook or any OS header. The core is
// a pure, deterministic, single-threaded state machine over keystrokes so that
// it can be unit-tested (see tests/) without a display server, a hook or an
// event loop.

#pragma once

#include <cstdint>
#include <string>

namespace st {

/// Coarse classification of an incoming keystroke, produced by the hook layer.
enum class KeyKind : std::uint8_t {
    Character,   ///< Produced a printable character; see KeyEvent::character.
    Backspace,
    Enter,
    Tab,
    Space,
    Escape,
    Navigation,  ///< Arrows, Home/End, PgUp/PgDn — caret moved, word context lost.
    Left, Right, Home, End, Delete, ///< Native composition caret editing.
    ForceVietnamese, UndoComposition, ///< Commands scoped to the current word.
    Modifier,    ///< Shift/Ctrl/Alt/Meta pressed alone.
    Other        ///< Function keys, media keys, anything we do not model.
};

/// A platform-neutral keystroke. The hook layer is responsible for turning a
/// libuiohook event (or a native event) into one of these.
struct KeyEvent {
    KeyKind       kind      = KeyKind::Other;
    char32_t      character = 0;   ///< Valid when kind == Character.
    std::uint16_t rawCode   = 0;   ///< libuiohook virtual code, for diagnostics.
    bool ctrl  = false;
    bool alt   = false;
    bool shift = false;
    bool meta  = false;
    bool altGr = false; ///< Right Alt must not trigger composition shortcuts.

    /// True when a command-style chord is held (Ctrl+C, Cmd+V, Alt+Tab …).
    /// These belong to the focused app unless an active composition explicitly handles them.
    [[nodiscard]] constexpr bool isChord() const noexcept { return ctrl || alt || meta; }

    static KeyEvent character_(char32_t c, bool shift = false) noexcept
    {
        KeyEvent e;
        e.kind      = KeyKind::Character;
        e.character = c;
        e.shift     = shift;
        return e;
    }

    static KeyEvent of(KeyKind k) noexcept
    {
        KeyEvent e;
        e.kind = k;
        return e;
    }
};

/// Shared shortcuts; adapters only supply logical keys and modifier state.
[[nodiscard]] inline KeyEvent compositionKey(KeyEvent key) noexcept
{
    if (key.kind == KeyKind::Character && key.ctrl && key.alt && !key.shift
        && !key.meta && !key.altGr) {
        if (key.character == U'v' || key.character == U'V')
            key.kind = KeyKind::ForceVietnamese;
        else if (key.character == U'z' || key.character == U'Z')
            key.kind = KeyKind::UndoComposition;
    }
    return key;
}

/// What the engine wants the hook layer to do about a keystroke.
///
/// The contract is deliberately narrow: "delete `backspaces` characters that are
/// already on screen, then type `text`". Everything the engines do — composing
/// a Vietnamese syllable, reverting a German digraph, expanding a macro — is
/// expressed as that single primitive.
struct EngineResult {
    /// When true the hook layer must swallow the original keystroke (where the
    /// platform allows it) and perform the edit below instead.
    bool           handled    = false;
    int            backspaces = 0;
    std::u32string text;
    int cursor = -1; ///< Optional codepoint offset for native preedit; -1 means end.

    [[nodiscard]] static EngineResult passthrough() noexcept { return {}; }

    [[nodiscard]] static EngineResult swallow() noexcept
    {
        EngineResult r;
        r.handled = true;
        return r;
    }

    [[nodiscard]] static EngineResult edit(int backspaces, std::u32string text) noexcept
    {
        EngineResult r;
        r.handled    = true;
        r.backspaces = backspaces;
        r.text       = std::move(text);
        return r;
    }

    /// True when the result asks for no visible change at all.
    [[nodiscard]] bool isNoop() const noexcept { return backspaces == 0 && text.empty(); }
};

} // namespace st
