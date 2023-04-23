#include "PboFolderContextMenu.hpp"
#include <shlwapi.h>

#include "FileWatcher.hpp"
#include "PboFolder.hpp"
#include "PboPidl.hpp"
import TempDiskFile;
#include "Util.hpp"

#include "DebugLogger.hpp"
#include "PboPatcher.hpp"

#ifndef SEE_MASK_NOASYNC
#define SEE_MASK_NOASYNC 0x00000100
#endif

#ifndef CMF_EXTENDEDVERBS
#define CMF_EXTENDEDVERBS 0x00000100
#endif

#ifndef CMIC_MASK_SHIFT_DOWN
#define CMIC_MASK_SHIFT_DOWN 0x10000000
#endif

import Tracy;

PboFolderContextMenu::PboFolderContextMenu(PboFolder* folder, HWND hwnd) //, LPCITEMIDLIST pidlRoot, UINT cidl, LPCITEMIDLIST* apidl
{
    m_folder = folder;
    m_hwnd = hwnd;
    //m_pidlRoot = ILClone(pidlRoot);

    //auto dcm = new DEFCONTEXTMENU{ hwnd, nullptr, pidlRoot, folder, cidl, apidl, nullptr,0 ,nullptr };
    //SHCreateDefaultContextMenu(dcm, IID_IContextMenu, m_DefCtxMenu.AsQueryInterfaceTarget());
}

PboFolderContextMenu::~PboFolderContextMenu()
{
}


// IUnknown
HRESULT PboFolderContextMenu::QueryInterface(REFIID riid, void** ppvObject)
{
    ProfilingScope pScope;
    pScope.SetValue(DebugLogger::GetGUIDName(riid).first);
    DebugLogger_OnQueryInterfaceEntry(riid);

    if (COMJoiner::QueryInterfaceJoiner(riid, ppvObject)) {
        AddRef();
        return S_OK;
    }
    else
    {
        DebugLogger_OnQueryInterfaceExitUnhandled(riid);
        *ppvObject = nullptr;
        return(E_NOINTERFACE);
    }

    AddRef();
    return(S_OK);
}


enum
{
    CONTEXT_PASTE,
    CONTEXT_QTY,
};

#define CONTEXT_TEXT_SIZE 32
static wchar_t contextText[CONTEXT_QTY][CONTEXT_TEXT_SIZE] = { L"Paste" };

static const wchar_t* getContextText(int idx)
{
    //do
    //{
    //    if (contextText[0][0]) break;
    //
    //    HMODULE mod = GetModuleHandle(L"shell32.dll");
    //
    //    LoadString(mod, 8496, contextText[CONTEXT_OPEN], CONTEXT_TEXT_SIZE);
    //
    //    //LoadString(mod, 5377, contextText[CONTEXT_OPEN_WITH], CONTEXT_TEXT_SIZE);
    //
    //    HMENU menu = LoadMenu(mod, MAKEINTRESOURCE(210));
    //    if (!menu) break;
    //    MENUITEMINFO mii;
    //    mii.cbSize = (UINT)sizeof(mii);
    //    mii.fMask = MIIM_STRING;
    //    mii.cch = CONTEXT_TEXT_SIZE;
    //    mii.dwTypeData = contextText[CONTEXT_COPY];
    //    GetMenuItemInfo(menu, 25, FALSE, &mii);
    //    DestroyMenu(menu);
    //} while (0);

    return(contextText[idx]);
}

// IContextMenu
HRESULT PboFolderContextMenu::QueryContextMenu(
    HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    //return m_DefCtxMenu->QueryContextMenu(hmenu, indexMenu, idCmdFirst, idCmdLast, uFlags);

    // menu already has paste

    //InsertMenu(hmenu, indexMenu++, MF_STRING | MF_BYPOSITION, idCmdFirst + CONTEXT_PASTE, getContextText(CONTEXT_PASTE));

    return(MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, CONTEXT_QTY));
}

HRESULT PboFolderContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    ProfilingScope pScope;
    // https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-invokecommand
    //#TODO Check the cbSize member of pici to determine which structure (CMINVOKECOMMANDINFO or CMINVOKECOMMANDINFOEX) was passed in. 


    DebugLogger::TraceLog(std::format("verb {}, dir {}, parms {}", !IS_INTRESOURCE(pici->lpVerb) ? pici->lpVerb : "0", pici->lpDirectory ? pici->lpDirectory : "", pici->lpParameters ? pici->lpParameters : ""), std::source_location::current(), __FUNCTION__);

    int cmd = -1;
    //return m_DefCtxMenu->InvokeCommand(pici);


    if (HIWORD(pici->lpVerb)) //IS_INTRESOURCE()
    {
        if (!lstrcmpiA(pici->lpVerb, "paste"))
            cmd = CONTEXT_PASTE;
        //#TODO properties
        else
            Util::TryDebugBreak(); // rename, refresh, paste, explore, find? properties.
    }
    else cmd = LOWORD(pici->lpVerb);

    if (cmd < 0 || cmd >= CONTEXT_QTY) {
        DebugLogger::TraceLog(std::format("NOT IMPLEMENTED COMMAND"), std::source_location::current(), __FUNCTION__);
        //#TODO warn log
        return E_INVALIDARG;
    }

    HWND hwnd = pici->hwnd;
    if (!hwnd) hwnd = m_hwnd;

    if (cmd != CONTEXT_PASTE)
        return E_INVALIDARG;

    {
        HRESULT hres = E_FAIL;
        //#TODO support move paste

        //#TODO https://www.geoffchappell.com/studies/windows/shell/shlwapi/api/util/simulatedrop.htm ? Thats what zipfldr uses

        {
            ComRef<IDataObject> pdto;
            hres = OleGetClipboard(&pdto);
            if (SUCCEEDED(hres))
            {
                ComRef<IDropTarget> pdt;
                hres = m_folder->CreateViewObject(pici->hwnd, IID_IDropTarget, pdt.AsQueryInterfaceTarget());
                if (SUCCEEDED(hres))
                {
                    {
                        POINTL pt = { 0, 0 };
                        DWORD dwEffect = DROPEFFECT_COPY;   // Default
                        //HRESULT hr = DataObj_GetDWORD(pdo, g_dropTypes[DROP_PrefDe].cfFormat, &dwEffect);

                        hres = pdt->DragEnter(pdto, MK_LBUTTON, pt, &dwEffect);
                        if (SUCCEEDED(hres) && dwEffect)
                        {
                            hres = pdt->Drop(pdto, MK_LBUTTON, pt, &dwEffect);
                        }
                        else
                            pdt->DragLeave();
                    }
                }
            }
        }

        return hres;
    }



    return S_OK;
}

HRESULT PboFolderContextMenu::GetCommandString(
    UINT_PTR idCmd, UINT uFlags, UINT*, LPSTR pszName, UINT cchMax)
{
    if (uFlags != GCS_VERBW) return E_INVALIDARG;

    switch (idCmd)
    {
    case CONTEXT_PASTE:
        wcsncpy_s((LPWSTR)pszName, cchMax, L"paste", cchMax);
        return S_OK;
    }

    //return m_DefCtxMenu->GetCommandString(idCmd, uFlags, nullptr, pszName, cchMax);

    return E_INVALIDARG;
}