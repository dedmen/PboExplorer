
#include "PboContextMenu.hpp"

#include "ClassFactory.hpp"
#include "PboFolder.hpp"
#include "PboPidl.hpp"
#include "Util.hpp"

#ifndef SEE_MASK_NOASYNC
#define SEE_MASK_NOASYNC 0x00000100
#endif
#ifndef CMF_EXTENDEDVERBS
#define CMF_EXTENDEDVERBS 0x00000100
#endif
#ifndef CMIC_MASK_SHIFT_DOWN
#define CMIC_MASK_SHIFT_DOWN 0x10000000
#endif


static bool copyIStreamToFile(IStream* is, std::filesystem::path& fileName, INT64 size = 0, FILETIME* ft = nullptr)
{
    wchar_t drive[3] = { fileName.c_str()[0],':',0 };
    ULARGE_INTEGER freeSpace;
    if (!GetDiskFreeSpaceEx(drive, &freeSpace, nullptr, nullptr) ||
        (INT64)freeSpace.QuadPart < size)
        return(false);

    HANDLE file = CreateFile(fileName.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        // auto fallback, obfuscated pbo file can contain illegal characters
        auto newName = fileName = fileName.parent_path() / (Util::random_string(16) + fileName.extension().string());

        file = CreateFile(newName.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file != INVALID_HANDLE_VALUE)
            fileName = newName;
    	
    }
    if (file == INVALID_HANDLE_VALUE) return false;
    if (size > 0)
    {
        LARGE_INTEGER li;

        li.QuadPart = size;
        SetFilePointerEx(file, li, nullptr, FILE_BEGIN);
        SetEndOfFile(file);

        li.QuadPart = 0;
        SetFilePointerEx(file, li, nullptr, FILE_BEGIN);
    }

    unsigned char buf[4096];
    ULONG didRead;
    DWORD didWrite;
    INT64 writtenSize = 0;
    while (SUCCEEDED(is->Read(buf, (ULONG)sizeof(buf), &didRead)) && didRead)
    {
        if (!WriteFile(file, buf, didRead, &didWrite, nullptr))
            break;

        writtenSize += didWrite;

        if (writtenSize < didRead)
            break;
    }

    if (ft)
        SetFileTime(file, nullptr, nullptr, ft);

    CloseHandle(file);

    if (writtenSize < size) return(false);

    return(true);
}

//static wchar_t* copyIStreamToDir(IStream* is, const wchar_t* dirName)
//{
//    STATSTG ss;
//    if (FAILED(is->Stat(&ss, 0))) return(NULL);
//
//    wchar_t* name = (wchar_t*)malloc(
//        (wcslen(dirName) + wcslen(ss.pwcsName) + 2) * 2);
//    wcscpy(name, dirName);
//    wcscat(name, L"\\");
//    wcscat(name, ss.pwcsName);
//
//    CoTaskMemFree(ss.pwcsName);
//
//    if (copyIStreamToFile(is, name, ss.cbSize.QuadPart, &ss.mtime))
//        return(name);
//
//    free(name);
//
//    return(NULL);
//}


PboContextMenu::PboContextMenu(PboFolder* folder, HWND hwnd, LPCITEMIDLIST pidlRoot, UINT cidl, LPCITEMIDLIST* apidl)
{
    
    m_folder = folder;
    m_hwnd = hwnd;
    m_pidlRoot = ILClone(pidlRoot);
    auto test = (PboPidl*)apidl[0];
    m_apidl.reserve(cidl);
    for (UINT i = 0; i < cidl; i++)
        m_apidl.emplace_back(ILClone(apidl[i]));
}
PboContextMenu::~PboContextMenu()
{
}


// IUnknown
HRESULT PboContextMenu::QueryInterface(REFIID riid, void** ppvObject)
{
    if (IsEqualIID(riid, IID_IContextMenu))
        *ppvObject = (IContextMenu*)this;
    else if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = (IUnknown*)this;
    else
    {
        *ppvObject = nullptr;
        return(E_NOINTERFACE);
    }

    AddRef();
    return(S_OK);
}


enum
{
    CONTEXT_OPEN,
    CONTEXT_OPEN_WITH,
    CONTEXT_COPY,
    CONTEXT_QTY,
};

#define CONTEXT_TEXT_SIZE 32
static wchar_t contextText[CONTEXT_QTY][CONTEXT_TEXT_SIZE] = { L"",L"Open With",L"" };

static const wchar_t* getContextText(int idx)
{
    do
    {
        if (contextText[0][0]) break;

        HMODULE mod = GetModuleHandle(L"shell32.dll");

        LoadString(mod, 8496, contextText[CONTEXT_OPEN], CONTEXT_TEXT_SIZE);

        //LoadString(mod, 5377, contextText[CONTEXT_OPEN_WITH], CONTEXT_TEXT_SIZE);

        HMENU menu = LoadMenu(mod, MAKEINTRESOURCE(210));
        if (!menu) break;
        MENUITEMINFO mii;
        mii.cbSize = (UINT)sizeof(mii);
        mii.fMask = MIIM_STRING;
        mii.cch = CONTEXT_TEXT_SIZE;
        mii.dwTypeData = contextText[CONTEXT_COPY];
        GetMenuItemInfo(menu, 25, FALSE, &mii);
        DestroyMenu(menu);
    } while (0);

    return(contextText[idx]);
}

// IContextMenu
HRESULT PboContextMenu::QueryContextMenu(
    HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT, UINT uFlags)
{

    //#TODO query filetype contextmenu and use them?
    // add notepad++ manually?







    if (m_apidl.size() == 1)
    {
        InsertMenu(hmenu, indexMenu++, MF_STRING | MF_BYPOSITION,
            idCmdFirst + CONTEXT_OPEN, getContextText(CONTEXT_OPEN));
        if (!(uFlags & CMF_NODEFAULT))
            SetMenuDefaultItem(hmenu, idCmdFirst, FALSE);

        if ((uFlags & CMF_DEFAULTONLY))
            return(MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 1));
        // (uFlags & CMF_EXTENDEDVERBS) && 
        if (((PboPidl*)m_apidl[0].GetRef())->IsFile())
            InsertMenu(hmenu, indexMenu++, MF_STRING | MF_BYPOSITION,
                idCmdFirst + CONTEXT_OPEN_WITH, getContextText(CONTEXT_OPEN_WITH));

        InsertMenu(hmenu, indexMenu++, MF_SEPARATOR | MF_BYPOSITION, 0, nullptr);
    }
    else if ((uFlags & CMF_DEFAULTONLY))
        return(MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0));

    InsertMenu(hmenu, indexMenu++, MF_STRING | MF_BYPOSITION,
        idCmdFirst + CONTEXT_COPY, getContextText(CONTEXT_COPY));

    return(MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, CONTEXT_QTY));
}


//static wchar_t* copyIStreamToDir(IStream* is, const wchar_t* dirName)
//{
//    STATSTG ss;
//    if (FAILED(is->Stat(&ss, 0))) return(NULL);
//
//    wchar_t* name = (wchar_t*)malloc(
//        (wcslen(dirName) + wcslen(ss.pwcsName) + 2) * 2);
//    wcscpy(name, dirName);
//    wcscat(name, L"\\");
//    wcscat(name, ss.pwcsName);
//
//    CoTaskMemFree(ss.pwcsName);
//
//    if (copyIStreamToFile(is, name, ss.cbSize.QuadPart, &ss.mtime))
//        return(name);
//
//    free(name);
//
//    return(NULL);
//}

HRESULT PboContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    int cmd = -1;
    if (HIWORD(pici->lpVerb))
    {
        if (!lstrcmpiA(pici->lpVerb, "open"))
            cmd = CONTEXT_OPEN;
        else if (!lstrcmpiA(pici->lpVerb, "openas"))
            cmd = CONTEXT_OPEN_WITH;
        else if (!lstrcmpiA(pici->lpVerb, "copy"))
            cmd = CONTEXT_COPY;
    }
    else cmd = LOWORD(pici->lpVerb);

    if (cmd < 0 || cmd >= CONTEXT_QTY) return(E_INVALIDARG);

    HWND hwnd = pici->hwnd;
    if (!hwnd) hwnd = m_hwnd;

    if (cmd == CONTEXT_COPY)
    {
        ComRef<IDataObject> dataObject;
        HRESULT hr = m_folder->GetUIObjectOf(
            hwnd, m_apidl.size(), const_cast<LPCITEMIDLIST*>(&m_apidl[0]),
            IID_IDataObject, nullptr, dataObject.AsQueryInterfaceTarget<>());
        if (FAILED(hr)) return(hr);

        hr = OleSetClipboard(dataObject);
        if (SUCCEEDED(hr))
            hr = OleFlushClipboard();

        return(hr);
    }

    PboPidl* qp = (PboPidl*)m_apidl[0].GetRef();
    if (qp->IsFile())
    {
        HRESULT hr;
        //const wchar_t* tmpDirFull = m_folder->getTempDir();
        std::filesystem::path fileTarget;

        if ((pici->fMask & CMIC_MASK_SHIFT_DOWN))
        {
            ComRef<IEnumIDList> srcEnum;
            hr = m_folder->EnumObjects(hwnd, SHCONTF_NONFOLDERS, srcEnum.AsQueryInterfaceTarget<IEnumIDList>());
            if (FAILED(hr)) return(hr);

            std::vector<CoTaskMemRefS<ITEMIDLIST>> subPidls;
            LPITEMIDLIST pn;
            while (srcEnum->Next(1, &pn, nullptr) == S_OK)
            {
                if (((PboPidl*)pn)->IsFolder())
                {
                    CoTaskMemFree(pn);
                    continue;
                }

                subPidls.emplace_back(pn);
            }

            ComRef<IDataObject> data;
            hr = m_folder->GetUIObjectOf(hwnd, subPidls.size(), reinterpret_cast<LPCITEMIDLIST*>(subPidls.data()), IID_IDataObject, nullptr, data.AsQueryInterfaceTarget());

            if (FAILED(hr)) return(hr);

            //FORMATETC formatetc = {
            //  QiewerDataObject::s_fileContents,NULL,DVASPECT_CONTENT,
            //  0,TYMED_ISTREAM
            //};
            //STGMEDIUM medium;
            //for (int i = 0; i < subPidlQty; i++)
            //{
            //    formatetc.lindex = i;
            //    if (FAILED(data->GetData(&formatetc, &medium))) continue;
            //
            //    free(copyIStreamToDir(medium.pstm, tmpDirFull));
            //
            //    ReleaseStgMedium(&medium);
            //}

            STRRET displayName;
            hr = m_folder->GetDisplayNameOf(m_apidl[0], SHGDN_INFOLDER, &displayName);
            if (FAILED(hr)) return(hr);

            //name = (wchar_t*)malloc(
            //    (wcslen(tmpDirFull) + wcslen(displayName.pOleStr) + 2) * 2);
            //wcscpy(name, tmpDirFull);
            //wcscat(name, L"\\");
            //wcscat(name, displayName.pOleStr);

            CoTaskMemFree(displayName.pOleStr);
        }
        else
        {
            ComRef<IStream> is;
            hr = m_folder->BindToObject(
                m_apidl[0], nullptr, IID_IStream, is.AsQueryInterfaceTarget());
            if (FAILED(hr)) return(hr);

            auto tempDir = m_folder->GetTempDir();
            fileTarget = tempDir / qp->filePath;
        	
            copyIStreamToFile(is, fileTarget);
        }

        if (fileTarget.empty())
            hr = E_UNEXPECTED;
        else
        {
            SHELLEXECUTEINFO sei;
            ZeroMemory(&sei, sizeof(sei));
            sei.cbSize = (DWORD)sizeof(sei);
            sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
            if (cmd == CONTEXT_OPEN_WITH)
                sei.lpVerb = L"openas";
            sei.lpFile = fileTarget.c_str();
            sei.hwnd = hwnd;
            sei.nShow = SW_SHOWNORMAL;

            hr = ShellExecuteEx(&sei) ? S_OK : E_UNEXPECTED;
            
        }

        return(hr);
    }

    LPITEMIDLIST pidlFull = ILCombine(m_pidlRoot, m_apidl[0]);
    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = (DWORD)sizeof(sei);
    sei.fMask = SEE_MASK_IDLIST | SEE_MASK_CLASSNAME;
    sei.lpIDList = pidlFull;
    sei.lpClass = L"folder";
    sei.hwnd = hwnd;
    sei.nShow = SW_SHOWNORMAL;
	
	
    //wchar_t path[MAX_PATH];
    //SHGetPathFromIDList(pidlFull, path);
	
    OutputDebugStringW(L"PboContextMenu::InvokeCommand");
    OutputDebugStringA("\n");


    bool ret = ShellExecuteEx(&sei);
    CoTaskMemFree(pidlFull);
    if (!ret) return(E_UNEXPECTED);

    return(S_OK);
}
HRESULT PboContextMenu::GetCommandString(
    UINT_PTR idCmd, UINT uFlags, UINT*, LPSTR pszName, UINT cchMax)
{
    if (uFlags != GCS_VERBW) return(E_INVALIDARG);

    switch (idCmd)
    {
    case CONTEXT_OPEN:
        wcsncpy_s((LPWSTR)pszName, cchMax, L"open", cchMax);
        return(S_OK);

    case CONTEXT_OPEN_WITH:
        wcsncpy_s((LPWSTR)pszName, cchMax, L"openas", cchMax);
        return(S_OK);

    case CONTEXT_COPY:
        wcsncpy_s((LPWSTR)pszName, cchMax, L"copy", cchMax);
        return(S_OK);
    }

    return(E_INVALIDARG);
}