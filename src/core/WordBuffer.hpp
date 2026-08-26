// SPDX-License-Identifier: MIT
//
// Shared bookkeeping for engines that work by "recompose the current word, then
// diff against what is already on screen".
//
// The buffer tracks three parallel things for the word being typed:
//   * `text`   — the composed characters we believe are on screen;
//   * `ascii`  — the plain-ASCII transliteration of the same word, used for
//                dictionary/exception lookups;
//   * `locked` — per-character flag marking characters that must not trigger a
//                rule again. This is what makes an undo stick: after `uee`
//                reverts `ü` back to `ue`, that `ue` is locked, so typing a
//                further `e` cannot re-fire the rule.

#pragma once

#include "core/Unicode.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace st {

class WordBuffer {
public:
    void clear() noexcept
    {
        text_.clear();
        ascii_.clear();
        asciiWidth_.clear();
        locked_.clear();
    }

    [[nodiscard]] bool empty() const noexcept { return text_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return text_.size(); }
    [[nodiscard]] const std::u32string& text() const noexcept { return text_; }
    [[nodiscard]] const std::u32string& ascii() const noexcept { return ascii_; }

    [[nodiscard]] char32_t back() const noexcept { return text_.empty() ? 0 : text_.back(); }
    [[nodiscard]] bool backLocked() const noexcept { return locked_.empty() ? false : locked_.back(); }

    /// Lock state counted from the end: 0 is the last character, 1 the one
    /// before it. Out-of-range indices report "locked" so callers fail closed.
    [[nodiscard]] bool lockedFromEnd(std::size_t indexFromEnd) const noexcept
    {
        return indexFromEnd < locked_.size() ? locked_[locked_.size() - 1 - indexFromEnd] : true;
    }

    /// Append one composed character standing for `asciiWidth` ASCII characters.
    void push(char32_t composed, std::u32string_view asciiForm, bool locked = false)
    {
        text_.push_back(composed);
        ascii_.append(asciiForm);
        asciiWidth_.push_back(static_cast<std::uint8_t>(asciiForm.size()));
        locked_.push_back(locked);
    }

    /// Replace the last `count` composed characters with `replacement`, which
    /// stands for the same ASCII text as what it replaces (a transformation
    /// never changes the ASCII transliteration — that is the whole point).
    void replaceBack(std::size_t count, std::u32string_view replacement, bool locked)
    {
        if (count > text_.size())
            count = text_.size();
        std::size_t asciiChars = 0;
        for (std::size_t i = 0; i < count; ++i)
            asciiChars += asciiWidth_[asciiWidth_.size() - 1 - i];

        text_.erase(text_.size() - count);
        asciiWidth_.resize(asciiWidth_.size() - count);
        locked_.resize(locked_.size() - count);

        // The ASCII form is unchanged; redistribute its width over the new chars.
        for (std::size_t i = 0; i < replacement.size(); ++i) {
            text_.push_back(replacement[i]);
            const std::size_t w = (i + 1 == replacement.size())
                                      ? asciiChars - (replacement.size() - 1)
                                      : 1;
            asciiWidth_.push_back(static_cast<std::uint8_t>(w));
            locked_.push_back(locked);
        }
    }

    /// Remove the last composed character (a Backspace the user typed).
    void pop() noexcept
    {
        if (text_.empty())
            return;
        const std::size_t w = asciiWidth_.back();
        text_.pop_back();
        asciiWidth_.pop_back();
        locked_.pop_back();
        ascii_.erase(ascii_.size() >= w ? ascii_.size() - w : 0);
    }

    void lockAll() noexcept
    {
        for (std::size_t i = 0; i < locked_.size(); ++i)
            locked_[i] = true;
    }

    void lockLast(std::size_t count) noexcept
    {
        for (std::size_t i = 0; i < count && i < locked_.size(); ++i)
            locked_[locked_.size() - 1 - i] = true;
    }

private:
    std::u32string            text_;
    std::u32string            ascii_;
    std::vector<std::uint8_t> asciiWidth_;
    std::vector<bool>         locked_;
};

} // namespace st
