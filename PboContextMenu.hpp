#pragma once
#define NOMINMAX
#include <ShlObj.h>
#include <vector>
#include <Windows.h>

#include "ComRef.hpp"

class PboFolder;

class PboContextMenu :
    GlobalRefCounted,
	public RefCountedCOM<PboContextMenu,
     IContextMenu
>
{
public:
    PboContextMenu(PboFolder* folder, HWND hwnd,
        LPCITEMIDLIST pidlRoot, UINT cidl, LPCITEMIDLIST* apidl);

    ~PboContextMenu();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    // IContextMenu
    HRESULT STDMETHODCALLTYPE QueryContextMenu(
        HMENU hmenu, UINT indexMenu, UINT idCmdFirst,
        UINT idCmdLast, UINT uFlags);
    HRESULT STDMETHODCALLTYPE InvokeCommand(LPCMINVOKECOMMANDINFO pici);
    HRESULT STDMETHODCALLTYPE GetCommandString(
        UINT_PTR idCmd, UINT uFlags, UINT* pwReserved,
        LPSTR pszName, UINT cchMax);

private:
    ComRef<PboFolder> m_folder;
    HWND m_hwnd;
    CoTaskMemRefS<ITEMIDLIST> m_pidlRoot;
    std::vector<CoTaskMemRefS<ITEMIDLIST>> m_apidl;
};
