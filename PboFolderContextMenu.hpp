#pragma once

#define NOMINMAX
#include <ShlObj.h>
#include <vector>

import ComRef;

class PboFolder;

class PboFolderContextMenu :
    GlobalRefCounted,
    public RefCountedCOM<PboFolderContextMenu,
    IContextMenu
    >
{
public:
    PboFolderContextMenu(PboFolder* folder, HWND hwnd); // ,LPCITEMIDLIST pidlRoot, UINT cidl, LPCITEMIDLIST* apidl

    ~PboFolderContextMenu() override;

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
    //CoTaskMemRefS<ITEMIDLIST> m_pidlRoot;
    ComRef<IContextMenu> m_DefCtxMenu;
};