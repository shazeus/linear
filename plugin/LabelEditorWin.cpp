#include "LabelEditorWin.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace linear::plugin {
namespace {

constexpr wchar_t kWindowClass[] = L"LinearLabelEditorWindow";
constexpr wchar_t kBuiltInFont[] = L"Linear Technical (Built-in)";
constexpr int kTextEditId = 1001;
constexpr int kFontComboId = 1002;

struct EditorState {
    std::wstring text;
    std::wstring font;
    HWND textEdit{};
    HWND fontCombo{};
    bool applied{};
    bool done{};
};

std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), required);
    return result;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), required,
        nullptr, nullptr);
    return result;
}

int CALLBACK collectFont(
    const LOGFONTW* logicalFont, const TEXTMETRICW*, DWORD,
    LPARAM context) {
    if (logicalFont == nullptr || logicalFont->lfFaceName[0] == L'@') return 1;
    auto* fonts = reinterpret_cast<std::vector<std::wstring>*>(context);
    fonts->emplace_back(logicalFont->lfFaceName);
    return 1;
}

std::vector<std::wstring> installedFonts() {
    std::vector<std::wstring> fonts;
    HDC device = GetDC(nullptr);
    if (device != nullptr) {
        LOGFONTW filter{};
        filter.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(
            device, &filter, collectFont,
            reinterpret_cast<LPARAM>(&fonts), 0);
        ReleaseDC(nullptr, device);
    }
    std::sort(fonts.begin(), fonts.end());
    fonts.erase(std::unique(fonts.begin(), fonts.end()), fonts.end());
    return fonts;
}

void useInterfaceFont(HWND control) {
    SendMessageW(
        control, WM_SETFONT,
        reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

void createEditorControls(HWND window, EditorState& state) {
    HWND textLabel = CreateWindowExW(
        0, L"STATIC", L"Label text", WS_CHILD | WS_VISIBLE,
        24, 22, 420, 20, window, nullptr, nullptr, nullptr);
    state.textEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", state.text.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        24, 44, 424, 28, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTextEditId)),
        nullptr, nullptr);
    SendMessageW(state.textEdit, EM_SETLIMITTEXT, 255, 0);

    HWND fontLabel = CreateWindowExW(
        0, L"STATIC", L"Font family", WS_CHILD | WS_VISIBLE,
        24, 88, 420, 20, window, nullptr, nullptr, nullptr);
    state.fontCombo = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
            CBS_DROPDOWN | CBS_AUTOHSCROLL,
        24, 110, 424, 260, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFontComboId)),
        nullptr, nullptr);
    SendMessageW(state.fontCombo, CB_LIMITTEXT, LF_FACESIZE - 1, 0);
    SendMessageW(
        state.fontCombo, CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(kBuiltInFont));
    for (const auto& font : installedFonts()) {
        SendMessageW(
            state.fontCombo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(font.c_str()));
    }
    SetWindowTextW(
        state.fontCombo,
        state.font.empty() ? kBuiltInFont : state.font.c_str());

    HWND hint = CreateWindowExW(
        0, L"STATIC",
        L"Type directly, choose any installed font, then press Apply.",
        WS_CHILD | WS_VISIBLE,
        24, 148, 424, 20, window, nullptr, nullptr, nullptr);
    HWND apply = CreateWindowExW(
        0, L"BUTTON", L"Apply",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        270, 180, 84, 30, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
        nullptr, nullptr);
    HWND cancel = CreateWindowExW(
        0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        364, 180, 84, 30, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
        nullptr, nullptr);
    for (HWND control : {
             textLabel, state.textEdit, fontLabel, state.fontCombo,
             hint, apply, cancel}) {
        if (control != nullptr) useInterfaceFont(control);
    }
    SetFocus(state.textEdit);
    SendMessageW(state.textEdit, EM_SETSEL, 0, -1);
}

void applyEditor(HWND window, EditorState& state) {
    std::array<wchar_t, 256U> text{};
    std::array<wchar_t, LF_FACESIZE> font{};
    GetWindowTextW(state.textEdit, text.data(), static_cast<int>(text.size()));
    GetWindowTextW(state.fontCombo, font.data(), static_cast<int>(font.size()));
    state.text = text.data();
    state.font = font.data();
    if (state.font == kBuiltInFont) state.font.clear();
    state.applied = true;
    DestroyWindow(window);
}

LRESULT CALLBACK editorWindowProcedure(
    HWND window, UINT message, WPARAM wordParameter, LPARAM longParameter) {
    auto* state = reinterpret_cast<EditorState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation =
            reinterpret_cast<const CREATESTRUCTW*>(longParameter);
        state = static_cast<EditorState*>(creation->lpCreateParams);
        SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    switch (message) {
    case WM_CREATE:
        if (state != nullptr) createEditorControls(window, *state);
        return 0;
    case WM_COMMAND:
        if (state == nullptr) return 0;
        switch (LOWORD(wordParameter)) {
        case IDOK: applyEditor(window, *state); return 0;
        case IDCANCEL: DestroyWindow(window); return 0;
        default: break;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (state != nullptr) state->done = true;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wordParameter, longParameter);
}

HINSTANCE moduleInstance() noexcept {
    HMODULE module = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleInstance), &module);
    return module;
}

bool registerEditorClass(HINSTANCE instance) {
    static std::once_flag once;
    static bool registered = false;
    std::call_once(once, [&] {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = editorWindowProcedure;
        windowClass.hInstance = instance;
        windowClass.hCursor =
            LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.hbrBackground =
            reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = kWindowClass;
        registered = RegisterClassExW(&windowClass) != 0;
    });
    return registered;
}

} // namespace

bool showLabelEditor(LabelEditorValues& values) {
    const HINSTANCE instance = moduleInstance();
    if (instance == nullptr || !registerEditorClass(instance)) return false;
    EditorState state{};
    state.text = utf8ToWide(values.text);
    state.font = utf8ToWide(values.fontFamily);

    HWND parent = GetActiveWindow();
    if (parent == nullptr) parent = GetForegroundWindow();
    RECT parentBounds{};
    if (parent == nullptr || GetWindowRect(parent, &parentBounds) == FALSE) {
        parentBounds = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                        GetSystemMetrics(SM_CYSCREEN)};
    }
    constexpr int width = 488;
    constexpr int height = 260;
    const int x = parentBounds.left +
        ((parentBounds.right - parentBounds.left) - width) / 2;
    const int y = parentBounds.top +
        ((parentBounds.bottom - parentBounds.top) - height) / 2;
    HWND window = CreateWindowExW(
        WS_EX_DLGMODALFRAME, kWindowClass, L"Linear - Label Editor",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        x, y, width, height, parent, nullptr, instance, &state);
    if (window == nullptr) return false;

    const bool parentWasEnabled =
        parent != nullptr && IsWindowEnabled(parent) != FALSE;
    if (parentWasEnabled) EnableWindow(parent, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    while (!state.done) {
        const BOOL messageResult = GetMessageW(&message, nullptr, 0, 0);
        if (messageResult <= 0) break;
        if (IsDialogMessageW(window, &message) == FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (parentWasEnabled) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }
    if (!state.applied) return false;
    values.text = wideToUtf8(state.text);
    values.fontFamily = wideToUtf8(state.font);
    return true;
}

} // namespace linear::plugin
