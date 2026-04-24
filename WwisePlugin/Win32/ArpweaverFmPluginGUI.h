#pragma once

#include "../ArpweaverFmPlugin.h"
#include "../Dx7SysexSupport.h"
#include <AK/Wwise/Plugin/GUIWindows.h>
#include <AK/Wwise/Plugin/PluginMFCWindows.h>
#include <string>

#if ((defined(_WIN32) || defined(_WIN64) || defined(WINAPI_FAMILY)) && ((defined(_AFXDLL) && _AFXDLL) || defined(_MFC_BLD) || defined(__AFX_H__)) && __has_include(<afxwin.h>))
using ArpweaverFmGUIWindowsBase = AK::Wwise::Plugin::PluginMFCWindows<>;
#else
struct ArpweaverFmGUIWindowsBase {};
#endif

class ArpweaverFmPluginGUI final
    : public ArpweaverFmGUIWindowsBase
    , public AK::Wwise::Plugin::RequestHost
    , public AK::Wwise::Plugin::RequestPropertySet
    , public AK::Wwise::Plugin::GUIWindows
{
public:
    ArpweaverFmPluginGUI();
    ~ArpweaverFmPluginGUI() override;

    bool GetDialog(AK::Wwise::Plugin::eDialog in_eDialog, UINT& out_uiDialogID, AK::Wwise::Plugin::PopulateTableItem*& out_pTable) const override;
    bool WindowProc(AK::Wwise::Plugin::eDialog in_eDialog, HWND in_hWnd, UINT in_message, WPARAM in_wParam, LPARAM in_lParam, LRESULT& out_lResult) override;
    bool Help(HWND in_hWnd, AK::Wwise::Plugin::eDialog in_eDialog, const char* in_szLanguageCode) const override;

private:
    static constexpr UINT_PTR kUiTimerId = 0x7711u;

    void ApplyDialogFonts();
    void ApplyDialogTheme();
    void RefreshSysexUi(bool in_force);
    void ApplyPatchMacroToVisibleParameters(const GUID& in_guidPlatform, const ArpweaverFmDx7::MacroPatch& in_patch);
    void StepPatchIndex(int in_delta);
    bool BrowseAndSetSysexPath(HWND in_hWnd);
    void SyncSequencerFromStepChecks();
    void SyncChordSequencerExclusivity(WORD in_controlId);
    bool DrawButton(const DRAWITEMSTRUCT* in_drawInfo);
    void TraceGuiEvent(const char* in_text) const;

    static std::string WideToUtf8(const wchar_t* in_wideText);
    static std::wstring Utf8ToWide(const char* in_utf8Text);

    HWND m_hDialog = nullptr;
    HWND m_hPatchFile = nullptr;
    HWND m_hPatchName = nullptr;
    HWND m_hHint = nullptr;
    HBRUSH m_backgroundBrush = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_sectionFont = nullptr;
    HFONT m_bodyFont = nullptr;
    std::wstring m_patchFileText = L"No SysEx file selected.";
    std::wstring m_patchNameText = L"Patch --/--";
    std::wstring m_hintText = L"Load a DX7 single voice or 32-voice bank and browse patches here.";
    std::string m_lastSysexPath;
    int m_lastPatchIndex = -1;
    bool m_lastUseSysex = false;
};
