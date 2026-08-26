// SPDX-License-Identifier: MIT
//
// macOS injection backend.
//
// CGEventKeyboardSetUnicodeString attaches an arbitrary UTF-16 string to a
// synthetic key event, so no layout mapping is needed for the composed
// Vietnamese and German characters. Backspace is sent as a real virtual key
// (kVK_Delete = 0x33) because applications treat it as an editing command
// rather than as text.
//
// Events are posted to kCGHIDEventTap so that they arrive before other taps and
// reach the focused application in order. This requires the process to hold
// Accessibility (AXIsProcessTrusted) — see PlatformSupport.

#include "hook/TextInjector.hpp"

#include "core/Unicode.hpp"

#include <ApplicationServices/ApplicationServices.h>

namespace st {
namespace {

constexpr CGKeyCode kVirtualKeyDelete = 0x33;

class MacInjector final : public TextInjector {
public:
    MacInjector() : source_(CGEventSourceCreate(kCGEventSourceStateHIDSystemState)) {}

    ~MacInjector() override
    {
        if (source_ != nullptr)
            CFRelease(source_);
    }

    [[nodiscard]] bool        available() const override { return AXIsProcessTrusted(); }
    [[nodiscard]] std::string backendName() const override { return "CGEvent"; }
    [[nodiscard]] std::string lastError() const override
    {
        return available() ? std::string{} : "Accessibility access has not been granted.";
    }

    void sendBackspaces(int count) override
    {
        for (int i = 0; i < count; ++i) {
            postKey(kVirtualKeyDelete, true);
            postKey(kVirtualKeyDelete, false);
        }
    }

    void sendText(const std::u32string& text) override
    {
        if (text.empty())
            return;
        const std::u16string utf16 = unicode::toUtf16(text);

        // One event per code point keeps ordering unambiguous in applications
        // that coalesce input, and keeps the "ignore my own events" accounting
        // in HookService simple.
        std::size_t i = 0;
        while (i < utf16.size()) {
            std::size_t len = 1;
            if (utf16[i] >= 0xD800 && utf16[i] <= 0xDBFF && i + 1 < utf16.size())
                len = 2;
            postUnicode(utf16.data() + i, len, true);
            postUnicode(utf16.data() + i, len, false);
            i += len;
        }
    }

private:
    void postKey(CGKeyCode key, bool down) const
    {
        CGEventRef event = CGEventCreateKeyboardEvent(source_, key, down);
        if (event == nullptr)
            return;
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }

    void postUnicode(const char16_t* units, std::size_t length, bool down) const
    {
        CGEventRef event = CGEventCreateKeyboardEvent(source_, 0, down);
        if (event == nullptr)
            return;
        CGEventKeyboardSetUnicodeString(event, static_cast<UniCharCount>(length),
                                        reinterpret_cast<const UniChar*>(units));
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }

    CGEventSourceRef source_ = nullptr;
};

} // namespace

std::unique_ptr<TextInjector> createMacInjector()
{
    return std::make_unique<MacInjector>();
}

} // namespace st
