#pragma once
#define NOMINMAX
#include <ShlObj.h>
#include <Windows.h>

import std;
import std.compat;
import ComRef;

class PboFolder;

//#TODO rename PboFileContextMenu
class PboContextMenu :
    GlobalRefCounted,
	public RefCountedCOM<PboContextMenu,
     IContextMenu
>
{
public:
    PboContextMenu(PboFolder* folder, HWND hwnd,
        LPCITEMIDLIST pidlRoot, UINT cidl, LPCITEMIDLIST* apidl);

    ~PboContextMenu() override;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    // IContextMenu
    HRESULT STDMETHODCALLTYPE QueryContextMenu(
        HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
        UINT idCmdLast, UINT uFlags) override;
    HRESULT STDMETHODCALLTYPE InvokeCommand(LPCMINVOKECOMMANDINFO pici) override;
    HRESULT STDMETHODCALLTYPE GetCommandString(
        UINT_PTR idCmd, UINT uFlags, UINT* pwReserved,
        LPSTR pszName, UINT cchMax) override;

private:
    ComRef<PboFolder> m_folder;
    HWND m_hwnd;
    CoTaskMemRefS<ITEMIDLIST> m_pidlRoot;
    std::vector<CoTaskMemRefS<ITEMIDLIST>> m_apidl;
    ComRef<IContextMenu> m_DefCtxMenu;
};
