// SPDX-License-Identifier: MIT
//
// Windows injection backend.
//
// KEYEVENTF_UNICODE lets SendInput deliver an arbitrary UTF-16 unit without any
// keyboard-layout involvement, which is exactly what an IME needs: no layout
// has "ế" or "ß" on a key. Surrogate pairs are sent as two consecutive units,
// which is what the API expects.
//
// Caveats documented in BUILD.md:
//   * a target window running elevated will not accept input from a
//     non-elevated sender (UIPI). Run schnellerTyp-e elevated, or accept that
//     it is inactive in those windows.
//   * some games and remote-desktop clients read raw input (WM_INPUT / DirectX)
//     and ignore synthesised events entirely.

#include "hook/TextInjector.hpp"

#include "core/Unicode.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <vector>

namespace st {
namespace {

class WindowsInjector final : public TextInjector {
public:
    [[nodiscard]] bool        available() const override { return true; }
    [[nodiscard]] std::string backendName() const override { return "SendInput"; }

    void sendBackspaces(int count) override
    {
        if (count <= 0)
            return;
        std::vector<INPUT> inputs;
        inputs.reserve(static_cast<std::size_t>(count) * 2);
        const WORD scan = static_cast<WORD>(MapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC));
        for (int i = 0; i < count; ++i) {
            inputs.push_back(makeVirtualKey(VK_BACK, scan, /*keyUp=*/false));
            inputs.push_back(makeVirtualKey(VK_BACK, scan, /*keyUp=*/true));
        }
        dispatch(inputs);
    }

    void sendText(const std::u32string& text) override
    {
        if (text.empty())
            return;
        const std::u16string utf16 = unicode::toUtf16(text);
        std::vector<INPUT>   inputs;
        inputs.reserve(utf16.size() * 2);
        for (char16_t unit : utf16) {
            inputs.push_back(makeUnicode(unit, /*keyUp=*/false));
            inputs.push_back(makeUnicode(unit, /*keyUp=*/true));
        }
        dispatch(inputs);
    }

private:
    static INPUT makeUnicode(char16_t unit, bool keyUp)
    {
        INPUT in{};
        in.type           = INPUT_KEYBOARD;
        in.ki.wVk         = 0;
        in.ki.wScan       = static_cast<WORD>(unit);
        in.ki.dwFlags     = KEYEVENTF_UNICODE | (keyUp ? KEYEVENTF_KEYUP : 0u);
        in.ki.time        = 0;
        in.ki.dwExtraInfo = 0;
        return in;
    }

    static INPUT makeVirtualKey(WORD vk, WORD scan, bool keyUp)
    {
        INPUT in{};
        in.type           = INPUT_KEYBOARD;
        in.ki.wVk         = vk;
        in.ki.wScan       = scan;
        in.ki.dwFlags     = keyUp ? KEYEVENTF_KEYUP : 0u;
        in.ki.time        = 0;
        in.ki.dwExtraInfo = 0;
        return in;
    }

    static void dispatch(std::vector<INPUT>& inputs)
    {
        if (inputs.empty())
            return;
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }
};

} // namespace

std::unique_ptr<TextInjector> createWindowsInjector()
{
    return std::make_unique<WindowsInjector>();
}

} // namespace st
