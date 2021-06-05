#pragma once
#include <ShlObj.h>
#include <Windows.h>

#include "ComRef.hpp"
#include "guid.hpp"

class ShellExt :
	GlobalRefCounted,
	public RefCountedCOM<ShellExt, IShellExtInit, IContextMenu, IShellPropSheetExt>
{
public:
	ShellExt();
    ~ShellExt() override;

	// IUnknown methods
	STDMETHOD(QueryInterface) (REFIID riid, LPVOID* ppvObj);

	// IShellExtInit
	STDMETHODIMP Initialize(LPCITEMIDLIST, LPDATAOBJECT, HKEY) override;

	// IContextMenu
	STDMETHODIMP GetCommandString(UINT_PTR, UINT, UINT*, LPSTR, UINT) override;
	STDMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO) override;
	STDMETHODIMP QueryContextMenu(HMENU, UINT, UINT, UINT, UINT) override;

	// IShellPropSheetExt
	STDMETHODIMP AddPages(LPFNADDPROPSHEETPAGE, LPARAM) override;
	STDMETHODIMP ReplacePage(UINT, LPFNADDPROPSHEETPAGE, LPARAM) override {
		return E_NOTIMPL;
	}

protected:
	TCHAR m_szFile[MAX_PATH];
};
