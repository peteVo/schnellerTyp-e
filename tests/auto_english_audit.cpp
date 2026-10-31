// SPDX-License-Identifier: MIT
// Measure Vietnamese-priority changes and verify explicit literal recovery.
// Build separately against schnellertype_core.

#include "core/auto/AutoRuleEngine.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace {
struct LexiconEntry { std::string_view word; int frequency; };
#include "core/auto/FrequencyData.inc"

struct Outcome { std::u32string screen; int errors = 0; };

Outcome run(st::AutoRuleEngine& engine, std::u32string_view input, bool escapeBeforeSpace = false)
{
    engine.reset();
    std::u32string screen;
    std::size_t active = 0;
    int errors = 0;
    auto process = [&](st::KeyEvent key, char32_t c) {
        const auto result = engine.processKey(key);
        if (result.backspaces < 0 || static_cast<std::size_t>(result.backspaces) > screen.size() - active)
            ++errors;
        if (result.handled) {
            const auto remove = std::min(screen.size(), static_cast<std::size_t>(std::max(0, result.backspaces)));
            screen.resize(screen.size() - remove);
            screen += result.text;
        } else if (c == U' ') {
            screen.push_back(c);
        } else if (c != 0) {
            screen.push_back(c);
        }
        if (c == U' ') active = screen.size();
    };
    for (char32_t c : input) {
        const auto key = c == U' ' ? st::KeyEvent::of(st::KeyKind::Space)
                                   : st::KeyEvent::character_(c, st::unicode::isAsciiUpper(c));
        process(key, c);
    }
    if (escapeBeforeSpace) {
        process(st::KeyEvent::of(st::KeyKind::Escape), 0);
        process(st::KeyEvent::of(st::KeyKind::Space), U' ');
    }
    return {std::move(screen), errors};
}

std::u32string u32(std::string_view word) { return st::unicode::fromUtf8(word); }

bool asciiWord(std::string_view word)
{
    return !word.empty() && std::all_of(word.begin(), word.end(), [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    });
}
}

int main()
{
    int audited = 0, highSoloChanged = 0, contextChanged = 0;
    int rareAudited = 0, rareSoloChanged = 0;
    int failures = 0, shown = 0, rareShown = 0;
    int safetyAudited = 0, skippedNonalpha = 0, escapeFailures = 0, literalFailures = 0;
    constexpr int limit = 30;
    st::AutoRuleEngine engine;
    std::string literalList;
    for (const auto& entry : kEnglishWords) {
        if (!asciiWord(entry.word)) { ++skippedNonalpha; continue; }
        if (!literalList.empty()) literalList.push_back('\n');
        literalList.append(entry.word);
    }
    st::AutoRuleEngine literalEngine;
    literalEngine.applyOptions({{"literalWords", literalList}});
    std::puts("Automatic mode Vietnamese-priority impact and literal safety audit");
    for (const auto& entry : kEnglishWords) {
        if (entry.frequency < 400) {
            ++rareAudited;
            const auto word = u32(entry.word);
            const auto solo = run(engine, word + U" ");
            const auto repeated = run(engine, word + U" ");
            std::u32string context = U"Hello ";
            context += word;
            context += U" ";
            const auto inContext = run(engine, context);
            if (solo.screen != word + U" ") {
                ++rareSoloChanged;
                if (rareShown++ < limit)
                    std::printf("  rare standalone %.*s -> %s\n",
                                static_cast<int>(entry.word.size()), entry.word.data(),
                                st::unicode::toUtf8(solo.screen).c_str());
            }
            if (solo.errors || inContext.errors || repeated.screen != solo.screen)
                ++failures;
            if (inContext.screen != context) {
                ++contextChanged;
                if (shown++ < limit)
                    std::printf("  Vietnamese-priority context %.*s -> %s\n",
                                static_cast<int>(entry.word.size()), entry.word.data(),
                                st::unicode::toUtf8(inContext.screen).c_str());
            }
            continue;
        }
        ++audited;
        const auto word = u32(entry.word);
        const auto solo = run(engine, word + U" ");
        const auto repeated = run(engine, word + U" ");
        std::u32string context = U"Hello ";
        context += word;
        context += U" ";
        const auto inContext = run(engine, context);
        if (solo.errors || inContext.errors || repeated.screen != solo.screen)
            ++failures;
        const bool soloAltered = solo.screen != word + U" ";
        const bool contextAltered = inContext.screen != context;
        if (soloAltered) {
            ++highSoloChanged;
            if (shown++ < limit)
                std::printf("  high-frequency standalone %.*s -> %s\n",
                            static_cast<int>(entry.word.size()), entry.word.data(),
                            st::unicode::toUtf8(solo.screen).c_str());
        }
        if (contextAltered) {
            ++contextChanged;
            if (shown++ < limit)
                std::printf("  Vietnamese-priority context %.*s -> %s\n",
                            static_cast<int>(entry.word.size()), entry.word.data(),
                            st::unicode::toUtf8(inContext.screen).c_str());
        }
    }
    for (const auto& entry : kEnglishWords) {
        if (!asciiWord(entry.word)) continue;
        ++safetyAudited;
        const auto word = u32(entry.word);
        const auto escaped = run(engine, word, true);
        const auto protectedWord = run(literalEngine, word + U" ");
        if (escaped.errors || escaped.screen != word + U" ") {
            ++escapeFailures;
            ++failures;
            if (shown++ < limit)
                std::printf("  FAIL Escape recovery %.*s -> %s\n",
                            static_cast<int>(entry.word.size()), entry.word.data(),
                            st::unicode::toUtf8(escaped.screen).c_str());
        }
        if (protectedWord.errors || protectedWord.screen != word + U" ") {
            ++literalFailures;
            ++failures;
            if (shown++ < limit)
                std::printf("  FAIL personal literal %.*s -> %s\n",
                            static_cast<int>(entry.word.size()), entry.word.data(),
                            st::unicode::toUtf8(protectedWord.screen).c_str());
        }
    }
    std::printf("\nHigh-frequency (score >= 400): %d words; %d standalone changes observed.\n",
                audited, highSoloChanged);
    std::printf("Vietnamese-priority context (all frequencies): %d words; %d changes observed.\n",
                audited + rareAudited, contextChanged);
    std::printf("Rare standalone (score < 400): %d words; %d changes observed (informational).\n",
                rareAudited, rareSoloChanged);
    std::printf("Literal recovery: %d ASCII words; %d Escape failures; %d personal-literal failures; %d non-ASCII/non-letter entries skipped.\n",
                safetyAudited, escapeFailures, literalFailures, skippedNonalpha);
    std::printf("Audit failures (invalid edits, key handling, or reset determinism): %d.\n", failures);
    if (shown > limit || rareShown > limit) std::printf("Showing at most %d changes per category.\n", limit);
    return failures ? 1 : 0;
}
