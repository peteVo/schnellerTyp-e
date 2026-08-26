// SPDX-License-Identifier: MIT
#include "core/Unicode.hpp"

#include <algorithm>

namespace st::unicode {

std::string toUtf8(std::u32string_view in)
{
    std::string out;
    out.reserve(in.size() * 3);
    for (char32_t cp : in) {
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            cp = 0xFFFD;
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::u32string fromUtf8(std::string_view in)
{
    std::u32string out;
    out.reserve(in.size());
    std::size_t i = 0;
    const std::size_t n = in.size();
    auto cont = [&](std::size_t k) {
        return k < n && (static_cast<unsigned char>(in[k]) & 0xC0) == 0x80;
    };
    while (i < n) {
        const auto b0 = static_cast<unsigned char>(in[i]);
        char32_t cp = 0xFFFD;
        std::size_t len = 1;
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0 && cont(i + 1)) {
            cp  = ((b0 & 0x1Fu) << 6) | (static_cast<unsigned char>(in[i + 1]) & 0x3Fu);
            len = 2;
        } else if ((b0 & 0xF0) == 0xE0 && cont(i + 1) && cont(i + 2)) {
            cp = ((b0 & 0x0Fu) << 12) | ((static_cast<unsigned char>(in[i + 1]) & 0x3Fu) << 6)
                 | (static_cast<unsigned char>(in[i + 2]) & 0x3Fu);
            len = 3;
        } else if ((b0 & 0xF8) == 0xF0 && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
            cp = ((b0 & 0x07u) << 18) | ((static_cast<unsigned char>(in[i + 1]) & 0x3Fu) << 12)
                 | ((static_cast<unsigned char>(in[i + 2]) & 0x3Fu) << 6)
                 | (static_cast<unsigned char>(in[i + 3]) & 0x3Fu);
            len = 4;
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            cp = 0xFFFD;
        out.push_back(cp);
        i += len;
    }
    return out;
}

std::u16string toUtf16(std::u32string_view in)
{
    std::u16string out;
    out.reserve(in.size() + 4);
    for (char32_t cp : in) {
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            cp = 0xFFFD;
        if (cp < 0x10000) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            const char32_t v = cp - 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (v >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (v & 0x3FF)));
        }
    }
    return out;
}

std::size_t commonPrefix(std::u32string_view a, std::u32string_view b) noexcept
{
    const std::size_t n = std::min(a.size(), b.size());
    std::size_t i = 0;
    while (i < n && a[i] == b[i])
        ++i;
    return i;
}

} // namespace st::unicode
