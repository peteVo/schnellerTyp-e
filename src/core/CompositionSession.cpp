// SPDX-License-Identifier: MIT
#include "core/CompositionSession.hpp"
#include "core/auto/AutoRuleEngine.hpp"

#include <algorithm>

namespace st {

CompositionSession::Result CompositionSession::finish()
{
    auto text = std::move(preedit_);
    preedit_.clear();
    cursor_ = 0;
    learningAllowed_ = true;
    if (engine_)
        engine_->reset();
    return {false, true, std::move(text)};
}

void CompositionSession::reset()
{
    preedit_.clear();
    cursor_ = 0;
    learningAllowed_ = true;
    if (engine_)
        engine_->reset();
}

std::u32string CompositionSession::literalWord() const
{
    const auto* automatic = dynamic_cast<const AutoRuleEngine*>(engine_.get());
    return automatic && learningAllowed_ && preedit_ == automatic->currentText()
        ? automatic->literalWord() : std::u32string();
}

bool CompositionSession::vietnameseOverride() const
{
    const auto* automatic = dynamic_cast<const AutoRuleEngine*>(engine_.get());
    return automatic && automatic->vietnameseOverride();
}

CompositionSession::Result CompositionSession::processKey(const KeyEvent& input)
{
    if (!engine_)
        return {};
    const auto key = compositionKey(input);
    if (key.kind == KeyKind::Modifier)
        return {};
    const bool command = key.kind == KeyKind::ForceVietnamese || key.kind == KeyKind::UndoComposition;
    if ((!command && key.isChord()) || key.kind == KeyKind::Enter || key.kind == KeyKind::Tab
        || key.kind == KeyKind::Navigation || key.kind == KeyKind::Other)
        return finish();
    auto* automatic = dynamic_cast<AutoRuleEngine*>(engine_.get());
    const bool direction = key.kind == KeyKind::Left || key.kind == KeyKind::Right
        || key.kind == KeyKind::Home || key.kind == KeyKind::End;
    if (direction && automatic && !preedit_.empty() && !key.shift && !key.isChord()) {
        if (key.kind == KeyKind::Left && cursor_ > 0) --cursor_;
        if (key.kind == KeyKind::Right && cursor_ < preedit_.size()) ++cursor_;
        if (key.kind == KeyKind::Home) cursor_ = 0;
        if (key.kind == KeyKind::End) cursor_ = preedit_.size();
        return {true, false, preedit_, cursor_};
    }
    if (direction || (key.kind == KeyKind::Delete && (!automatic || preedit_.empty())))
        return finish();
    if (key.kind == KeyKind::Backspace && preedit_.empty()) {
        engine_->reset();
        return {};
    }

    EngineResult result;
    if (automatic && !key.isChord()
        && ((cursor_ < preedit_.size() && (key.kind == KeyKind::Character || key.kind == KeyKind::Backspace))
            || key.kind == KeyKind::Delete)) {
        auto edited = preedit_;
        auto position = cursor_;
        if (key.kind == KeyKind::Character) {
            edited.insert(edited.begin() + static_cast<std::ptrdiff_t>(position), key.character);
            ++position;
        } else if (key.kind == KeyKind::Backspace && position > 0) {
            edited.erase(--position, 1);
        } else if (key.kind == KeyKind::Delete && position < edited.size()) {
            edited.erase(position, 1);
        } else {
            return {true, false, preedit_, cursor_};
        }
        result = automatic->editWord(std::move(edited), position, cursor_);
    } else {
        result = engine_->processKey(key);
    }
    if (result.handled) {
        const auto count = std::min(preedit_.size(),
                                    static_cast<std::size_t>(std::max(0, result.backspaces)));
        preedit_.resize(preedit_.size() - count);
        preedit_ += result.text;
    } else if (key.kind == KeyKind::Backspace) {
        preedit_.pop_back();
    } else if (key.kind == KeyKind::Character || key.kind == KeyKind::Space) {
        preedit_.push_back(key.kind == KeyKind::Space ? U' ' : key.character);
    } else {
        return finish();
    }

    cursor_ = result.cursor < 0 ? preedit_.size()
        : std::min(preedit_.size(), static_cast<std::size_t>(result.cursor));

    const bool wordEnded = automatic && key.kind == KeyKind::Character && automatic->currentText().empty();
    if (wordEnded) learningAllowed_ = false;
    if (key.kind == KeyKind::Space) learningAllowed_ = true;
    if (key.kind == KeyKind::Space || wordEnded || preedit_.size() >= 256) {
        auto text = std::move(preedit_);
        preedit_.clear();
        cursor_ = 0;
        if (text.size() >= 256)
            engine_->reset();
        return {true, true, std::move(text)};
    }
    return {true, false, preedit_, cursor_};
}

} // namespace st
