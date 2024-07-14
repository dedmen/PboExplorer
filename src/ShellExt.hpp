#pragma once

import std;

#define NOMINMAX
#include <ShlObj.h>
#include <Windows.h>

import ComRef;
#include "guid.hpp"
import ContextMenu;

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

	const auto& GetSelectedFiles() const { return selectedFiles; }

private:
	std::vector<ContextMenuItem> QueryContextMenuFromCache();
	static std::vector<ContextMenuItem> CreateContextMenu_SingleFolder();
	static std::vector<ContextMenuItem> CreateContextMenu_MultiFolder();
	static std::vector<ContextMenuItem> CreateContextMenu_SinglePbo();
	static std::vector<ContextMenuItem> CreateContextMenu_MultiPbo();




protected:
	std::vector<std::filesystem::path> selectedFiles;
	std::vector<ContextMenuItem> contextMenu;
	uint32_t cmdFirst;
};

