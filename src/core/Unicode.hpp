// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace st::unicode {

/// UTF-32 -> UTF-8. Used at the boundary to Qt and to the OS injectors.
std::string toUtf8(std::u32string_view in);

/// UTF-8 -> UTF-32. Invalid sequences are replaced with U+FFFD.
std::u32string fromUtf8(std::string_view in);

/// UTF-32 -> UTF-16 (with surrogate pairs). Windows SendInput and macOS
/// CGEventKeyboardSetUnicodeString both want UTF-16.
std::u16string toUtf16(std::u32string_view in);

/// ASCII-only case helpers. The engines never need locale-aware casing: every
/// trigger character they inspect is a plain ASCII letter or digit.
[[nodiscard]] constexpr bool isAsciiAlpha(char32_t c) noexcept
{
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
}
[[nodiscard]] constexpr bool isAsciiDigit(char32_t c) noexcept { return c >= U'0' && c <= U'9'; }
[[nodiscard]] constexpr bool isAsciiUpper(char32_t c) noexcept { return c >= U'A' && c <= U'Z'; }
[[nodiscard]] constexpr char32_t toLowerAscii(char32_t c) noexcept
{
    return isAsciiUpper(c) ? static_cast<char32_t>(c + 32) : c;
}
[[nodiscard]] constexpr char32_t toUpperAscii(char32_t c) noexcept
{
    return (c >= U'a' && c <= U'z') ? static_cast<char32_t>(c - 32) : c;
}

/// Length of the common prefix of two strings — the basis of the diff that
/// turns "old on-screen text" + "new composed text" into backspaces + insert.
[[nodiscard]] std::size_t commonPrefix(std::u32string_view a, std::u32string_view b) noexcept;

} // namespace st::unicode
