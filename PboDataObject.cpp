#define _CRT_SECURE_NO_WARNINGS
#include "PboDataObject.hpp"

#define NOMINMAX
#include <utility>

#include "PboFileStream.hpp"
#include "PboPidl.hpp"

#ifndef FD_PROGRESSUI
#define FD_PROGRESSUI 0x00004000
#endif
#include "DebugLogger.hpp"

#ifndef CFSTR_OLECLIPBOARDPERSISTONFLUSH
#define CFSTR_OLECLIPBOARDPERSISTONFLUSH TEXT("OleClipboardPersistOnFlush")
#endif
#include "Util.hpp"

#ifndef __MINGW64_VERSION_MAJOR
const GUID IID_IAsyncOperation =
{ 0x3D8B0590,0xF691,0x11D2,{0x8E,0xA9,0x00,0x60,0x97,0xDF,0x5B,0xD4} };
#endif


HRESULT QLoadFromStream(IStream* pStm, LPITEMIDLIST* pidl)
{
    if (!pStm || !pidl) return(E_INVALIDARG);

    HRESULT hr;
    DWORD dwRead;
    UINT size;
    if (FAILED(hr = pStm->Read(&size, (ULONG)sizeof(UINT), &dwRead)))
        return(hr);

    LPITEMIDLIST pidlNew = (LPITEMIDLIST)CoTaskMemAlloc(size);
    if (!pidlNew) return(E_OUTOFMEMORY);
    if (FAILED(hr = pStm->Read(pidlNew, size, &dwRead)))
    {
        CoTaskMemFree(pidlNew);
        return(hr);
    }
    *pidl = pidlNew;

    return(S_OK);
}
HRESULT QSaveToStream(IStream* pStm, LPCITEMIDLIST pidl)
{
    if (!pStm || !pidl) return(E_INVALIDARG);

    HRESULT hr;
    UINT size = ILGetSize(pidl);
    if (FAILED(hr = pStm->Write(&size, (ULONG)sizeof(UINT), NULL)))
        return(hr);

    if (FAILED(hr = pStm->Write(pidl, size, NULL)))
        return(hr);

    return(S_OK);
}


PboDataObject::PboDataObject(void)
{
    //m_dir = NULL;
    m_asyncMode = true; // OS_MIN_VISTA;
    m_inAsyncOperation = FALSE;
    m_pidlRoot = nullptr;
    m_cidl = 0;
    //m_apidl = NULL;
    m_listsInit = false;
    m_postQuit = false;
    m_offset = nullptr;
}
PboDataObject::PboDataObject(std::shared_ptr<PboSubFolder> rootfolder, PboFolder* mainFolder, LPCITEMIDLIST pidl,
    UINT cidl, LPCITEMIDLIST* apidl)
{
    //m_dir = dir;
    m_asyncMode = true; // OS_MIN_VISTA;
    m_inAsyncOperation = FALSE;
    m_listsInit = false;
    m_postQuit = false;
    m_offset = nullptr;

    rootFolder = std::move(rootfolder);
    pboFolder = mainFolder;
	
    m_pidlRoot = ILClone(pidl);
    m_cidl = cidl;
    m_apidl.reserve(m_cidl);
    for (UINT i = 0; i < m_cidl; i++)
        m_apidl.emplace_back(ILClone(apidl[i]));

    //dirMutex.lock();
    //m_dir->countUp();
    //dirMutex.unlock();
}
PboDataObject::PboDataObject(std::shared_ptr<PboSubFolder> rootfolder, PboFolder* mainFolder, LPCITEMIDLIST pidl,
    const wchar_t* offset)
{
    __debugbreak();
    m_asyncMode = TRUE;
    m_inAsyncOperation = FALSE;
    m_listsInit = false;
    m_postQuit = true;

    rootFolder = std::move(rootfolder);
    pboFolder = mainFolder;
	
    m_offset = nullptr;
    if (offset && offset[0])
    {
        size_t len = wcslen(offset);
        m_offset = (wchar_t*)malloc((len + 2) * 2);
        wcscpy(m_offset, offset);
        if (m_offset[len - 1] != '\\')
            wcscpy(m_offset + len, L"\\");
    }

    m_pidlRoot = ILClone(pidl);



    m_cidl = rootFolder->subfiles.size() + rootFolder->subfolders.size();


	
    //int counts[3];
    //counts[0] = dir->getDirQty();
    //counts[1] = dir->getArchiveQty();
    //counts[2] = dir->getStreamQty();
    //m_cidl = counts[0] + counts[1] + counts[2];
    //m_apidl = (LPITEMIDLIST*)malloc(m_cidl * sizeof(LPITEMIDLIST));
    m_apidl.reserve(m_cidl);

    for (auto& it : rootFolder->subfolders)
    {
        PboPidl* qp = (PboPidl*)CoTaskMemAlloc(sizeof(PboPidl) + sizeof(USHORT));
        memset(qp, 0, sizeof(PboPidl) + sizeof(USHORT));
        qp->cb = (USHORT)sizeof(PboPidl);
        qp->type = PboPidlFileType::Folder;
        //qp->idx = idx;
        qp->filePath = it->fullPath;

    	
        m_apidl.emplace_back((LPITEMIDLIST)qp);
    }

    for (auto& it : rootFolder->subfiles)
    {
        PboPidl* qp = (PboPidl*)CoTaskMemAlloc(sizeof(PboPidl) + sizeof(USHORT));
        memset(qp, 0, sizeof(PboPidl) + sizeof(USHORT));
        qp->cb = (USHORT)sizeof(PboPidl);
        qp->type = PboPidlFileType::File;
        //qp->idx = idx;
        qp->filePath = it.fullPath;

        m_apidl.emplace_back((LPITEMIDLIST)qp);
    }
}
PboDataObject::~PboDataObject()
{
    //dirMutex.lock();
    //if (m_dir)
    //    m_dir->countDown();
    //for (int i = 0; i < m_dirs.qty(); i++)
    //    m_dirs[i]->countDown();
    //dirMutex.unlock();
    //
    //for (int i = 0; i < m_dirnames.qty(); i++)
    //    free(m_dirnames[i]);
    //for (int i = 0; i < m_filenames.qty(); i++)
    //    free(m_filenames[i]);

    //CoTaskMemFree(m_pidlRoot);
    //for (UINT i = 0; i < m_cidl; i++)
    //    CoTaskMemFree(m_apidl[i]);
    //free(m_apidl);
    m_apidl.clear();
    free(m_offset);
}
/*
void PboDataObject::initLists(void)
{
    if (m_listsInit) return;

    //dirMutex.lock();

    if (m_listsInit)
    {
        //dirMutex.unlock();
        return;
    }

    if (m_offset)
        m_dirnames.add(wcsdup(L""));

    const wchar_t* rootName = m_dir->getName();
    for (UINT i = 0; i < m_cidl; i++)
    {
        const PboPidl* qp = (const PboPidl*)m_apidl[i];
        int idx = qp->idx;
        switch (qp->type)
        {
        default:
        {
            const wchar_t* name = m_dir->getDirName(idx);
            if (!name) break;

            wchar_t* fullName = (wchar_t*)malloc(
                (wcslen(rootName) + wcslen(name) + 2) * sizeof(wchar_t));
            wcscpy(fullName, rootName);
            wcscat(fullName, L"\\");
            wcscat(fullName, name);

            Dir* subdir = Dir::getAbsDir(fullName);
            free(fullName);
            if (!subdir) break;

            wchar_t* nameCopy = wcsdup(m_dir->getDirName(idx));
            m_dirnames.add(nameCopy);

            addFilesToLists(subdir, nameCopy);

            m_dirs.add(subdir);
        }
        break;
        case 1:
        case 2:
        {
            const wchar_t* name = qp->type == 1 ?
                m_dir->getArchiveName(idx) : m_dir->getStreamName(idx);
            if (!name) break;

            wchar_t* nameCopy = wcsdup(name);
            m_filenames.add(nameCopy);
        }
        break;
        }
    }
    m_dir->sortFiles(m_filenames);

    m_listsInit = true;

    dirMutex.unlock();
}
void PboDataObject::addFilesToLists(Dir* dir, const wchar_t* offset)
{
    Dir* childDir;
    wchar_t* fullPath;

    const wchar_t* rootName = dir->getName();
    for (int j = 0; j < 3; j++)
    {
        int qty = j == 0 ? dir->getDirQty() :
            (j == 1 ? dir->getArchiveQty() :
                dir->getStreamQty());
        const wchar_t* c;
        wchar_t* fullName;
        for (int i = 0; i < qty; i++)
        {
            switch (j)
            {
            case 0:
                c = dir->getDirName(i);

                fullPath = (wchar_t*)malloc(
                    (wcslen(offset) + wcslen(c) + 2) * sizeof(wchar_t));
                wcscpy(fullPath, offset);
                if (fullPath[wcslen(fullPath) - 1] != '\\')
                    wcscat(fullPath, QC("\\"));
                wcscat(fullPath, c);

                m_dirnames.add(fullPath);

                fullName = (wchar_t*)malloc(
                    (wcslen(rootName) + wcslen(c) + 2) * sizeof(wchar_t));
                wcscpy(fullName, rootName);
                wcscat(fullName, L"\\");
                wcscat(fullName, c);

                childDir = Dir::getAbsDir(fullName);
                free(fullName);
                if (!childDir) break;

                addFilesToLists(childDir, fullPath);

                m_dirs.add(childDir);
                break;

            case 1:
            case 2:
                c = j == 1 ? dir->getArchiveName(i) :
                    dir->getStreamName(i);

                fullPath = (wchar_t*)malloc(
                    (wcslen(offset) + wcslen(c) + 2) * sizeof(wchar_t));
                wcscpy(fullPath, offset);
                if (fullPath[wcslen(fullPath) - 1] != '\\')
                    wcscat(fullPath, QC("\\"));
                wcscat(fullPath, c);

                m_filenames.add(fullPath);
                break;
            }
        }
    }
}

*/
// IUnknown
HRESULT PboDataObject::QueryInterface(REFIID riid, void** ppvObject)
{
    DebugLogger_OnQueryInterfaceEntry(riid);
#if _DEBUG
    static const GUID IID_IApplicationFrame = //{143715D9-A015-40EA-B695-D5CC267E36EE}
    { 0x143715D9, 0xA015, 0x40EA, 0xB6, 0x95, 0xD5, 0xCC, 0x26, 0x7E, 0x36, 0xEE };

    static const GUID IID_IApplicationFrameManager = //{D6DEFAB3-DBB9-4413-8AF9-554586FDFF94}
    { 0xD6DEFAB3, 0xDBB9, 0x4413, 0x8A, 0xF9, 0x55, 0x45, 0x86, 0xFD, 0xFF, 0x94 };

    static const GUID IID_IApplicationFrameEventHandler = //{EA5D0DE4-770D-4DA0-A9F8-D7F9A140FF79}
    { 0xEA5D0DE4, 0x770D, 0x4DA0, 0xA9, 0xF8, 0xD7, 0xF9, 0xA1, 0x40, 0xFF, 0x79 };

    static const GUID IID_IPimcContext2 = //{1868091E-AB5A-415F-A02F-5C4DD0CF901D}
    { 0x1868091E, 0xAB5A, 0x415F, 0xA0, 0x2F, 0x5C, 0x4D, 0xD0, 0xCF, 0x90, 0x1D };

    static const GUID IID_IEUserBroker = //{1AC7516E-E6BB-4A69-B63F-E841904DC5A6}
    { 0x1AC7516E, 0xE6BB, 0x4A69, 0xB6, 0x3F, 0xE8, 0x41, 0x90, 0x4D, 0xC5, 0xA6 };

    static const GUID IID_IFrameTaskManager =
    { 0x35bd3360, 0x1b35, 0x4927, 0xBa, 0xe4, 0xb1, 0x0e, 0x70, 0xd9, 0x9e, 0xff };

    static const GUID IID_IVerbStateTaskCallBack =
    { 0xf2153260, 0x232e, 0x4474, 0x9d, 0x0a, 0x9f, 0x2a, 0xb1, 0x53, 0x44, 0x1d };

#endif


    if (COMJoiner::QueryInterfaceJoiner(riid, ppvObject)) {
        AddRef();
        return S_OK;
    }

    //if (IsEqualIID(riid, IID_IPersistStream))
    //    *ppvObject = static_cast<IPersistStream*>(this);
    //else if (IsEqualIID(riid, IID_IAsyncOperation))
    //    *ppvObject = (IAsyncOperation*)this;

    //else if (IsEqualIID(riid, (GUID{ 0x6C72B11B, 0xDBE0, 0x4C87, 0xB1, 0xA8, 0x7C, 0x8A, 0x36, 0xBD, 0x56, 0x3D }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0xECC8691B, 0xC1DB, 0x4DC0, 0x85, 0x5E, 0x65, 0xF6, 0xC5, 0x51, 0xAF, 0x49 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x94EA2B94, 0xE9CC, 0x49E0, 0xC0, 0xFF, 0xEE, 0x64, 0xCA, 0x8F, 0x5B, 0x90 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x2132B005, 0xC604, 0x4354, 0x85, 0xBD, 0x8F, 0x2E, 0x24, 0x18, 0x1B, 0x0C }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x00000003, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x0000001B, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x00000018, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x334D391F, 0x0E79, 0x3B15, 0xC9, 0xFF, 0xEA, 0xC6, 0x5D, 0xD0, 0x7C, 0x42 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x6F3094CB, 0xFFC0, 0x4CF8, 0xB5, 0x2E, 0x7E, 0x9A, 0x81, 0x0B, 0x6C, 0xD5 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //
    //else if (IsEqualIID(riid, (GUID{ 0x77DD1250, 0x139C, 0x2BC3, 0xBD, 0x95, 0x90, 0x0A, 0xCE, 0xD6, 0x1B, 0xE5 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0xBFD60505, 0x5A1F, 0x4E41, 0x88, 0xBA, 0xA6, 0xFB, 0x07, 0x20, 0x2D, 0xA9 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x816E5B3E, 0x5523, 0x4EFC, 0x92, 0x23, 0x98, 0xEC, 0x42, 0x14, 0xC3, 0xA0 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x3C169FF7, 0x37B2, 0x484C, 0xB1, 0x99, 0xC3, 0x15, 0x55, 0x90, 0xF3, 0x16 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x4F4F92B5, 0x6DED, 0x4E9B, 0xA9, 0x3F, 0x01, 0x38, 0x91, 0xB3, 0xA8, 0xB7 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x9BC79C93, 0x2289, 0x4BB5, 0xAB, 0xF4, 0x32, 0x87, 0xFD, 0x9C, 0xAE, 0x39 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x11456F96, 0x09D1, 0x4909, 0x8F, 0x36, 0x4E, 0xB7, 0x4E, 0x42, 0xB9, 0x3E }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x03FB5C57, 0xD534, 0x45F5, 0xA1, 0xF4, 0xD3, 0x95, 0x56, 0x98, 0x38, 0x75 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x2C258AE7, 0x50DC, 0x49FF, 0x9D, 0x1D, 0x2E, 0xCB, 0x9A, 0x52, 0xCD, 0xD7 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x00000019, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x4C1E39E1, 0xE3E3, 0x4296, 0xAA, 0x86, 0xEC, 0x93, 0x8D, 0x89, 0x6E, 0x92 }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }
    //else if (IsEqualIID(riid, (GUID{ 0x1C733A30, 0x2A1C, 0x11CE, 0xAD, 0xE5, 0x00, 0xAA, 0x00, 0x44, 0x77, 0x3D }))) { DebugLogger_OnQueryInterfaceExitUnhandled(riid); *ppvObject = nullptr; return(E_NOINTERFACE); }


    else if (DebugLogger::IsIIDUninteresting(riid)) {
        *ppvObject = nullptr;
        return(E_NOINTERFACE);
    }

    else
    {
        DebugLogger_OnQueryInterfaceExitUnhandled(riid);

        *ppvObject = nullptr;
        //__debugbreak();
        return(E_NOINTERFACE);
    }


    AddRef();
    return(S_OK);
}


// https://www.codeproject.com/Reference/1091137/Windows-Clipboard-Formats

// IDataObject
HRESULT PboDataObject::GetData(
    FORMATETC* pformatetc, STGMEDIUM* pmedium)
{
    wchar_t buffer[128];
    GetClipboardFormatNameW(pformatetc->cfFormat, buffer, 127);


    DebugLogger::TraceLog(std::format("formatName {}, format {}", Util::utf8_encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);


    if (pformatetc->cfFormat == s_preferredDropEffect)
    {
        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE, sizeof(DWORD));
        if (!hMem) return(E_OUTOFMEMORY);

        LPDWORD pdw = (LPDWORD)GlobalLock(hMem);
        *pdw = DROPEFFECT_COPY;
        GlobalUnlock(hMem);

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->pUnkForRelease = nullptr;
        pmedium->hGlobal = hMem;

        return(S_OK);
    }

    if (pformatetc->cfFormat == s_fileDescriptor)
    {
        if (!(pformatetc->tymed & TYMED_HGLOBAL)) return(DV_E_TYMED);

        size_t offsetLen = 0;
        if (m_offset)
        {
            offsetLen = wcslen(m_offset);
            if (offsetLen >= MAX_PATH - 1) return(E_UNEXPECTED);
        }

        //initLists();

        // all the files associated with this data object






        //#TODO if m_cidl == 1, we only have a single file (for example drag&drop), we should only return that one file
    	
        size_t memSize = sizeof(FILEGROUPDESCRIPTOR) + m_apidl.size() * sizeof(FILEDESCRIPTOR);

        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, memSize);
        if (!hMem) return(E_OUTOFMEMORY);

        FILEGROUPDESCRIPTOR* fgd = (FILEGROUPDESCRIPTOR*)GlobalLock(hMem);

        fgd->cItems = m_apidl.size();



        int i = 0;
        for (auto& it : m_apidl) {
            auto file = (PboPidl*)it.GetRef();

            FILEDESCRIPTOR* fd = &fgd->fgd[i];

            if (m_offset)
                wcscpy(fd->cFileName, m_offset);
            wcsncpy(fd->cFileName + offsetLen, file->filePath.filename().c_str(), MAX_PATH - offsetLen);
            fd->cFileName[MAX_PATH - 1] = 0;

            //#TODO add subitems inside the folder then?
            if (file->IsFolder()) {
                fd->dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                fd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            } else {

                fd->dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                fd->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;


                auto subFile = rootFolder->GetFileByPath(file->filePath);

                if (subFile) {
                    fd->dwFlags |= FD_FILESIZE;
                    uint64_t size = subFile->get().dataSize;
                    fd->nFileSizeLow = (DWORD)size;
                    fd->nFileSizeHigh = (DWORD)(size >> 32);
                }

                
            }

            ++i;
        }


        GlobalUnlock(hMem);

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->pUnkForRelease = nullptr;
        pmedium->hGlobal = hMem;

        return(S_OK);
    }

    if (pformatetc->cfFormat == s_fileContents)
    {
        if (!(pformatetc->tymed & TYMED_ISTREAM)) return(DV_E_TYMED);

        //initLists();


        LONG index = pformatetc->lindex;

        if (index < 0 || index >= m_apidl.size())
            return(DV_E_LINDEX);

        auto file = (PboPidl*)m_apidl[pformatetc->lindex].GetRef();


        //const wchar_t* rootName = rootFolder->filename.c_str();
        //const wchar_t* name = rootFolder->subfiles[index - dirCount].filename.c_str();
        //wchar_t* fullName =
        //    (wchar_t*)malloc((wcslen(rootName) + wcslen(name) + 2) * sizeof(wchar_t));
        //wcscpy(fullName, rootName);
        //wcscat_s(fullName,500, L"\\");
        //wcscat_s(fullName, 500, name);

        //dirMutex.lock();
        //Stream* stream = Dir::getAbsStream(fullName, NULL);
        //dirMutex.unlock();
        
        //if (!stream) return(E_FAIL);

        IStream* qs = new PboFileStream(pboFolder->pboFile, file->filePath);
        qs->AddRef();
        //free(fullName);
        pmedium->tymed = TYMED_ISTREAM;
        pmedium->pUnkForRelease = nullptr;
        pmedium->pstm = qs;

        return(S_OK);
    }

    if (pformatetc->cfFormat == s_shellIdList)
    {
        if (!(pformatetc->tymed & TYMED_HGLOBAL)) return(DV_E_TYMED);

        UINT offset = UINT(sizeof(CIDA) + sizeof(UINT) * m_cidl);
        UINT size = offset + ILGetSize(m_pidlRoot);
        for (UINT i = 0; i < m_cidl; i++)
            size += ILGetSize(m_apidl[i]);

        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, size);
        if (!hMem) return(E_OUTOFMEMORY);

        CIDA* cida = (CIDA*)GlobalLock(hMem);
        BYTE* p = (BYTE*)cida;
        p += offset;

        cida->cidl = m_cidl;

        cida->aoffset[0] = offset;
        size = ILGetSize(m_pidlRoot);
        CopyMemory(p, m_pidlRoot, size);
        p += size;
        offset += size;

        for (UINT i = 0; i < m_cidl; i++)
        {
            cida->aoffset[1 + i] = offset;
            size = ILGetSize(m_apidl[i]);
            CopyMemory(p, m_apidl[i], size);
            p += size;
            offset += size;
        }

        GlobalUnlock(hMem);

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->pUnkForRelease = nullptr;
        pmedium->hGlobal = hMem;

        return(S_OK);
    }

    if (pformatetc->cfFormat == s_oleClipboardPersistOnFlush)
    {
        if (!(pformatetc->tymed & TYMED_HGLOBAL)) return(DV_E_TYMED);

        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT,
            sizeof(DWORD));
        if (!hMem) return(E_OUTOFMEMORY);

        DWORD* persistOnFlush = (DWORD*)GlobalLock(hMem);
        *persistOnFlush = 0;
        GlobalUnlock(hMem);

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->pUnkForRelease = nullptr;
        pmedium->hGlobal = hMem;

        return(S_OK);
    }

    if (pformatetc->cfFormat == 0xc2b4 && pformatetc->tymed & TYMED_HGLOBAL) { // L"IsShowingText"

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, sizeof(DWORD));

        LPDWORD pdw = (LPDWORD)GlobalLock(pmedium->hGlobal);
        *pdw = 1;
        GlobalUnlock(pmedium->hGlobal);

        return S_OK;
    }

    if (pformatetc->cfFormat == 0xc116 && pformatetc->tymed & TYMED_HGLOBAL) { //  L"DropDescription"

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, sizeof(DROPDESCRIPTION));

        auto pdw = (DROPDESCRIPTION*)GlobalLock(pmedium->hGlobal);
        auto& desc = *pdw;

        lstrcpyW(desc.szMessage, L"Test1");
        lstrcpyW(desc.szInsert, L"Test2");
        desc.type = DROPIMAGE_COPY;

        GlobalUnlock(pmedium->hGlobal);

        return S_OK;
    }

    if (pformatetc->cfFormat == CF_UNICODETEXT && pformatetc->tymed & TYMED_HGLOBAL) {

        pmedium->tymed = TYMED_HGLOBAL;


        //#TODO create this method
        //return CreateHGlobalFromBlob(&& c_hdrop, sizeof(c_hdrop), GMEM_MOVEABLE, &pmed->hGlobal);

        pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, sizeof(wchar_t)*20);

        auto pdw = (wchar_t*)GlobalLock(pmedium->hGlobal);
        lstrcpyW(pdw, L"Test1");
        GlobalUnlock(pmedium->hGlobal);

        return S_OK;
    }

    //if (pformatetc->cfFormat == CF_HDROP ) { // && pformatetc->tymed & TYMED_HGLOBAL
    //
    //    pmedium->tymed = TYMED_HGLOBAL;
    //
    //    auto test = L"O:\\dev\\pbofolder\\test\\ace_advanced_ballistics.pbox";
    //    pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + sizeof(test) + sizeof(wchar_t)*2);
    //
    //
    //    https://docs.microsoft.com/en-us/windows/win32/shell/clipboard#cf_hdrop
    //
    //    auto pdw = (DROPFILES*)GlobalLock(pmedium->hGlobal);
    //    pdw->fWide = 1;
    //    pdw->pFiles = sizeof(DROPFILES);
    //    pdw++;
    //    wchar_t* data = (wchar_t*)pdw;
    //
    //    lstrcpyW(data, test);
    //    GlobalUnlock(pmedium->hGlobal);
    //
    //    return S_OK;
    //}

    
    /*
    
    GetData 0x000000000bcabad0 L"AsyncFlag"
unknown 0x000000000bcabad0 L"AsyncFlag" 0xc118
GetData 0x000000000bcabad0 L"FileGroupDescriptorW"
GetData 0x000000000bcabbe0 L"Shell Object Offsets"
unknown 0x000000000bcabbe0 L"Shell Object Offsets" 0xc0ff

    */

    //#TODO not a trace, this is a warn
    DebugLogger::TraceLog(std::format("formatName {}, format {}, UNHANDLED", Util::utf8_encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);


    return(DV_E_FORMATETC);
}
HRESULT PboDataObject::GetDataHere(FORMATETC*, STGMEDIUM*)
{
    return(E_NOTIMPL);
}
HRESULT PboDataObject::QueryGetData(FORMATETC* pformatetc)
{
    if (pformatetc->cfFormat == s_preferredDropEffect
        || pformatetc->cfFormat == s_fileDescriptor
        || pformatetc->cfFormat == s_fileContents
        || pformatetc->cfFormat == s_shellIdList
        || pformatetc->cfFormat == s_oleClipboardPersistOnFlush
        //|| pformatetc->cfFormat == CF_HDROP // https://msdn.microsoft.com/en-us/library/bb773269.aspx 
        || pformatetc->cfFormat == CF_UNICODETEXT
        )
        return S_OK;

    wchar_t buffer[128];
    GetClipboardFormatNameW(pformatetc->cfFormat, buffer, 127);




    return DV_E_FORMATETC;
}
HRESULT PboDataObject::GetCanonicalFormatEtc(FORMATETC*, FORMATETC*)
{
    return(E_NOTIMPL);
}
HRESULT PboDataObject::SetData(FORMATETC* tf , STGMEDIUM* med , BOOL b)
{
    return(E_NOTIMPL);
}









class QiewerEnumFormatetc : GlobalRefCounted, public RefCountedCOM<QiewerEnumFormatetc, IEnumFORMATETC>
{
public:
    QiewerEnumFormatetc(void);
    ~QiewerEnumFormatetc();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    // IEnumFORMATETC
    HRESULT STDMETHODCALLTYPE Next(
        ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched);
    HRESULT STDMETHODCALLTYPE Skip(DWORD celt);
    HRESULT STDMETHODCALLTYPE Reset(void);
    HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** ppenum);

private:

    
    int m_pos;
};

QiewerEnumFormatetc::QiewerEnumFormatetc(void)
{
    m_pos = 0;
}
QiewerEnumFormatetc::~QiewerEnumFormatetc()
{
}


// IUnknown
HRESULT QiewerEnumFormatetc::QueryInterface(REFIID riid, void** ppvObject)
{
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


// IEnumFORMATETC
HRESULT QiewerEnumFormatetc::Next(
    ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched)
{
    ULONG i = 0;

    for (; m_pos < 4 && i < celt; m_pos++, i++)
    {
        CLIPFORMAT cf;
        DWORD tymed;
        FORMATETC fe = { 0, nullptr,DVASPECT_CONTENT,-1,0 };
        switch (m_pos)
        {
        default:
            cf = PboDataObject::s_fileDescriptor;
            tymed = TYMED_HGLOBAL;
            break;

        case 1:
            cf = PboDataObject::s_fileContents;
            tymed = TYMED_ISTREAM;
            break;

        case 2:
            cf = PboDataObject::s_preferredDropEffect;
            tymed = TYMED_HGLOBAL;
            break;

        case 3:
            cf = PboDataObject::s_oleClipboardPersistOnFlush;
            tymed = TYMED_HGLOBAL;
            break;
        }
        fe.cfFormat = cf;
        fe.tymed = tymed;
        rgelt[i] = fe;
    }

    if (pceltFetched)
        *pceltFetched = i;

    return(celt == i ? S_OK : S_FALSE);
}
HRESULT QiewerEnumFormatetc::Skip(DWORD celt)
{
    m_pos += celt;
    return(S_OK);
}
HRESULT QiewerEnumFormatetc::Reset(void)
{
    m_pos = 0;
    return(S_OK);
}
HRESULT QiewerEnumFormatetc::Clone(IEnumFORMATETC** ppenum)
{
    *ppenum = nullptr;
    return(E_NOTIMPL);
}














HRESULT PboDataObject::EnumFormatEtc(
    DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc)
{
    if (dwDirection != DATADIR_GET) return(E_NOTIMPL);

    *ppenumFormatEtc = new QiewerEnumFormatetc();
    return(S_OK);
}
HRESULT PboDataObject::DAdvise(
    FORMATETC*, DWORD, IAdviseSink*, DWORD*)
{
    return(E_NOTIMPL);
}
HRESULT PboDataObject::DUnadvise(DWORD)
{
    return(E_NOTIMPL);
}
HRESULT PboDataObject::EnumDAdvise(IEnumSTATDATA**)
{
    return(OLE_E_ADVISENOTSUPPORTED);
}


// IPersist
HRESULT PboDataObject::GetClassID(CLSID* pClassID)
{
    *pClassID = CLSID_PboExplorer;

    return(S_OK);
}


// IPersistStream
//HRESULT PboDataObject::IsDirty(void)
//{
//    return(E_NOTIMPL);
//}
//HRESULT PboDataObject::Load(IStream* pStm)
//{
//    HRESULT hr;
//    if (FAILED(hr = QLoadFromStream(pStm, &m_pidlRoot)))
//        return(hr);
//
//    DWORD cidl;
//    DWORD dwRead;
//    if (FAILED(hr = pStm->Read(&cidl, (ULONG)sizeof(DWORD), &dwRead)))
//        return(hr);
//
//    m_apidl = (LPITEMIDLIST*)malloc(cidl * sizeof(LPITEMIDLIST));
//    for (m_cidl = 0; m_cidl < cidl; m_cidl++)
//    {
//        if (FAILED(hr = QLoadFromStream(pStm, &m_apidl[m_cidl])))
//            return(hr);
//    }
//
//    if (FAILED(hr = pStm->Read(&cidl, (ULONG)sizeof(DWORD), &dwRead)))
//        return(hr);
//    wchar_t* name = (wchar_t*)malloc(cidl * 2);
//    if (FAILED(hr = pStm->Read(name, cidl * 2, &dwRead)))
//    {
//        free(name);
//        return(hr);
//    }
//    dirMutex.lock();
//    m_dir = Dir::getAbsDir(name);
//    dirMutex.unlock();
//    free(name);
//
//    return(m_dir ? S_OK : E_FAIL);
//}
//HRESULT PboDataObject::Save(IStream* pStm, BOOL)
//{
//    HRESULT hr;
//    if (FAILED(hr = QSaveToStream(pStm, m_pidlRoot)))
//        return(hr);
//
//    DWORD cidl = m_cidl;
//    if (FAILED(hr = pStm->Write(&cidl, (ULONG)sizeof(DWORD), NULL)))
//        return(hr);
//
//    for (UINT i = 0; i < m_cidl; i++)
//    {
//        if (FAILED(hr = QSaveToStream(pStm, m_apidl[i])))
//            return(hr);
//    }
//
//    const wchar_t* name = m_dir->getName();
//    cidl = (DWORD)wcslen(name) + 1;
//    if (FAILED(hr = pStm->Write(&cidl, (ULONG)sizeof(DWORD), NULL)))
//        return(hr);
//    if (FAILED(hr = pStm->Write(name, cidl * 2, NULL)))
//        return(hr);
//
//    return(S_OK);
//}
//HRESULT PboDataObject::GetSizeMax(ULARGE_INTEGER*)
//{
//    return(E_NOTIMPL);
//}


// IAsyncOperation
HRESULT PboDataObject::SetAsyncMode(BOOL fDoOpAsync)
{
    m_asyncMode = fDoOpAsync;
    return(S_OK);
}
HRESULT PboDataObject::GetAsyncMode(BOOL* pfIsOpAsync)
{
    *pfIsOpAsync = m_asyncMode;
    return(S_OK);
}
HRESULT PboDataObject::StartOperation(IBindCtx*)
{
    AddRef();

    m_inAsyncOperation = TRUE;
    return(S_OK);
}
HRESULT PboDataObject::InOperation(BOOL* pfInAsyncOp)
{
    *pfInAsyncOp = m_inAsyncOperation;
    return(S_OK);
}
HRESULT PboDataObject::EndOperation(HRESULT, IBindCtx*, DWORD)
{
    m_inAsyncOperation = FALSE;
    if (m_postQuit)
        PostQuitMessage(0);

    Release();

    return(S_OK);
}


CLIPFORMAT PboDataObject::s_preferredDropEffect = (CLIPFORMAT)
RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
CLIPFORMAT PboDataObject::s_fileDescriptor = (CLIPFORMAT)
RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
CLIPFORMAT PboDataObject::s_fileContents = (CLIPFORMAT)
RegisterClipboardFormat(CFSTR_FILECONTENTS);
CLIPFORMAT PboDataObject::s_shellIdList = (CLIPFORMAT)
RegisterClipboardFormat(CFSTR_SHELLIDLIST);
CLIPFORMAT PboDataObject::s_oleClipboardPersistOnFlush = (CLIPFORMAT)
RegisterClipboardFormat(CFSTR_OLECLIPBOARDPERSISTONFLUSH);
