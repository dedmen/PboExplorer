module;

#include <Windows.h>
#include <windef.h>
#include <ShObjIdl_core.h>


export module ContextMenu;

import std;
import std.compat;


export class ContextMenuItem {
	std::wstring text;
	std::wstring verb;
	std::vector<ContextMenuItem> children;
	std::function<HRESULT(const std::vector<std::filesystem::path>&)> invoke;
	// Remembered from last insert
	uint32_t ownPosition;
	uint16_t ownCmd;
public:

	ContextMenuItem() : invoke([](const std::vector<std::filesystem::path>&) { return E_NOTIMPL; }) {};
	ContextMenuItem(std::wstring text, std::wstring verb, std::function<HRESULT(const std::vector<std::filesystem::path>&)> invoke) : text(text), verb(verb), invoke(invoke) {};

	void AddChild(ContextMenuItem child) {
		children.emplace_back(std::move(child));
	}

	// returns how many items were inserted including submenus
	uint16_t InsertIntoMenu(HMENU parentMenu, uint32_t& position, uint32_t& id);
	std::wstring GetVerb(uint32_t cmd) const;
	HRESULT Invoke(const std::vector<std::filesystem::path>& files) const { return invoke(files); }
	HRESULT InvokeByVerb(std::wstring_view verb, const std::vector<std::filesystem::path>& files) const {
		if (verb == this->verb)
			return invoke(files);

		for (const auto& it : children)
			if (SUCCEEDED(it.InvokeByVerb(verb, files)))
				return S_OK;
		return E_INVALIDARG;
	}

	HRESULT InvokeByCmd(uint16_t cmd, const std::vector<std::filesystem::path>& files) const {
		if (cmd == ownCmd)
			return invoke(files);

		for (const auto& it : children)
			if (SUCCEEDED(it.InvokeByCmd(cmd, files)))
				return S_OK;
		return E_INVALIDARG;
	}

	bool HasChildren() const {
		return !children.empty();
	}
};




uint16_t ContextMenuItem::InsertIntoMenu(HMENU parentMenu, uint32_t& position, uint32_t& id)
{
	if (text.empty()) return 0; // we don't exist

	ownPosition = position++;

	uint16_t subItemCount = 0;

	MENUITEMINFOW info{};
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STRING; // text itself
	info.fMask |= MIIM_DATA; // dunno if useful
	info.fMask |= MIIM_ID; // dunno if useful
	info.wID = ownCmd = id++;
	info.dwItemData = (uintptr_t)"testo";
	info.dwTypeData = text.data();
	info.cch = text.length();

	if (!children.empty()) {
		HMENU submenu = CreatePopupMenu();
		uint32_t childPosition = 0;

		for (auto& child : children)
			subItemCount += child.InsertIntoMenu(submenu, childPosition, id);


		info.fMask |= MIIM_SUBMENU;
		info.hSubMenu = submenu;
	}

	auto hres = InsertMenuItem(parentMenu, ownPosition, true, &info);

	return subItemCount + 1;
}

std::wstring ContextMenuItem::GetVerb(uint32_t cmd) const
{
	if (ownCmd == cmd)
		return verb;

	for (auto& child : children) {
		if (child.ownCmd != cmd && child.children.empty())
			continue;
		auto res = child.GetVerb(cmd);
		if (!res.empty())
			return res;
	}

	return {};
}
