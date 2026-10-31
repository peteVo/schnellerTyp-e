// SPDX-License-Identifier: MIT
// Run every high-frequency English word through Automatic mode and report
// changes. Build separately against schnellertype_core.

#include "core/auto/AutoRuleEngine.hpp"

#include <array>
#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

namespace {
struct LexiconEntry { std::string_view word; int frequency; };
#include "core/auto/FrequencyData.inc"

struct Outcome { std::u32string screen; int errors = 0; };

Outcome run(st::AutoRuleEngine& engine, std::u32string_view input)
{
    engine.reset();
    std::u32string screen;
    std::size_t active = 0;
    int errors = 0;
    for (char32_t c : input) {
        const auto key = c == U' ' ? st::KeyEvent::of(st::KeyKind::Space)
                                   : st::KeyEvent::character_(c, st::unicode::isAsciiUpper(c));
        const auto result = engine.processKey(key);
        if (result.backspaces < 0 || static_cast<std::size_t>(result.backspaces) > screen.size() - active)
            ++errors;
        if (result.handled) {
            const auto remove = std::min(screen.size(), static_cast<std::size_t>(std::max(0, result.backspaces)));
            screen.resize(screen.size() - remove);
            screen += result.text;
        } else if (c == U' ') {
            screen.push_back(c);
        } else {
            screen.push_back(c);
        }
        if (c == U' ') active = screen.size();
    }
    return {std::move(screen), errors};
}

std::u32string u32(std::string_view word) { return st::unicode::fromUtf8(word); }
}

int main()
{
    int audited = 0, highSoloChanged = 0, contextChanged = 0;
    int rareAudited = 0, rareSoloChanged = 0;
    int failures = 0, shown = 0, rareShown = 0;
    constexpr int limit = 30;
    st::AutoRuleEngine engine;
    std::puts("Automatic mode English frequency audit (frequency >= 400)");
    for (const auto& entry : kEnglishWords) {
        if (entry.frequency < 400) {
            ++rareAudited;
            const auto word = u32(entry.word);
            const auto solo = run(engine, word + U" ");
            const auto repeated = run(engine, word + U" ");
            std::u32string context = U"This is ";
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
            if (solo.errors || inContext.errors || repeated.screen != solo.screen
                || inContext.screen != context)
                ++failures;
            if (inContext.screen != context) {
                ++contextChanged;
                if (shown++ < limit)
                    std::printf("  FAIL English context %.*s -> %s\n",
                                static_cast<int>(entry.word.size()), entry.word.data(),
                                st::unicode::toUtf8(inContext.screen).c_str());
            }
            continue;
        }
        ++audited;
        const auto word = u32(entry.word);
        const auto solo = run(engine, word + U" ");
        const auto repeated = run(engine, word + U" ");
        std::u32string context = U"This is ";
        context += word;
        context += U" ";
        const auto inContext = run(engine, context);
        if (solo.errors || inContext.errors || repeated.screen != solo.screen)
            ++failures;
        const bool soloAltered = solo.screen != word + U" ";
        const bool contextAltered = inContext.screen != context;
        if (soloAltered) {
            ++highSoloChanged;
            ++failures;
            if (shown++ < limit)
                std::printf("  FAIL high-frequency standalone %.*s -> %s\n",
                            static_cast<int>(entry.word.size()), entry.word.data(),
                            st::unicode::toUtf8(solo.screen).c_str());
        }
        if (contextAltered) {
            ++contextChanged;
            ++failures;
            if (shown++ < limit)
                std::printf("  FAIL English context %.*s -> %s\n",
                            static_cast<int>(entry.word.size()), entry.word.data(),
                            st::unicode::toUtf8(inContext.screen).c_str());
        }
    }
    std::printf("\nHigh-frequency (score >= 400): %d words; %d standalone changes (must be zero).\n",
                audited, highSoloChanged);
    std::printf("English context (all frequencies): %d words; %d changes (must be zero).\n",
                audited + rareAudited, contextChanged);
    std::printf("Rare standalone (score < 400): %d words; %d changes observed (informational).\n",
                rareAudited, rareSoloChanged);
    std::printf("Regression failures, including backspace, committed-prefix, or reset determinism: %d.\n",
                failures);
    if (shown > limit || rareShown > limit) std::printf("Showing at most %d changes per category.\n", limit);
    return failures ? 1 : 0;
}
