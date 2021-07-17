#pragma once

#include <filesystem>

#include <ShlObj.h>
#include <Windows.h>

#include "ComRef.hpp"
#include "guid.hpp"
#include "ContextMenu.hpp"

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

private:
	ContextMenuItem QueryContextMenuFromCache();
	static ContextMenuItem CreateContextMenu_SingleFolder();
	static ContextMenuItem CreateContextMenu_MultiFolder();
	static ContextMenuItem CreateContextMenu_SinglePbo();
	static ContextMenuItem CreateContextMenu_MultiPbo();




protected:
	std::vector<std::filesystem::path> selectedFiles;
	ContextMenuItem contextMenu;
	uint32_t cmdFirst;
};

