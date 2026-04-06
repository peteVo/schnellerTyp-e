#include "framework.h"
#include "schnellerTyp-e.h"
#include <shellapi.h>

#define MAX_LOADSTRING 100
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_APP_ICON 1001
#define ID_TRAY_EXIT 1002
#define ID_TRAY_TOGGLE 1003
#define WM_TOGGLE_HOTKEY (WM_USER + 2) // Custom message for Alt+Z

// Global Variables:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING] = L"schnellerTyp-e";
WCHAR szWindowClass[MAX_LOADSTRING] = L"SCHNELLERTYPECLASS";

NOTIFYICONDATA nid;
HHOOK hKeyboardHook;
HHOOK hMouseHook;
bool isAppEnabled = true;
HWND g_hWnd = NULL;           // Tracks our hidden application window
HWND lastActiveWindow = NULL; // Tracks the user's current window

// Character tracking
char lastCharPressed = 0;
bool lastCharWasUpper = false;

// State tracking for the "Undo" (aee -> ae) feature
enum ReplaceState {
    STATE_NONE,
    STATE_A_UMLAUT_LOWER,
    STATE_A_UMLAUT_UPPER,
    STATE_O_UMLAUT_LOWER,
    STATE_O_UMLAUT_UPPER,
    STATE_U_UMLAUT_LOWER,
    STATE_U_UMLAUT_UPPER,
    STATE_ESZETT
};
ReplaceState currentState = STATE_NONE;

// Forward declarations:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK    LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

// Helper: Sends backspaces, then up to two characters
void ReplaceText(int backspaces, WCHAR char1, WCHAR char2 = 0) {
    INPUT inputs[6] = { 0 };
    int idx = 0;

    // 1. Send Backspaces
    for (int i = 0; i < backspaces; ++i) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_BACK;
        idx++;
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_BACK;
        inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
        idx++;
    }

    // 2. Send First Character
    if (char1 != 0) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wScan = char1;
        inputs[idx].ki.dwFlags = KEYEVENTF_UNICODE;
        idx++;
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wScan = char1;
        inputs[idx].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        idx++;
    }

    // 3. Send Second Character (for undoing escapes)
    if (char2 != 0) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wScan = char2;
        inputs[idx].ki.dwFlags = KEYEVENTF_UNICODE;
        idx++;
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wScan = char2;
        inputs[idx].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        idx++;
    }

    SendInput(idx, inputs, sizeof(INPUT));
}

// The Mouse Hook: Wipes memory if you click somewhere else
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && isAppEnabled) {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN) {
            currentState = STATE_NONE;
            lastCharPressed = 0;
        }
    }
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

// The Main Keyboard Hook
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;

        // Ignore injected events to prevent infinite loops from our own SendInput
        if ((pKeyBoard->flags & LLKHF_INJECTED) == 0) {

            // --- GLOBAL HOTKEY: ALT + Z ---
            if (wParam == WM_SYSKEYDOWN) {
                if (pKeyBoard->vkCode == 'Z') {
                    if (GetAsyncKeyState(VK_MENU) & 0x8000) { // If ALT is held
                        if (g_hWnd) PostMessage(g_hWnd, WM_TOGGLE_HOTKEY, 0, 0);
                        return 1; // Block default Alt+Z behavior
                    }
                }
            }

            if (isAppEnabled && wParam == WM_KEYDOWN) {

                // --- CONTEXT AWARENESS: WINDOW SWITCH DETECTION ---
                HWND currentWindow = GetForegroundWindow();
                if (currentWindow != lastActiveWindow) {
                    currentState = STATE_NONE;
                    lastCharPressed = 0;
                    lastActiveWindow = currentWindow;
                }

                DWORD vkCode = pKeyBoard->vkCode;

                // --- NAVIGATION AWARENESS ---
                // Arrow keys, backspace, enter, space, etc., wipe the memory buffer
                if (vkCode == VK_LEFT || vkCode == VK_RIGHT || vkCode == VK_UP || vkCode == VK_DOWN ||
                    vkCode == VK_BACK || vkCode == VK_RETURN || vkCode == VK_SPACE || vkCode == VK_ESCAPE || vkCode == VK_TAB) {
                    currentState = STATE_NONE;
                    lastCharPressed = 0;
                    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
                }

                // Determine character and smart capitalization
                char currentChar = 0;
                bool isShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                bool isCapsOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                bool isUpper = isShiftPressed ^ isCapsOn;

                if (vkCode >= 'A' && vkCode <= 'Z') {
                    currentChar = (char)vkCode + 32; // Normalize to lowercase for logic checks
                }

                if (currentChar != 0) {
                    bool handled = false;

                    // --- 1. UNDO (TELEX) ACTIONS (e.g., aee -> ae) ---
                    if (currentState == STATE_A_UMLAUT_LOWER && currentChar == 'e') {
                        ReplaceText(1, 'a', isUpper ? 'E' : 'e');
                        currentState = STATE_NONE; handled = true; lastCharPressed = 'e'; lastCharWasUpper = isUpper;
                    }
                    else if (currentState == STATE_A_UMLAUT_UPPER && currentChar == 'e') {
                        ReplaceText(1, 'A', isUpper ? 'E' : 'e');
                        currentState = STATE_NONE; handled = true; lastCharPressed = 'e'; lastCharWasUpper = isUpper;
                    }
                    else if (currentState == STATE_O_UMLAUT_LOWER && currentChar == 'e') {
                        ReplaceText(1, 'o', isUpper ? 'E' : 'e');
                        currentState = STATE_NONE; handled = true; lastCharPressed = 'e'; lastCharWasUpper = isUpper;
                    }
                    else if (currentState == STATE_O_UMLAUT_UPPER && currentChar == 'e') {
                        ReplaceText(1, 'O', isUpper ? 'E' : 'e');
                        currentState = STATE_NONE; handled = true; lastCharPressed = 'e'; lastCharWasUpper = isUpper;
                    }
                    else if (currentState == STATE_U_UMLAUT_LOWER && currentChar == 'e') {
                        ReplaceText(1, 'u', isUpper ? 'E' : 'e');
                        currentState = STATE_NONE; handled = true; lastCharPressed = 'e'; lastCharWasUpper = isUpper;
                    }
                    else if (currentState == STATE_U_UMLAUT_UPPER && currentChar == 'e') {
                        ReplaceText(1, 'U', isUpper ? 'E' : 'e');
                        currentState = STATE_NONE; handled = true; lastCharPressed = 'e'; lastCharWasUpper = isUpper;
                    }
                    else if (currentState == STATE_ESZETT && currentChar == 's') {
                        ReplaceText(1, lastCharWasUpper ? 'S' : 's', isUpper ? 'S' : 's');
                        currentState = STATE_NONE; handled = true; lastCharPressed = 's'; lastCharWasUpper = isUpper;
                    }

                    // --- 2. NEW UMLAUT COMBINATIONS (e.g., a + e -> ä) ---
                    else if (lastCharPressed == 'a' && currentChar == 'e') {
                        if (lastCharWasUpper) {
                            ReplaceText(1, 0x00C4); // Ä
                            currentState = STATE_A_UMLAUT_UPPER;
                        }
                        else {
                            ReplaceText(1, 0x00E4); // ä
                            currentState = STATE_A_UMLAUT_LOWER;
                        }
                        handled = true;
                    }
                    else if (lastCharPressed == 'o' && currentChar == 'e') {
                        if (lastCharWasUpper) {
                            ReplaceText(1, 0x00D6); // Ö
                            currentState = STATE_O_UMLAUT_UPPER;
                        }
                        else {
                            ReplaceText(1, 0x00F6); // ö
                            currentState = STATE_O_UMLAUT_LOWER;
                        }
                        handled = true;
                    }
                    else if (lastCharPressed == 'u' && currentChar == 'e') {
                        if (lastCharWasUpper) {
                            ReplaceText(1, 0x00DC); // Ü
                            currentState = STATE_U_UMLAUT_UPPER;
                        }
                        else {
                            ReplaceText(1, 0x00FC); // ü
                            currentState = STATE_U_UMLAUT_LOWER;
                        }
                        handled = true;
                    }
                    else if (lastCharPressed == 's' && currentChar == 's') {
                        ReplaceText(1, 0x00DF); // ß (Standard lowercase eszett used for both cases)
                        currentState = STATE_ESZETT;
                        handled = true;
                    }

                    // --- 3. CLEANUP AND STATE MANAGEMENT ---
                    if (handled) {
                        if (currentState != STATE_NONE) lastCharPressed = 0;
                        return 1; // Block the natural keystroke
                    }
                    else {
                        // Standard typing continues
                        lastCharPressed = currentChar;
                        lastCharWasUpper = isUpper;
                        currentState = STATE_NONE;
                    }
                }
                else {
                    // Non-letter key pressed
                    lastCharPressed = 0;
                    currentState = STATE_NONE;
                }
            }
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    // Set the global hooks
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

    MSG msg;
    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup before exit
    UnhookWindowsHookEx(hKeyboardHook);
    UnhookWindowsHookEx(hMouseHook);
    Shell_NotifyIcon(NIM_DELETE, &nid);

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SCHNELLERTYPE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return FALSE;

    g_hWnd = hWnd; // Store window handle globally for PostMessage

    // Setup System Tray Icon
    memset(&nid, 0, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
    wcscpy_s(nid.szTip, L"schnellerTyp-e (Active) - Alt+Z to Toggle");

    Shell_NotifyIcon(NIM_ADD, &nid);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TOGGLE_HOTKEY: // Handle Alt+Z
        isAppEnabled = !isAppEnabled;
        if (isAppEnabled) {
            wcscpy_s(nid.szTip, L"schnellerTyp-e (Active) - Alt+Z to Toggle");
        }
        else {
            wcscpy_s(nid.szTip, L"schnellerTyp-e (Paused) - Alt+Z to Toggle");
        }
        Shell_NotifyIcon(NIM_MODIFY, &nid);
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();

            if (isAppEnabled) {
                AppendMenu(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"Disable");
            }
            else {
                AppendMenu(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"Enable");
            }
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_TRAY_TOGGLE:
            PostMessage(hWnd, WM_TOGGLE_HOTKEY, 0, 0); // Reuse toggle logic
            break;
        case ID_TRAY_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}