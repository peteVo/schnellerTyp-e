// SPDX-License-Identifier: MIT
#include "core/CompositionSession.hpp"

#include <algorithm>

namespace st {

CompositionSession::Result CompositionSession::finish()
{
    auto text = std::move(preedit_);
    preedit_.clear();
    if (engine_)
        engine_->reset();
    return {false, true, std::move(text)};
}

void CompositionSession::reset()
{
    preedit_.clear();
    if (engine_)
        engine_->reset();
}

CompositionSession::Result CompositionSession::processKey(const KeyEvent& key)
{
    if (!engine_)
        return {};
    if (key.kind == KeyKind::Modifier)
        return {};
    if (key.isChord() || key.kind == KeyKind::Enter || key.kind == KeyKind::Tab
        || key.kind == KeyKind::Navigation || key.kind == KeyKind::Other)
        return finish();
    if (key.kind == KeyKind::Backspace && preedit_.empty()) {
        engine_->reset();
        return {};
    }

    const auto result = engine_->processKey(key);
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

    if (key.kind == KeyKind::Space || preedit_.size() >= 256) {
        auto text = std::move(preedit_);
        preedit_.clear();
        if (text.size() >= 256)
            engine_->reset();
        return {true, true, std::move(text)};
    }
    return {true, false, preedit_};
}

} // namespace st
