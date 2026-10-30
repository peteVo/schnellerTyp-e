// SPDX-License-Identifier: MIT
// Opt-in interactive probe for the real Windows keyboard hook.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include "core/auto/AutoRuleEngine.hpp"
#include "core/vietnamese/VietnameseRuleEngine.hpp"
#include "hook/HookService.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <cwchar>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr DWORD kChildMaxMs = 120'000;
constexpr DWORD kReadyMaxMs = 8'000;
constexpr DWORD kDrainMaxMs = 3'000;
constexpr wchar_t kWindowClass[] = L"SchnellerTypeHookProbeWindow";

void log(bool error, const wchar_t* format, ...)
{
    wchar_t buffer[4096]{};
    va_list args;
    va_start(args, format);
    std::vswprintf(buffer, std::size(buffer), format, args);
    va_end(args);
    const int size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
    std::string text(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, text.data(), size, nullptr, nullptr);
    DWORD written = 0;
    WriteFile(GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE),
              text.data(), static_cast<DWORD>(text.size() - 1), &written, nullptr);
}

struct Handles {
    HANDLE ready = nullptr;
    HANDLE stop = nullptr;
    PROCESS_INFORMATION child{};

    ~Handles()
    {
        if (stop)
            SetEvent(stop);
        if (child.hProcess) {
            if (WaitForSingleObject(child.hProcess, 5'000) == WAIT_TIMEOUT) {
                TerminateProcess(child.hProcess, 0xEE);
                WaitForSingleObject(child.hProcess, 1'000);
            }
            CloseHandle(child.hProcess);
            CloseHandle(child.hThread);
        }
        if (ready)
            CloseHandle(ready);
        if (stop)
            CloseHandle(stop);
    }
};

struct ScratchWindow {
    HWND window = nullptr;
    HWND edit = nullptr;

    ~ScratchWindow()
    {
        if (window)
            DestroyWindow(window);
    }
};

void pumpMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT)
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool scratchOwnsFocus(const ScratchWindow& scratch)
{
    pumpMessages();
    return scratch.window && scratch.edit && IsWindow(scratch.window) && IsWindow(scratch.edit)
           && GetForegroundWindow() == scratch.window && GetFocus() == scratch.edit;
}

bool capsLockIsOff()
{
    return (GetKeyState(VK_CAPITAL) & 1) == 0;
}

bool noCommandModifierHeld()
{
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0
           && (GetAsyncKeyState(VK_MENU) & 0x8000) == 0
           && (GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0
           && (GetAsyncKeyState(VK_LWIN) & 0x8000) == 0
           && (GetAsyncKeyState(VK_RWIN) & 0x8000) == 0;
}

bool acquireFocus(ScratchWindow& scratch)
{
    ShowWindow(scratch.window, SW_SHOWNORMAL);
    SetWindowPos(scratch.window, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    UpdateWindow(scratch.window);
    SetForegroundWindow(scratch.window);
    SetFocus(scratch.edit);
    const auto deadline = GetTickCount64() + 30'000;
    ULONGLONG focusedSince = 0;
    do {
        pumpMessages();
        if (GetForegroundWindow() == scratch.window)
            SetFocus(scratch.edit);
        if (scratchOwnsFocus(scratch)) {
            if (focusedSince == 0)
                focusedSince = GetTickCount64();
            if (GetTickCount64() - focusedSince >= 250)
                return true;
        } else {
            focusedSince = 0;
        }
        Sleep(25);
    } while (GetTickCount64() < deadline);
    log(true, L"Scratch state: visible=%d, foreground=%d, editFocus=%d\n",
                  IsWindowVisible(scratch.window), GetForegroundWindow() == scratch.window,
                  GetFocus() == scratch.edit);
    return false;
}

bool createScratchWindow(ScratchWindow& scratch, HINSTANCE instance)
{
    WNDCLASSW cls{};
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    cls.lpszClassName = kWindowClass;
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    scratch.window = CreateWindowExW(0, kWindowClass, L"schnellerTyp-e hook probe — scratch only",
                                     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 820, 320,
                                     nullptr, nullptr, instance, nullptr);
    if (!scratch.window)
        return false;
    scratch.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE
                                       | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
                                   12, 12, 780, 250, scratch.window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(100)), instance, nullptr);
    if (!scratch.edit) {
        DestroyWindow(scratch.window);
        scratch.window = nullptr;
        return false;
    }
    return true;
}

bool makeChildCommand(std::wstring& command, const std::wstring& readyName,
                      const std::wstring& stopName, bool vni, bool vi)
{
    std::array<wchar_t, 32'768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
        return false;
    command = L"\"" + std::wstring(path.data(), length) + L"\" --hook \"" + readyName
              + L"\" \"" + stopName + L"\"" + (vni ? L" --vni" : vi ? L" --vi" : L"");
    return true;
}

bool launchHookChild(Handles& handles, bool vni, bool vi)
{
    const DWORD processId = GetCurrentProcessId();
    const unsigned long long nonce = GetTickCount64();
    std::array<wchar_t, 160> readyName{};
    std::array<wchar_t, 160> stopName{};
    std::swprintf(readyName.data(), readyName.size(), L"Local\\schnellerTypHook_%lu_%llu_ready",
                  processId, nonce);
    std::swprintf(stopName.data(), stopName.size(), L"Local\\schnellerTypHook_%lu_%llu_stop",
                  processId, nonce);

    handles.ready = CreateEventW(nullptr, TRUE, FALSE, readyName.data());
    handles.stop = CreateEventW(nullptr, TRUE, FALSE, stopName.data());
    if (!handles.ready || !handles.stop)
        return false;

    std::wstring command;
    if (!makeChildCommand(command, readyName.data(), stopName.data(), vni, vi))
        return false;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    return CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                          nullptr, nullptr, &startup, &handles.child) != FALSE;
}

int runHookChild(const wchar_t* readyName, const wchar_t* stopName, bool vni, bool vi)
{
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, readyName);
    HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, stopName);
    if (!ready || !stop) {
        if (ready)
            CloseHandle(ready);
        if (stop)
            CloseHandle(stop);
        return 2;
    }

    int exitCode = 0;
    try {
        st::HookService hook;
        std::unique_ptr<st::LanguageRuleEngine> engine;
        if (vi) {
            engine = std::make_unique<st::VietnameseRuleEngine>();
        } else {
            auto automatic = std::make_unique<st::AutoRuleEngine>();
            if (vni)
                automatic->applyOptions({{"method", "vni"}});
            engine = std::move(automatic);
        }
        hook.setEngine(std::move(engine));
        hook.setEnabled(true);
        std::atomic_bool failed{false};
        hook.setStatusCallback([ready, &failed](st::HookService::Status status) {
            if (status.state == st::HookService::State::Running)
                SetEvent(ready);
            else if (status.state == st::HookService::State::Failed)
                failed.store(true, std::memory_order_release);
        });

        if (!hook.start()) {
            hook.stop();
            CloseHandle(ready);
            CloseHandle(stop);
            return 3;
        }

        const ULONGLONG deadline = GetTickCount64() + kChildMaxMs;
        bool stopped = false;
        while (GetTickCount64() < deadline) {
            if (WaitForSingleObject(stop, 50) == WAIT_OBJECT_0) {
                stopped = true;
                break;
            }
            if (failed.load(std::memory_order_acquire)) {
                exitCode = 4;
                break;
            }
        }
        if (!stopped && exitCode == 0)
            exitCode = 5;
        hook.stop();
    } catch (...) {
        exitCode = 6;
    }

    CloseHandle(ready);
    CloseHandle(stop);
    return exitCode;
}

bool waitForHookReady(const Handles& handles)
{
    const HANDLE waits[] = {handles.ready, handles.child.hProcess};
    const DWORD result = WaitForMultipleObjects(2, waits, FALSE, kReadyMaxMs);
    if (result == WAIT_OBJECT_0)
        return WaitForSingleObject(handles.child.hProcess, 0) == WAIT_TIMEOUT;
    return false;
}

enum class TypeResult { Success, FocusLost, CapsLockOn, ModifierHeld, UnsupportedKey, SendFailed };

TypeResult sendCharacter(const ScratchWindow& scratch, wchar_t character)
{
    if (!scratchOwnsFocus(scratch))
        return TypeResult::FocusLost;
    if (!capsLockIsOff())
        return TypeResult::CapsLockOn;
    if (!noCommandModifierHeld())
        return TypeResult::ModifierHeld;

    const HKL layout = GetKeyboardLayout(0);
    const SHORT mapping = character == L'\n' ? VK_RETURN
                        : character == L'\b' ? VK_BACK
                        : character == 0x1b ? VK_ESCAPE
                        : VkKeyScanExW(character, layout);
    if (mapping == -1)
        return TypeResult::UnsupportedKey;
    const BYTE modifiers = HIBYTE(static_cast<WORD>(mapping));
    if ((modifiers & ~1u) != 0)
        return TypeResult::UnsupportedKey;

    const WORD key = LOBYTE(static_cast<WORD>(mapping));
    INPUT events[4]{};
    UINT count = 0;
    if ((modifiers & 1u) != 0) {
        events[count].type = INPUT_KEYBOARD;
        events[count++].ki.wVk = VK_SHIFT;
    }
    events[count].type = INPUT_KEYBOARD;
    events[count++].ki.wVk = key;
    events[count].type = INPUT_KEYBOARD;
    events[count].ki.wVk = key;
    events[count++].ki.dwFlags = KEYEVENTF_KEYUP;
    if ((modifiers & 1u) != 0) {
        events[count].type = INPUT_KEYBOARD;
        events[count].ki.wVk = VK_SHIFT;
        events[count++].ki.dwFlags = KEYEVENTF_KEYUP;
    }

    if (SendInput(count, events, sizeof(INPUT)) != count) {
        INPUT release[2]{};
        release[0].type = INPUT_KEYBOARD;
        release[0].ki.wVk = key;
        release[0].ki.dwFlags = KEYEVENTF_KEYUP;
        release[1].type = INPUT_KEYBOARD;
        release[1].ki.wVk = VK_SHIFT;
        release[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(static_cast<UINT>(std::size(release)), release, sizeof(INPUT));
        return TypeResult::SendFailed;
    }
    return TypeResult::Success;
}

bool sendVirtualKey(const ScratchWindow& scratch, WORD key)
{
    if (!scratchOwnsFocus(scratch) || !capsLockIsOff() || !noCommandModifierHeld())
        return false;
    INPUT events[2]{};
    events[0].type = INPUT_KEYBOARD;
    events[0].ki.wVk = key;
    events[1].type = INPUT_KEYBOARD;
    events[1].ki.wVk = key;
    events[1].ki.dwFlags = KEYEVENTF_KEYUP;
    if (SendInput(static_cast<UINT>(std::size(events)), events, sizeof(INPUT))
        != std::size(events)) {
        events[0].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, events, sizeof(INPUT));
        return false;
    }
    return true;
}

TypeResult typeText(const ScratchWindow& scratch, std::wstring_view text, DWORD delayMs)
{
    for (wchar_t character : text) {
        const TypeResult result = sendCharacter(scratch, character);
        if (result != TypeResult::Success)
            return result;
        Sleep(delayMs);
        if (!scratchOwnsFocus(scratch))
            return TypeResult::FocusLost;
        if (!capsLockIsOff())
            return TypeResult::CapsLockOn;
    }
    return TypeResult::Success;
}

std::wstring readEditText(HWND edit)
{
    const int length = GetWindowTextLengthW(edit);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(edit, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(std::max(copied, 0)));
    return text;
}

enum class DrainResult { Match, Timeout, FocusLost };

DrainResult waitForExpectedText(const ScratchWindow& scratch, std::wstring_view expected)
{
    const ULONGLONG deadline = GetTickCount64() + kDrainMaxMs;
    while (GetTickCount64() < deadline) {
        if (!scratchOwnsFocus(scratch))
            return DrainResult::FocusLost;
        if (readEditText(scratch.edit) == expected)
            return DrainResult::Match;
        Sleep(25);
    }
    if (!scratchOwnsFocus(scratch))
        return DrainResult::FocusLost;
    return readEditText(scratch.edit) == expected ? DrainResult::Match : DrainResult::Timeout;
}

void printText(const wchar_t* label, std::wstring_view text)
{
    log(false,L"%ls: \"%.*ls\"\n", label, static_cast<int>(text.size()), text.data());
}

void printTotals(int checks, int failures)
{
    log(false,L"%d checks, %d failure(s)\n", checks, failures);
}

bool resetScratch(const ScratchWindow& scratch)
{
    // Navigation ends the current word. Escape may instead restore its raw
    // keys and leave a literal word active, leaking state into the next case.
    if (!sendVirtualKey(scratch, VK_END))
        return false;
    Sleep(10);
    if (!scratchOwnsFocus(scratch))
        return false;
    SetWindowTextW(scratch.edit, L"");
    SetFocus(scratch.edit);
    return scratchOwnsFocus(scratch);
}

struct Sample {
    const wchar_t* name;
    std::wstring_view raw;
    std::wstring_view expected;
    DWORD delayMs;
};

bool runSample(const ScratchWindow& scratch, const Sample& sample, int& checks)
{
    ++checks;
    const TypeResult typed = typeText(scratch, sample.raw, sample.delayMs);
    if (typed != TypeResult::Success) {
        if (typed == TypeResult::FocusLost)
            log(true, L"SKIP: focus left the probe window before completing %ls.\n", sample.name);
        else if (typed == TypeResult::CapsLockOn)
            log(true, L"SKIP: CapsLock turned on during %ls.\n", sample.name);
        else if (typed == TypeResult::ModifierHeld)
            log(true, L"SKIP: Shift, Ctrl, or Alt was held during %ls.\n", sample.name);
        else if (typed == TypeResult::UnsupportedKey)
            log(true, L"SKIP: the current keyboard layout cannot produce a key in %ls with Shift only.\n", sample.name);
        else
            log(true, L"FAIL: SendInput could not send a key in %ls.\n", sample.name);
        return false;
    }

    const DrainResult drained = waitForExpectedText(scratch, sample.expected);
    if (drained == DrainResult::FocusLost) {
        log(true, L"SKIP: focus left the probe window while waiting for %ls edits.\n",
                      sample.name);
        return false;
    }
    if (drained == DrainResult::Timeout) {
        const std::wstring actual = readEditText(scratch.edit);
        log(true, L"FAIL: %ls text mismatch.\n", sample.name);
        printText(L"Expected", sample.expected);
        printText(L"Actual  ", actual);
        return false;
    }
    log(false,L"PASS: %ls\n", sample.name);
    return true;
}

int runParentProbe(bool vni, bool vi)
{
    Handles handles;
    if (!launchHookChild(handles, vni, vi)) {
        log(true, L"SKIP: could not start the hidden hook child process.\n");
        printTotals(0, 1);
        return 2;
    }
    if (!waitForHookReady(handles)) {
        DWORD childCode = 0;
        GetExitCodeProcess(handles.child.hProcess, &childCode);
        log(true, L"SKIP: the child hook did not report Running (exit %lu).\n", childCode);
        printTotals(0, 1);
        return 2;
    }

    ScratchWindow scratch;
    if (!createScratchWindow(scratch, GetModuleHandleW(nullptr))) {
        log(true, L"FAIL: could not create the private scratch window.\n");
        printTotals(0, 1);
        return 1;
    }
    if (!acquireFocus(scratch)) {
        log(true, L"SKIP: Windows did not grant focus to the probe scratch field.\n");
        printTotals(0, 1);
        return 2;
    }
    if (!capsLockIsOff()) {
        log(true, L"SKIP: CapsLock is on; turn it off and rerun the probe.\n");
        printTotals(0, 1);
        return 2;
    }

    const std::array<Sample, 43> telexSamples{{
        {L"Vietnamese", L"hoom nay tooi ddeens New York ", L"hôm nay tôi đến New York ", 20},
        {L"German", L"Ich moechte schoene Aepfel und Wasser. ",
         L"Ich möchte schöne Äpfel und Wasser. ", 5},
        {L"Strasse -> Straße, burst", L"Strasse ", L"Straße ", 0},
        {L"Strassen -> Straßen", L"Strassen ", L"Straßen ", 1},
        {L"Gruesse -> Grüße", L"Gruesse ", L"Grüße ", 5},
        {L"Groesse -> Größe", L"Groesse ", L"Größe ", 1},
        {L"Fuss -> Fuß, burst", L"Fuss ", L"Fuß ", 0},
        {L"Grossstadt triple-s", L"Grossstadt ", L"Großstadt ", 5},
        {L"German ss exception: Wasser", L"Wasser ", L"Wasser ", 1},
        {L"German ss exception: Masse", L"Masse ", L"Masse ", 5},
        {L"German ss exception: Kuss, burst", L"Kuss ", L"Kuss ", 0},
        {L"German ss exception: muessen", L"muessen ", L"müssen ", 1},
        {L"German ss exception: aussehen", L"aussehen ", L"aussehen ", 5},
        {L"English sentence", L"The queen will address the blue guide. ",
         L"The queen will address the blue guide. ", 20},
        {L"English regression: axis, burst", L"axis ", L"axis ", 0},
        {L"English regression: beef", L"beef ", L"beef ", 1},
        {L"English regression: cars", L"cars ", L"cars ", 5},
        {L"English regression: cheers", L"cheers ", L"cheers ", 1},
        {L"English regression: hats, burst", L"hats ", L"hats ", 0},
        {L"English regression: hurts", L"hurts ", L"hurts ", 5},
        {L"English regression: moon", L"moon ", L"moon ", 1},
        {L"English regression: teen, burst", L"teen ", L"teen ", 0},
        {L"Vietnamese, German and English",
         L"hoom tooi ddeens Berlin; Ich moechte coffee ",
         L"hôm tôi đến Berlin; Ich möchte coffee ", 1},
        {L"Email stays literal", L"hoom@tooi.com ", L"hoom@tooi.com ", 1},
        {L"URL stays literal, burst", L"https://example.com/hoom ",
         L"https://example.com/hoom ", 0},
        {L"Windows path stays literal", L"C:\\users\\tooi\\hoom ",
         L"C:\\users\\tooi\\hoom ", 5},
        {L"Visible Backspace", L"tooi\b ", L"tô ", 1},
        {L"Escape restores original keys", L"tooi\x1b ", L"tooi ", 5},
        {L"Enter preserves order", L"tooi\nNew York ", L"tôi\r\nNew York ", 20},
        {L"Vietnamese: ww -> w", L"ww ", L"w ", 0},
        {L"Vietnamese: ww commits as w", L"ww ", L"w ", 5},
        {L"Literal w keeps English words intact", L"ww cars hats moon ", L"w cars hats moon ", 1},
        {L"Vietnamese: nữa live", L"nuawx", L"nữa", 0},
        {L"Vietnamese: nữa at separator", L"nuawx ", L"nữa ", 5},
        {L"Vietnamese: doubled modifier undo tww", L"tww ", L"tw ", 1},
        {L"Vietnamese: doubled modifier undo uww", L"uww ", L"uw ", 1},
        {L"Vietnamese: delayed mưa", L"muaw ", L"mưa ", 20},
        {L"Vietnamese: delayed dưa", L"duaw ", L"dưa ", 20},
        {L"Vietnamese: cứu", L"cuuws ", L"cứu ", 1},
        {L"Vietnamese: hữu", L"huuwx ", L"hữu ", 1},
        {L"English context then nữa", L"New York nuawx ", L"New York nữa ", 5},
        {L"Vietnamese: Backspace from nữa", L"nuawx\b ", L"nữ ", 1},
        {L"Vietnamese: Escape restores raw nuawx", L"nuawx\x1b ", L"nuawx ", 5},
    }};
    const std::array<Sample, 13> vniSamples{{
        {L"VNI: hôm nay tôi đến New York, burst", L"ho6m nay to6i d9e6n1 New York ",
         L"hôm nay tôi đến New York ", 0},
        {L"VNI: tiếng Việt", L"tie6ng1 Vie6t5 ", L"tiếng Việt ", 1},
        {L"VNI: người", L"ngu7o7i2 ", L"người ", 5},
        {L"VNI: giữa", L"giu7a4 ", L"giữa ", 20},
        {L"VNI: English stays literal", L"The queen will address the blue guide. ",
         L"The queen will address the blue guide. ", 1},
        {L"VNI: German still converts", L"Strasse und Gruesse ", L"Straße und Grüße ", 5},
        {L"VNI: digits compose Vietnamese", L"tie6ng1 Vie6t5 ", L"tiếng Việt ", 0},
        {L"VNI: visible Backspace", L"to6i\b ", L"tô ", 1},
        {L"VNI: Escape restores original keys", L"to6i\x1b ", L"to6i ", 5},
        {L"VNI: Enter preserves order", L"to6i\nNew York ", L"tôi\r\nNew York ", 20},
        {L"VNI: nữa", L"nua74 ", L"nữa ", 1},
        {L"VNI: cứu", L"cuu71 ", L"cứu ", 0},
        {L"VNI: quăng", L"qua8ng ", L"quăng ", 5},
    }};
    const std::array<Sample, 10> viSamples{{
        {L"Manual Telex: ww -> w live", L"ww", L"w", 0},
        {L"Manual Telex: ww commits as w", L"ww ", L"w ", 5},
        {L"Manual Telex: uww -> uw", L"uww ", L"uw ", 1},
        {L"Manual Telex: tww -> tw", L"tww ", L"tw ", 1},
        {L"Manual Telex: nữa live", L"nuawx", L"nữa", 0},
        {L"Manual Telex: nữa at separator", L"nuawx ", L"nữa ", 5},
        {L"Manual Telex: uppercase NỮA", L"NUAWX ", L"NỮA ", 1},
        {L"Manual Telex: mưa dưa cứu hữu", L"muaw duaw cuuws huuwx ",
         L"mưa dưa cứu hữu ", 20},
        {L"Manual Telex: Backspace from nữa", L"nuawx\b ", L"nữ ", 1},
        {L"Manual Telex: Enter preserves order", L"nuawx\nNew York ",
         L"nữa\r\nNew York ", 5},
    }};
    const std::span<const Sample> samples = vi ? std::span<const Sample>(viSamples)
                                         : vni ? std::span<const Sample>(vniSamples)
                                               : std::span<const Sample>(telexSamples);

    int checks = 0;
    int failures = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (i > 0 && !resetScratch(scratch)) {
            log(true, L"SKIP: focus changed while resetting the scratch field.\n");
            ++failures;
            break;
        }
        if (!scratchOwnsFocus(scratch)) {
            log(true, L"SKIP: focus left the probe window before typing.\n");
            ++failures;
            break;
        }
        if (!runSample(scratch, samples[i], checks)) {
            ++failures;
            if (!scratchOwnsFocus(scratch))
                break;
        }
    }

    if (vni || vi) {
        printTotals(checks, failures);
        return failures == 0 && checks == static_cast<int>(samples.size()) ? 0 : 1;
    }

    constexpr std::wstring_view stressRawPhrase =
        L"hoom tooi ddeens Berlin und moechte coffee; ";
    constexpr std::wstring_view stressExpectedPhrase =
        L"hôm tôi đến Berlin und möchte coffee; ";
    std::wstring stressRaw;
    std::wstring stressExpected;
    for (int i = 0; i < 20; ++i) {
        stressRaw += stressRawPhrase;
        stressExpected += stressExpectedPhrase;
    }
    if (failures == 0 && !resetScratch(scratch)) {
        log(true, L"SKIP: focus changed while resetting for repeated-line stress.\n");
        ++failures;
    }
    if (failures == 0 && scratchOwnsFocus(scratch)) {
        const Sample stress{L"20x mixed-language line at 0 ms", stressRaw,
                            stressExpected, 0};
        if (!runSample(scratch, stress, checks))
            ++failures;
    }

    printTotals(checks, failures);
    return failures == 0 && checks == static_cast<int>(samples.size()) + 1 ? 0 : 1;
}

} // namespace

int probeMain(int argc, char* argv[])
{
    if ((argc == 4 || (argc == 5 && (std::string(argv[4]) == "--vni"
                                     || std::string(argv[4]) == "--vi")))
        && std::string(argv[1]) == "--hook") {
        const auto widenAscii = [](const char* text) {
            std::wstring result;
            for (; *text; ++text)
                result.push_back(static_cast<unsigned char>(*text));
            return result;
        };
        const std::wstring ready = widenAscii(argv[2]);
        const std::wstring stop = widenAscii(argv[3]);
        const bool vni = argc == 5 && std::string(argv[4]) == "--vni";
        return runHookChild(ready.c_str(), stop.c_str(), vni,
                            argc == 5 && std::string(argv[4]) == "--vi");
    }
    const bool vni = argc == 2 && std::string(argv[1]) == "--vni";
    const bool vi = argc == 2 && std::string(argv[1]) == "--vi";
    if (argc != 1 && !vni && !vi) {
        log(true, L"Usage: windows_hook_probe.exe [--vni | --vi]\n");
        return 2;
    }
    return runParentProbe(vni, vi);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argc = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!arguments)
        return 2;
    std::vector<std::string> strings;
    for (int i = 0; i < argc; ++i) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, arguments[i], -1, nullptr, 0, nullptr, nullptr);
        std::string value(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, arguments[i], -1, value.data(), size, nullptr, nullptr);
        value.pop_back();
        strings.push_back(std::move(value));
    }
    LocalFree(arguments);
    std::vector<char*> pointers;
    for (auto& value : strings)
        pointers.push_back(value.data());
    return probeMain(argc, pointers.data());
}
