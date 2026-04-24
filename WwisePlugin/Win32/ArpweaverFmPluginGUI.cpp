#include "ArpweaverFmPluginGUI.h"
#include "../resource.h"

#include <commdlg.h>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>
#include <uxtheme.h>
#include <vector>

#pragma comment(lib, "UxTheme.lib")

namespace
{
constexpr COLORREF kBgColor = RGB(15, 20, 24);
constexpr COLORREF kTextColor = RGB(240, 244, 245);
constexpr COLORREF kDimTextColor = RGB(240, 244, 245);
constexpr COLORREF kAccentColor = RGB(67, 213, 196);
constexpr COLORREF kPanelColor = RGB(29, 37, 43);
constexpr COLORREF kPanelHotColor = RGB(40, 52, 60);
constexpr COLORREF kBorderColor = RGB(67, 213, 196);

bool IsAccentStaticId(int in_ctrlId)
{
    switch (in_ctrlId)
    {
    case IDC_STATIC_TITLE:
    case IDC_STATIC_SECTION_GLOBAL:
    case IDC_STATIC_SECTION_FM:
    case IDC_STATIC_SECTION_CHORD:
    case IDC_STATIC_SECTION_SEQ:
    case IDC_STATIC_SECTION_FX:
        return true;
    default:
        return false;
    }
}

bool IsDimStaticId(int in_ctrlId)
{
    switch (in_ctrlId)
    {
    case IDC_STATIC_SUBTITLE:
    case IDC_STATIC_HINT:
    case IDC_STATIC_PATCH_FILE:
    case IDC_STATIC_PATCH_NAME:
        return true;
    default:
        return false;
    }
}

BOOL CALLBACK EnumChildFont(HWND in_hWnd, LPARAM in_lParam)
{
    ::SendMessageW(in_hWnd, WM_SETFONT, in_lParam, TRUE);
    return TRUE;
}

BOOL CALLBACK EnumChildTheme(HWND in_hWnd, LPARAM)
{
    ::SetWindowTheme(in_hWnd, L"", L"");
    return TRUE;
}

std::wstring FormatPatchInfo(int in_patchIndex, int in_patchCount, const std::wstring& in_patchName)
{
    if (in_patchCount <= 0)
        return L"Patch --/--";

    wchar_t buffer[256] = {};
    _snwprintf_s(buffer, _TRUNCATE, L"Patch %d/%d - %ls", in_patchIndex + 1, in_patchCount, in_patchName.c_str());
    return buffer;
}
}

AK_WWISE_PLUGIN_GUI_WINDOWS_BEGIN_POPULATE_TABLE(ArpweaverFmProp)
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_USE_SYSEX, "UseSysexPatch")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_CHORD_ON, "ChordOn")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_SEQ_ON, "ArpOn")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP1, "SeqStep1")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP2, "SeqStep2")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP3, "SeqStep3")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP4, "SeqStep4")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP5, "SeqStep5")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP6, "SeqStep6")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP7, "SeqStep7")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_STEP8, "SeqStep8")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_UNISON_ON, "UnisonOn")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_CHORUS_ON, "ChorusOn")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_DELAY_ON, "DelayOn")
    AK_WWISE_PLUGIN_GUI_WINDOWS_POP_ITEM(IDC_CHECK_REVERB_ON, "ReverbOn")
AK_WWISE_PLUGIN_GUI_WINDOWS_END_POPULATE_TABLE()

ArpweaverFmPluginGUI::ArpweaverFmPluginGUI()
{
    m_backgroundBrush = ::CreateSolidBrush(kBgColor);
}

ArpweaverFmPluginGUI::~ArpweaverFmPluginGUI()
{
    if (m_backgroundBrush)
        ::DeleteObject(m_backgroundBrush);
    if (m_titleFont)
        ::DeleteObject(m_titleFont);
    if (m_sectionFont)
        ::DeleteObject(m_sectionFont);
    if (m_bodyFont)
        ::DeleteObject(m_bodyFont);
}

bool ArpweaverFmPluginGUI::GetDialog(AK::Wwise::Plugin::eDialog in_eDialog, UINT& out_uiDialogID, AK::Wwise::Plugin::PopulateTableItem*& out_pTable) const
{
    switch (in_eDialog)
    {
    case AK::Wwise::Plugin::SettingsDialog:
        out_uiDialogID = IDD_ARPWEAVERFM_BIG;
        out_pTable = ArpweaverFmProp;
        return true;
    case AK::Wwise::Plugin::ContentsEditorDialog:
        out_uiDialogID = IDD_ARPWEAVERFM_SMALL;
        out_pTable = ArpweaverFmProp;
        return true;
    default:
        return false;
    }
}

bool ArpweaverFmPluginGUI::WindowProc(AK::Wwise::Plugin::eDialog in_eDialog, HWND in_hWnd, UINT in_message, WPARAM in_wParam, LPARAM in_lParam, LRESULT& out_lResult)
{
    switch (in_message)
    {
    case WM_INITDIALOG:
    {
        m_hDialog = in_hWnd;
        m_hPatchFile = ::GetDlgItem(in_hWnd, IDC_STATIC_PATCH_FILE);
        m_hPatchName = ::GetDlgItem(in_hWnd, IDC_STATIC_PATCH_NAME);
        m_hHint = ::GetDlgItem(in_hWnd, IDC_STATIC_HINT);
        ApplyDialogFonts();
        ApplyDialogTheme();
        if (in_eDialog == AK::Wwise::Plugin::SettingsDialog)
            ::SetTimer(in_hWnd, kUiTimerId, 180u, nullptr);
        RefreshSysexUi(true);
        out_lResult = 1;
        return true;
    }

    case WM_TIMER:
        if (in_wParam == kUiTimerId)
        {
            RefreshSysexUi(false);
            out_lResult = 0;
            return true;
        }
        break;

    case WM_COMMAND:
    {
        const WORD controlId = LOWORD(in_wParam);
        const WORD notifyCode = HIWORD(in_wParam);
        if (notifyCode == BN_CLICKED)
        {
            if (controlId == IDC_BTN_LOAD_SYSEX || controlId == IDC_BTN_IMPORT_SYSEX)
            {
                BrowseAndSetSysexPath(in_hWnd);
                out_lResult = 1;
                return true;
            }
            if (controlId == IDC_BTN_CLEAR_SYSEX)
            {
                const GUID platform = m_host.GetCurrentPlatform();
                m_propertySet.SetValueString(platform, "SysexFilePath", "");
                m_propertySet.SetValueBool(platform, "UseSysexPatch", false);
                m_propertySet.SetValueInt32(platform, "PatchIndex", 0);
                RefreshSysexUi(true);
                out_lResult = 1;
                return true;
            }
            if (controlId == IDC_BTN_PATCH_PREV)
            {
                StepPatchIndex(-1);
                out_lResult = 1;
                return true;
            }
            if (controlId == IDC_BTN_PATCH_NEXT)
            {
                StepPatchIndex(1);
                out_lResult = 1;
                return true;
            }
            if (controlId == IDC_CHECK_CHORD_ON || controlId == IDC_CHECK_SEQ_ON)
            {
                SyncChordSequencerExclusivity(controlId);
                out_lResult = 0;
                return false;
            }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        if (DrawButton(reinterpret_cast<const DRAWITEMSTRUCT*>(in_lParam)))
        {
            out_lResult = TRUE;
            return true;
        }
        break;
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = reinterpret_cast<HDC>(in_wParam);
        ::SetBkColor(hdc, kBgColor);
        ::SetTextColor(hdc, kTextColor);
        out_lResult = reinterpret_cast<LRESULT>(m_backgroundBrush);
        return true;
    }

    case WM_CTLCOLORBTN:
    {
        HDC hdc = reinterpret_cast<HDC>(in_wParam);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetBkColor(hdc, kBgColor);
        ::SetTextColor(hdc, kTextColor);
        out_lResult = reinterpret_cast<LRESULT>(m_backgroundBrush);
        return true;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(in_wParam);
        const int ctrlId = ::GetDlgCtrlID(reinterpret_cast<HWND>(in_lParam));
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetBkColor(hdc, kBgColor);
        ::SetTextColor(hdc, IsAccentStaticId(ctrlId) ? kAccentColor : (IsDimStaticId(ctrlId) ? kDimTextColor : kTextColor));
        out_lResult = reinterpret_cast<LRESULT>(m_backgroundBrush);
        return true;
    }

    case WM_ERASEBKGND:
    {
        RECT rc = {};
        ::GetClientRect(in_hWnd, &rc);
        ::FillRect(reinterpret_cast<HDC>(in_wParam), &rc, m_backgroundBrush);
        out_lResult = 1;
        return true;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps = {};
        HDC hdc = ::BeginPaint(in_hWnd, &ps);
        RECT rc = {};
        ::GetClientRect(in_hWnd, &rc);
        ::FillRect(hdc, &rc, m_backgroundBrush);
        ::EndPaint(in_hWnd, &ps);
        out_lResult = 0;
        return true;
    }

    case WM_DESTROY:
        if (in_eDialog == AK::Wwise::Plugin::SettingsDialog)
            ::KillTimer(in_hWnd, kUiTimerId);
        m_hDialog = nullptr;
        m_hPatchFile = nullptr;
        m_hPatchName = nullptr;
        m_hHint = nullptr;
        break;
    }

    out_lResult = 0;
    return false;
}

bool ArpweaverFmPluginGUI::Help(HWND, AK::Wwise::Plugin::eDialog, const char*) const
{
    return false;
}

void ArpweaverFmPluginGUI::ApplyDialogFonts()
{
    if (!m_hDialog)
        return;

    if (!m_titleFont)
        m_titleFont = ::CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
    if (!m_sectionFont)
        m_sectionFont = ::CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
    if (!m_bodyFont)
        m_bodyFont = ::CreateFontW(-12, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    ::EnumChildWindows(m_hDialog, EnumChildFont, reinterpret_cast<LPARAM>(m_bodyFont));
    if (const HWND hTitle = ::GetDlgItem(m_hDialog, IDC_STATIC_TITLE))
        ::SendMessageW(hTitle, WM_SETFONT, reinterpret_cast<WPARAM>(m_titleFont), TRUE);

    const int accentIds[] =
    {
        IDC_STATIC_SECTION_GLOBAL,
        IDC_STATIC_SECTION_FM,
        IDC_STATIC_SECTION_CHORD,
        IDC_STATIC_SECTION_SEQ,
        IDC_STATIC_SECTION_FX
    };
    for (const int ctrlId : accentIds)
    {
        if (const HWND hCtrl = ::GetDlgItem(m_hDialog, ctrlId))
            ::SendMessageW(hCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(m_sectionFont), TRUE);
    }
}

void ArpweaverFmPluginGUI::ApplyDialogTheme()
{
    if (!m_hDialog)
        return;

    ::SetWindowTheme(m_hDialog, L"", L"");
    ::EnumChildWindows(m_hDialog, EnumChildTheme, 0);
}

void ArpweaverFmPluginGUI::RefreshSysexUi(bool in_force)
{
    if (!m_hDialog)
        return;

    const GUID platform = m_host.GetCurrentPlatform();
    const char* pathUtf8 = m_propertySet.GetString(platform, "SysexFilePath");
    const std::string path = pathUtf8 ? pathUtf8 : "";
    const int patchIndex = m_propertySet.GetInt32(platform, "PatchIndex");
    const bool useSysex = m_propertySet.GetBool(platform, "UseSysexPatch");

    if (!in_force && path == m_lastSysexPath && patchIndex == m_lastPatchIndex && useSysex == m_lastUseSysex)
        return;

    m_lastSysexPath = path;
    m_lastPatchIndex = patchIndex;
    m_lastUseSysex = useSysex;

    m_patchFileText = path.empty() ? L"No SysEx file selected." : Utf8ToWide(path.c_str());
    m_patchNameText = L"Patch --/--";
    m_hintText = L"Load a DX7 single voice or 32-voice bank and browse patches here.";

    if (!path.empty())
    {
        std::vector<ArpweaverFmDx7::PatchData> patches;
        std::string formatName;
        if (ArpweaverFmDx7::LoadPatchesFromFile(path, patches, formatName) && !patches.empty())
        {
            const int safePatchIndex = ArpweaverFmDx7::ClampInt(patchIndex, 0, static_cast<int>(patches.size()) - 1);
            if (safePatchIndex != patchIndex)
                m_propertySet.SetValueInt32(platform, "PatchIndex", safePatchIndex);

            const auto macroPatch = ArpweaverFmDx7::ReducePatchToMacro(patches[static_cast<size_t>(safePatchIndex)]);
            if (useSysex)
            {
                ApplyPatchMacroToVisibleParameters(platform, macroPatch);
                m_propertySet.SetValueInt32(platform, "FmAlgorithmMacro", patches[static_cast<size_t>(safePatchIndex)].algorithm);
                m_propertySet.SetValueReal32(platform, "FmFeedback", static_cast<float>(patches[static_cast<size_t>(safePatchIndex)].feedback) / 7.0f);
            }

            const std::wstring patchName = Utf8ToWide(patches[static_cast<size_t>(safePatchIndex)].name.c_str());
            m_patchNameText = FormatPatchInfo(safePatchIndex, static_cast<int>(patches.size()), patchName);
            m_hintText = Utf8ToWide((formatName + (useSysex ? " active. Full DX patch data is serialized into the SoundBank." : " loaded. Enable Use SysEx Patch to play the imported voice.")).c_str());
        }
        else
        {
            m_patchNameText = L"Patch metadata unavailable";
            m_hintText = L"The file was loaded, but patch metadata could not be parsed cleanly.";
        }
    }

    if (m_hPatchFile)
        ::SetWindowTextW(m_hPatchFile, m_patchFileText.c_str());
    if (m_hPatchName)
        ::SetWindowTextW(m_hPatchName, m_patchNameText.c_str());
    if (m_hHint)
        ::SetWindowTextW(m_hHint, m_hintText.c_str());
}

void ArpweaverFmPluginGUI::ApplyPatchMacroToVisibleParameters(const GUID& in_guidPlatform, const ArpweaverFmDx7::MacroPatch& in_patch)
{
    m_propertySet.SetValueReal32(in_guidPlatform, "FmRatioCarrier", in_patch.ratioCarrier);
    m_propertySet.SetValueReal32(in_guidPlatform, "FmRatioMod", in_patch.ratioMod);
    m_propertySet.SetValueReal32(in_guidPlatform, "FmIndex", in_patch.index);
    m_propertySet.SetValueReal32(in_guidPlatform, "FmCarrierLevel", in_patch.carrierLevel);
    m_propertySet.SetValueReal32(in_guidPlatform, "FmModLevel", in_patch.modLevel);
    m_propertySet.SetValueReal32(in_guidPlatform, "FmFeedback", in_patch.feedback);
    m_propertySet.SetValueInt32(in_guidPlatform, "FmAlgorithmMacro", in_patch.algorithmMacro);
    m_propertySet.SetValueReal32(in_guidPlatform, "AmpAttack", in_patch.ampAttack);
    m_propertySet.SetValueReal32(in_guidPlatform, "AmpDecay", in_patch.ampDecay);
    m_propertySet.SetValueReal32(in_guidPlatform, "AmpSustain", in_patch.ampSustain);
    m_propertySet.SetValueReal32(in_guidPlatform, "AmpRelease", in_patch.ampRelease);
}

void ArpweaverFmPluginGUI::StepPatchIndex(int in_delta)
{
    const GUID platform = m_host.GetCurrentPlatform();
    const char* pathUtf8 = m_propertySet.GetString(platform, "SysexFilePath");
    const std::string path = pathUtf8 ? pathUtf8 : "";
    int patchIndex = m_propertySet.GetInt32(platform, "PatchIndex");

    if (path.empty())
    {
        m_propertySet.SetValueInt32(platform, "PatchIndex", ArpweaverFmDx7::ClampInt(patchIndex + in_delta, 0, 127));
        RefreshSysexUi(true);
        return;
    }

    std::vector<ArpweaverFmDx7::PatchData> patches;
    std::string formatName;
    if (!ArpweaverFmDx7::LoadPatchesFromFile(path, patches, formatName) || patches.empty())
    {
        m_propertySet.SetValueInt32(platform, "PatchIndex", ArpweaverFmDx7::ClampInt(patchIndex + in_delta, 0, 127));
        RefreshSysexUi(true);
        return;
    }

    const int patchCount = static_cast<int>(patches.size());
    int nextIndex = patchIndex + in_delta;
    if (nextIndex < 0)
        nextIndex = patchCount - 1;
    if (nextIndex >= patchCount)
        nextIndex = 0;

    m_propertySet.SetValueInt32(platform, "PatchIndex", nextIndex);
    RefreshSysexUi(true);
}

bool ArpweaverFmPluginGUI::BrowseAndSetSysexPath(HWND in_hWnd)
{
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = in_hWnd;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"SysEx Files (*.syx)\0*.syx;*.SYX\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = L"syx";

    if (!::GetOpenFileNameW(&ofn))
        return false;

    const std::string pathUtf8 = WideToUtf8(filePath);
    const GUID platform = m_host.GetCurrentPlatform();
    m_propertySet.SetValueString(platform, "SysexFilePath", pathUtf8.c_str());
    m_propertySet.SetValueBool(platform, "UseSysexPatch", true);
    m_propertySet.SetValueInt32(platform, "PatchIndex", 0);
    RefreshSysexUi(true);
    return true;
}

void ArpweaverFmPluginGUI::SyncSequencerFromStepChecks()
{
}

void ArpweaverFmPluginGUI::SyncChordSequencerExclusivity(WORD in_controlId)
{
    const GUID platform = m_host.GetCurrentPlatform();
    if (in_controlId == IDC_CHECK_CHORD_ON)
    {
        if (::SendMessageW(::GetDlgItem(m_hDialog, IDC_CHECK_CHORD_ON), BM_GETCHECK, 0, 0) == BST_CHECKED)
            m_propertySet.SetValueBool(platform, "ArpOn", false);
    }
    else if (in_controlId == IDC_CHECK_SEQ_ON)
    {
        if (::SendMessageW(::GetDlgItem(m_hDialog, IDC_CHECK_SEQ_ON), BM_GETCHECK, 0, 0) == BST_CHECKED)
            m_propertySet.SetValueBool(platform, "ChordOn", false);
    }
}

bool ArpweaverFmPluginGUI::DrawButton(const DRAWITEMSTRUCT* in_drawInfo)
{
    if (!in_drawInfo)
        return false;

    const UINT ctrlId = in_drawInfo->CtlID;
    const bool isKnownButton =
        (ctrlId == IDC_BTN_PATCH_PREV) ||
        (ctrlId == IDC_BTN_PATCH_NEXT) ||
        (ctrlId == IDC_BTN_LOAD_SYSEX) ||
        (ctrlId == IDC_BTN_IMPORT_SYSEX) ||
        (ctrlId == IDC_BTN_CLEAR_SYSEX);

    if (!isKnownButton)
        return false;

    RECT rc = in_drawInfo->rcItem;
    HDC hdc = in_drawInfo->hDC;
    const bool selected = (in_drawInfo->itemState & ODS_SELECTED) != 0;
    const bool focused = (in_drawInfo->itemState & ODS_FOCUS) != 0;
    const bool checked = (::SendMessageW(in_drawInfo->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);
    const LONG_PTR style = ::GetWindowLongPtrW(in_drawInfo->hwndItem, GWL_STYLE);
    const bool isCheckbox = (style & BS_AUTOCHECKBOX) == BS_AUTOCHECKBOX || (style & BS_CHECKBOX) == BS_CHECKBOX;

    const COLORREF fill = checked ? RGB(24, 62, 61) : (selected ? kPanelHotColor : kPanelColor);
    const COLORREF border = (checked || focused) ? kBorderColor : RGB(68, 78, 85);
    HBRUSH fillBrush = ::CreateSolidBrush(fill);
    HBRUSH borderBrush = ::CreateSolidBrush(border);
    ::FillRect(hdc, &rc, fillBrush);
    ::FrameRect(hdc, &rc, borderBrush);
    ::DeleteObject(fillBrush);
    ::DeleteObject(borderBrush);

    wchar_t text[128] = {};
    ::GetWindowTextW(in_drawInfo->hwndItem, text, static_cast<int>(std::size(text)));

    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, kTextColor);

    RECT textRc = rc;
    if (isCheckbox)
    {
        RECT box = rc;
        box.right = box.left + 14;
        box.top += 2;
        box.bottom -= 2;
        HBRUSH boxBrush = ::CreateSolidBrush(checked ? kAccentColor : kBgColor);
        HBRUSH boxBorder = ::CreateSolidBrush(checked ? kAccentColor : RGB(90, 102, 110));
        ::FillRect(hdc, &box, boxBrush);
        ::FrameRect(hdc, &box, boxBorder);
        ::DeleteObject(boxBrush);
        ::DeleteObject(boxBorder);
        textRc.left = box.right + 6;
        ::DrawTextW(hdc, text, -1, &textRc, DT_VCENTER | DT_SINGLELINE | DT_LEFT);
    }
    else
    {
        ::DrawTextW(hdc, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (focused)
    {
        RECT focusRc = rc;
        ::InflateRect(&focusRc, -3, -3);
        ::DrawFocusRect(hdc, &focusRc);
    }

    return true;
}

void ArpweaverFmPluginGUI::TraceGuiEvent(const char* in_text) const
{
    if (!in_text)
        return;

    wchar_t tempPath[MAX_PATH] = {};
    if (::GetTempPathW(MAX_PATH, tempPath) == 0)
        return;

    std::filesystem::path path(tempPath);
    path /= L"ArpweaverFm_gui_trace.log";
    std::ofstream file(path, std::ios::app);
    if (file)
        file << in_text << '\n';
}

std::string ArpweaverFmPluginGUI::WideToUtf8(const wchar_t* in_wideText)
{
    if (!in_wideText)
        return {};

    const int required = ::WideCharToMultiByte(CP_UTF8, 0, in_wideText, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0)
        return {};

    std::string out(static_cast<size_t>(required), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, in_wideText, -1, out.data(), required, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0')
        out.pop_back();
    return out;
}

std::wstring ArpweaverFmPluginGUI::Utf8ToWide(const char* in_utf8Text)
{
    if (!in_utf8Text)
        return {};

    const int required = ::MultiByteToWideChar(CP_UTF8, 0, in_utf8Text, -1, nullptr, 0);
    if (required <= 0)
        return {};

    std::wstring out(static_cast<size_t>(required), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, in_utf8Text, -1, out.data(), required);
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

AK_ADD_PLUGIN_CLASS_TO_CONTAINER(
    ArpweaverFm,
    ArpweaverFmPluginGUI,
    ArpweaverFmSource
);
