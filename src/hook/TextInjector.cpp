// SPDX-License-Identifier: MIT
#include "hook/TextInjector.hpp"

#include "core/Unicode.hpp"
#include "hook/PlatformSupport.hpp"

#include <uiohook.h>

#include <chrono>
#include <thread>

namespace st {
namespace {

/// Fallback backend built on libuiohook's own event posting. Kept because it is
/// the only thing available on Wayland without a compositor-specific protocol,
/// and because it makes the app degrade instead of failing outright.
class UiohookInjector final : public TextInjector {
public:
    [[nodiscard]] bool available() const override { return true; }
    [[nodiscard]] std::string backendName() const override { return "uiohook"; }

    void sendBackspaces(int count) override
    {
        for (int i = 0; i < count; ++i) {
            postKey(VC_BACKSPACE, 0, true);
            postKey(VC_BACKSPACE, 0, false);
        }
    }

    void sendText(const std::u32string& text) override
    {
        const std::u16string utf16 = unicode::toUtf16(text);
        for (char16_t unit : utf16) {
            uiohook_event event{};
            event.type                  = EVENT_KEY_TYPED;
            event.mask                  = 0;
            event.data.keyboard.keycode = VC_UNDEFINED;
            event.data.keyboard.rawcode = 0;
            event.data.keyboard.keychar = static_cast<uint16_t>(unit);
            hook_post_event(&event);
        }
    }

private:
    static void postKey(std::uint16_t keycode, unsigned mask, bool pressed)
    {
        uiohook_event event{};
        event.type                  = pressed ? EVENT_KEY_PRESSED : EVENT_KEY_RELEASED;
        event.mask                  = static_cast<uint16_t>(mask);
        event.data.keyboard.keycode = keycode;
        event.data.keyboard.rawcode = 0;
        event.data.keyboard.keychar = CHAR_UNDEFINED;
        hook_post_event(&event);
    }
};

} // namespace

int TextInjector::keyEventCount(int backspaces, const std::u32string& text)
{
    // Every backspace and every UTF-16 unit is one press + one release.
    const std::size_t units = unicode::toUtf16(text).size();
    return (backspaces + static_cast<int>(units)) * 2;
}

std::unique_ptr<TextInjector> createUiohookInjector()
{
    return std::make_unique<UiohookInjector>();
}

std::unique_ptr<TextInjector> TextInjector::create()
{
    std::unique_ptr<TextInjector> injector;
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
    injector = createWindowsInjector();
#elif defined(SCHNELLERTYPE_PLATFORM_MACOS)
    injector = createMacInjector();
#elif defined(SCHNELLERTYPE_PLATFORM_LINUX)
    if (platform::sessionType() != platform::SessionType::Wayland)
        injector = createX11Injector();
#endif
    if (injector && injector->available())
        return injector;
    return createUiohookInjector();
}

} // namespace st
