#pragma once
#include <ShlObj.h>
#include <Windows.h>

#include "ComRef.hpp"
#include "guid.hpp"

class ShellExt :
	GlobalRefCounted,
	public RefCountedCOM<ShellExt, IShellExtInit, IContextMenu>
{
public:
	ShellExt();
	virtual ~ShellExt();

	// IUnknown methods
	STDMETHOD(QueryInterface) (REFIID riid, LPVOID* ppvObj);

	// IShellExtInit
	STDMETHODIMP Initialize(LPCITEMIDLIST, LPDATAOBJECT, HKEY);

	// IContextMenu
	STDMETHODIMP GetCommandString(UINT_PTR, UINT, UINT*, LPSTR, UINT);
	STDMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO);
	STDMETHODIMP QueryContextMenu(HMENU, UINT, UINT, UINT, UINT);

protected:
	TCHAR m_szFile[MAX_PATH];
};
