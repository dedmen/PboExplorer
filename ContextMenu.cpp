#include <Windows.h>
#include "ContextMenu.hpp"


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
