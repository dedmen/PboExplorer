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
#include "Util.hpp"

#ifndef __MINGW64_VERSION_MAJOR
const GUID IID_IAsyncOperation =
{ 0x3D8B0590,0xF691,0x11D2,{0x8E,0xA9,0x00,0x60,0x97,0xDF,0x5B,0xD4} };
#endif
#include "ClipboardFormatHandler.hpp"
#include <numeric>
#include "guid.hpp"
import TempDiskFile;

import Encoding;
import Tracy;

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
    //m_apidl = NULL;
    m_listsInit = false;
    m_postQuit = false;
}
PboDataObject::PboDataObject(std::shared_ptr<PboSubFolder> rootfolder, PboFolder* mainFolder, LPCITEMIDLIST pidl,
    UINT cidl, LPCITEMIDLIST* apidl)
{
    ProfilingScope pScope;
    //m_dir = dir;
    m_asyncMode = true; // OS_MIN_VISTA;
    m_inAsyncOperation = FALSE;
    m_listsInit = false;
    m_postQuit = false;

    rootFolder = std::move(rootfolder);
    pboFile = mainFolder->pboFile;
    
    m_pidlRoot = ILClone(pidl);
    m_apidl.reserve(cidl);
    for (UINT i = 0; i < cidl; i++) {
        m_apidl.emplace_back(ILClone(apidl[i]));
        auto file = (PboPidl*)m_apidl.back().GetRef();
        EXPECT_SINGLE_PIDL(file);
        DebugLogger::TraceLog(std::format("Create PboDataObject for [{}] {}", i, (rootFolder->fullPath / file->GetFilePath()).string()), std::source_location::current(), __FUNCTION__);
    }
        


    

    //dirMutex.lock();
    //m_dir->countUp();
    //dirMutex.unlock();
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
    ProfilingScope pScope;
    pScope.SetValue(DebugLogger::GetGUIDName(riid).first);
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

    if (IsEqualIID(riid, IID_IPersist)) {
        *ppvObject = static_cast<IPersistStream*>(this);
        AddRef();
        return S_OK;
    }
        
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

    // 3CEE8CC1-1ADB-327F-9B97-7A9C8089BFB3 IID_IDataObject 

    //#TODO riid = {IID_IDataObjectAsyncCapability}

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

    __debugbreak(); // should never get here
    AddRef();
    return(S_OK);
}


// https://www.codeproject.com/Reference/1091137/Windows-Clipboard-Formats


class FileGroupDescriptorHelper {
public:
    FILEGROUPDESCRIPTOR groupDescriptor;
    std::vector<FILEDESCRIPTOR> fileDescriptors;

    void AddFile(FILEDESCRIPTOR descriptor) {
        fileDescriptors.emplace_back(std::move(descriptor));
    }

    [[nodiscard]] HGLOBAL GenerateHMem() {
        size_t memSize = sizeof(FILEGROUPDESCRIPTOR) + fileDescriptors.size() * sizeof(FILEDESCRIPTOR);

        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, memSize);
        if (!hMem) 
            return nullptr;

        FILEGROUPDESCRIPTOR* fgd = (FILEGROUPDESCRIPTOR*)GlobalLock(hMem);

        fgd->cItems = fileDescriptors.size();

        int i = 0;
        for (auto& it : fileDescriptors) {
           fgd->fgd[i] = it;
            ++i;
        }

        GlobalUnlock(hMem);
        return hMem;
    }
};

class ShellIDListHelper {
public:
    ITEMIDLIST* rootPidl;
    std::vector<std::string> subPidls;

    void SetRoot(ITEMIDLIST* subPidl) {
        rootPidl = subPidl;
    }


    void AddFile(ITEMIDLIST* subPidl) {
        std::string data;

        data.resize(ILGetSize(subPidl));
        CopyMemory(data.data(), subPidl, data.size());

        subPidls.emplace_back(std::move(data));
    }

    [[nodiscard]] HGLOBAL GenerateHMem() {
        
        UINT offset = UINT(sizeof(CIDA) + sizeof(UINT) * subPidls.size());
        UINT size = offset + ILGetSize(rootPidl);
        for (const auto& it : subPidls)
            size += it.size();



        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, size);
        if (!hMem) return nullptr;

        CIDA* cida = (CIDA*)GlobalLock(hMem); //#TODO write a safety wrapper for this
        BYTE* p = (BYTE*)cida;
        p += offset;

        cida->cidl = subPidls.size();

        cida->aoffset[0] = offset;
        size = ILGetSize(rootPidl);
        CopyMemory(p, rootPidl, size);
        p += size;
        offset += size;

        for (UINT i = 0; i < subPidls.size(); i++)
        {
            cida->aoffset[1 + i] = offset;
            size = subPidls[i].size();
            CopyMemory(p, subPidls[i].data(), size);
            p += size;
            offset += size;
        }

        GlobalUnlock(hMem);

        return hMem;



    }
};







// IDataObject
HRESULT PboDataObject::GetData(FORMATETC* pformatetc, STGMEDIUM* pmedium)
{
    ProfilingScope pScope;
    wchar_t buffer[128];
    GetClipboardFormatNameW(pformatetc->cfFormat, buffer, 127);


    //DebugLogger::TraceLog(std::format("formatName {}, format {}", UTF8::Encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);

    auto type = ClipboardFormatHandler::GetTypeFromCF(pformatetc->cfFormat);

    //#TODO
    /* "NotRecyclable"
    if ( v4 == g_cfNotRecyclable )
  {
    v10 = GlobalAlloc(0, 4ui64);
    v3->hBitmap = (HBITMAP)v10;
    if ( v10 )
    {
      v3->tymed = 1;
      *v10 = 1;
LABEL_11:
      return 0;
    }
LABEL_15:
    return (unsigned int)-2147024882;
  }
    */

    if (type == ClipboardFormatHandler::ClipboardFormatType::PreferredDropEffect)
    {
        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE, sizeof(DWORD));
        if (!hMem) return E_OUTOFMEMORY;

        LPDWORD pdw = (LPDWORD)GlobalLock(hMem);
        *pdw = DROPEFFECT_COPY;
        GlobalUnlock(hMem);

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->pUnkForRelease = nullptr;
        pmedium->hGlobal = hMem;

        return S_OK;
    }

    if (type == ClipboardFormatHandler::ClipboardFormatType::FileGroupDescriptor) // Repro copy folder inside pbo
    {
        if (!(pformatetc->tymed & TYMED_HGLOBAL)) return DV_E_TYMED;

        // all the files associated with this data object
        FileGroupDescriptorHelper helper;

        for (auto& it : m_apidl) {
            auto file = (PboPidl*)it.GetRef();
            EXPECT_SINGLE_PIDL(file);

            FILEDESCRIPTOR fd;
            wcsncpy_s(fd.cFileName, file->GetFileName().filename().c_str(), MAX_PATH);
            fd.cFileName[MAX_PATH - 1] = 0;

            if (file->IsFolder()) {
                fd.dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                fd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

                // all files in folder
                auto folder = rootFolder->GetFolderByPath(file->GetFileName());
                auto rootPath = rootFolder->fullPath;

                folder->ForEachFileOrFolder([&helper, &rootPath](const IPboSub* entry) {

                    if (auto file = dynamic_cast<const PboSubFile*>(entry)) {
                        FILEDESCRIPTOR fd;

                        auto name = file->fullPath.lexically_relative(rootPath);

                        wcsncpy_s(fd.cFileName, name.c_str(), MAX_PATH);
                        fd.cFileName[MAX_PATH - 1] = 0;

                        fd.dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                        fd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                        fd.dwFlags |= FD_FILESIZE;
                        uint64_t size = file->dataSize;
                        fd.nFileSizeLow = (DWORD)size;
                        fd.nFileSizeHigh = (DWORD)(size >> 32);
                       
                        helper.AddFile(fd);
                    } else if (auto folder = dynamic_cast<const PboSubFolder*>(entry)) {
                        //FILEDESCRIPTOR fd;
                        //
                        //auto name = folder->fullPath.lexically_relative(rootPath);
                        //
                        //wcsncpy(fd.cFileName, name.c_str(), MAX_PATH);
                        //fd.cFileName[MAX_PATH - 1] = 0;
                        //
                        //fd.dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                        //fd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                        //
                        //helper.AddFile(fd);
                    }
                }, true);
            } else {
                fd.dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                fd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

                auto subFile = rootFolder->GetFileByPath(file->GetFileName());

                if (subFile) {
                    fd.dwFlags |= FD_FILESIZE;
                    uint64_t size = subFile->get().dataSize;
                    fd.nFileSizeLow = (DWORD)size;
                    fd.nFileSizeHigh = (DWORD)(size >> 32);
                }
                helper.AddFile(fd);
            }

        }

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->pUnkForRelease = nullptr;
        pmedium->hGlobal = helper.GenerateHMem();
        if (!pmedium->hGlobal)
            return E_OUTOFMEMORY;


        return S_OK;
    }

    if (type == ClipboardFormatHandler::ClipboardFormatType::ShellIDList)
    {
        if (!(pformatetc->tymed & TYMED_HGLOBAL)) return DV_E_TYMED;

        //#TODO I think the correct way to do this is to handle ITransferSource in PboFolder? zipfldr doesn't do it like this. it only has the folder in ShellIDList

        // For folder, only the folder itself

        ShellIDListHelper helper {};

        helper.SetRoot(m_pidlRoot);

        for (auto& it : m_apidl) {
            auto file = (PboPidl*)it.GetRef();
            EXPECT_SINGLE_PIDL(file);

            if (file->IsFile()) {
                helper.AddFile(it);
            } else {
                auto folder = rootFolder->GetRootFile()->GetFolderByPath(file->GetFilePath());//#TODO fix relative
                auto rootPath = rootFolder->fullPath / file->GetFilePath().parent_path(); 

                if (folder) {
                    std::vector<char> buffer;

                    //#TODO this is not recursive
                    folder->ForEachFileOrFolder([&helper, &rootPath, &buffer](const IPboSub* entry) {

                        auto name = entry->fullPath.lexically_relative(rootPath);

                        auto endType = PboPidlFileType::File;

                        if (auto file = dynamic_cast<const PboSubFile*>(entry)) {
                            endType = PboPidlFileType::File;
                        }
                        else if (auto folder = dynamic_cast<const PboSubFolder*>(entry)) {
                            return; // skip folders
                            endType = PboPidlFileType::Folder;
                        }

                        // for each file add a fully qualified pidl

                        auto pidlSize = std::accumulate(name.begin(), name.end(), 0, [](uint32_t sz, const std::filesystem::path& element) { return sz + PboPidl::GetPidlSizeForPath(element); }) + 2;

                        buffer.resize(pidlSize, 0);

                        PboPidl* nextPidl = reinterpret_cast<PboPidl*>(buffer.data());

                        for (auto& it : name) {
                            nextPidl = PboPidl::CreatePidlAt(nextPidl, it, PboPidlFileType::Folder);
                        }

                        // null terminator
                        nextPidl->cb = 0;
                        // set correct type of last element, we know that all before the last must be folders
                        reinterpret_cast<PboPidl*>(ILFindLastID(reinterpret_cast<LPITEMIDLIST>(buffer.data())))->type = endType;
                        helper.AddFile((ITEMIDLIST*)buffer.data());



                    }, true);
                }
            }
        }

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->pUnkForRelease = nullptr;
        pmedium->hGlobal = helper.GenerateHMem();

        if (!pmedium->hGlobal)
            return E_OUTOFMEMORY;

        return S_OK;
    }


    /*
    if (type == ClipboardFormatHandler::ClipboardFormatType::PreferredDropEffect)
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

    if (type == ClipboardFormatHandler::ClipboardFormatType::FileGroupDescriptor)
    {
        if (!(pformatetc->tymed & TYMED_HGLOBAL)) return(DV_E_TYMED);

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
            EXPECT_SINGLE_PIDL(file);

            FILEDESCRIPTOR* fd = &fgd->fgd[i];

            wcsncpy(fd->cFileName, file->GetFileName().filename().c_str(), MAX_PATH);
            fd->cFileName[MAX_PATH - 1] = 0;

            //#TODO add subitems inside the folder then?
            if (file->IsFolder()) {
                fd->dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                fd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            } else {

                fd->dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
                fd->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;


                auto subFile = rootFolder->GetFileByPath(file->GetFilePath());

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
    */
    if (type == ClipboardFormatHandler::ClipboardFormatType::FileContents)
    {
        if (!(pformatetc->tymed & TYMED_ISTREAM)) return(DV_E_TYMED);

        //initLists();


        LONG index = pformatetc->lindex;

        if (index < 0 || index >= m_apidl.size())
            return(DV_E_LINDEX);

        auto file = (PboPidl*)m_apidl[pformatetc->lindex].GetRef();
        EXPECT_SINGLE_PIDL(file);
        if (!file->IsFile())
            return DV_E_FORMATETC;

        pmedium->tymed = TYMED_ISTREAM;
        pmedium->pUnkForRelease = nullptr;
        pmedium->pstm = ComRef<PboFileStream>::CreateForReturn(pboFile->GetRootFile(), rootFolder->fullPath / file->GetFileName());

        return S_OK;
    }

    /*
    if (type == ClipboardFormatHandler::ClipboardFormatType::ShellIDList)
    {
        if (!(pformatetc->tymed & TYMED_HGLOBAL)) return(DV_E_TYMED);

        UINT offset = UINT(sizeof(CIDA) + sizeof(UINT) * m_apidl.size());
        UINT size = offset + ILGetSize(m_pidlRoot);
        for (const auto& it : m_apidl)
            size += ILGetSize(it);

        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, size);
        if (!hMem) return(E_OUTOFMEMORY);

        CIDA* cida = (CIDA*)GlobalLock(hMem);
        BYTE* p = (BYTE*)cida;
        p += offset;

        cida->cidl = m_apidl.size();

        cida->aoffset[0] = offset;
        size = ILGetSize(m_pidlRoot);
        CopyMemory(p, m_pidlRoot, size);
        p += size;
        offset += size;

        for (UINT i = 0; i < m_apidl.size(); i++)
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
    */
    if (type == ClipboardFormatHandler::ClipboardFormatType::OleClipboardPersistOnFlush)
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

    if (type == ClipboardFormatHandler::ClipboardFormatType::IsShowingText && pformatetc->tymed & TYMED_HGLOBAL) {

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, sizeof(DWORD));

        LPDWORD pdw = (LPDWORD)GlobalLock(pmedium->hGlobal);
        *pdw = 1;
        GlobalUnlock(pmedium->hGlobal);

        return S_OK;
    }

    if (type == ClipboardFormatHandler::ClipboardFormatType::DropDescription && pformatetc->tymed & TYMED_HGLOBAL) {

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

        pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, sizeof(wchar_t) * 20);

        auto pdw = (wchar_t*)GlobalLock(pmedium->hGlobal);
        lstrcpyW(pdw, L"Test1");
        GlobalUnlock(pmedium->hGlobal);

        return S_OK;
    }


    // We have special support for HDROP, but only if specifically requested. Its very inefficient for us.
    if (pformatetc->cfFormat == CF_HDROP && pformatetc->tymed & TYMED_HGLOBAL) {
    
        pmedium->tymed = TYMED_HGLOBAL;

        //#TODO support multiple selected files
        auto file = (PboPidl*)m_apidl[0].GetRef();

        //#TODO if this is a folder, should we copy out the whole folder??
        if (!file->IsFile())
            return DV_E_FORMATETC;

        if (!hdrop_tempfile || hdrop_tempfile->GetPboSubPath() != (rootFolder->fullPath / file->GetFilePath()))
            hdrop_tempfile = TempDiskFile::GetFile(*pboFile->GetRootFile(), rootFolder->fullPath / file->GetFileName());

        auto test = hdrop_tempfile->GetPath().native();
        pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + test.length()*sizeof(wchar_t)+ sizeof(wchar_t)*2);
    
    
        https://docs.microsoft.com/en-us/windows/win32/shell/clipboard#cf_hdrop
    
        auto pdw = (DROPFILES*)GlobalLock(pmedium->hGlobal);
        pdw->fWide = 1;
        pdw->pFiles = sizeof(DROPFILES);
        pdw++;
        wchar_t* data = (wchar_t*)pdw;
    
        lstrcpyW(data, test.c_str());
        GlobalUnlock(pmedium->hGlobal);

        //#TODO store tempfile in a IUnknown instead of member in here, then pass it through here to get cleaned up
        //pmedium->pUnkForRelease
    
        return S_OK;
    }

    // We have special support for HDROP, but only if specifically requested. Its very inefficient for us.
    if (type == ClipboardFormatHandler::ClipboardFormatType::FileName && pformatetc->tymed & TYMED_HGLOBAL) {

        pmedium->tymed = TYMED_HGLOBAL;

        //#TODO support multiple selected files
        auto file = (PboPidl*)m_apidl[0].GetRef();
        EXPECT_SINGLE_PIDL(file);

        if (!hdrop_tempfile || hdrop_tempfile->GetPboSubPath() != (rootFolder->fullPath / file->GetFilePath()))
            hdrop_tempfile = TempDiskFile::GetFile(*pboFile->GetRootFile(), rootFolder->fullPath / file->GetFilePath());

        auto test = hdrop_tempfile->GetPath().native();
        pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, test.length() * sizeof(wchar_t) + sizeof(wchar_t) * 2);

        auto pdw = (wchar_t*)GlobalLock(pmedium->hGlobal);
        lstrcpyW(pdw, test.c_str());
        GlobalUnlock(pmedium->hGlobal);

        //#TODO store tempfile in a IUnknown instead of member in here, then pass it through here to get cleaned up
        //pmedium->pUnkForRelease

        return S_OK;
    }


    //#TODO not a trace, this is a warn
    //DebugLogger::TraceLog(std::format("formatName {}, format {}, UNHANDLED", UTF8::Encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);


    return DV_E_FORMATETC;
}

HRESULT PboDataObject::GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium)
{
    ProfilingScope pScope;
    wchar_t buffer[128];
    GetClipboardFormatNameW(pformatetc->cfFormat, buffer, 127);






    DebugLogger::TraceLog(std::format("formatName {}, format {}", UTF8::Encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);
    
    auto type = ClipboardFormatHandler::GetTypeFromCF(pformatetc->cfFormat);
    
    if (type == ClipboardFormatHandler::ClipboardFormatType::FileContents && pformatetc->tymed == TYMED_ISTREAM) {
        if (m_apidl.size() != 1)
            return E_NOTIMPL; // We cannot copy multiple files into a single stream
        auto file = (PboPidl*)m_apidl[0].GetRef();
        EXPECT_SINGLE_PIDL(file);
        if (file->IsFolder())
            return DV_E_FORMATETC; // No, you cannot get file contents of a folder

        auto inpStream = ComRef<PboFileStream>::Create(pboFile->GetRootFile(), rootFolder->fullPath / file->GetFilePath());
        ULARGE_INTEGER bytes;
        bytes.QuadPart = -1;
        return inpStream->CopyTo(pmedium->pstm, bytes, nullptr, nullptr);
    }

    //#TODO not a trace, this is a warn
    DebugLogger::TraceLog(std::format("formatName {}, format {}, UNHANDLED", UTF8::Encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);


    //return(DV_E_FORMATETC);
    return E_NOTIMPL;
}

HRESULT PboDataObject::QueryGetData(FORMATETC* pformatetc)
{
    ProfilingScope pScope;
    wchar_t buffer[128];
    GetClipboardFormatNameW(pformatetc->cfFormat, buffer, 127);


    //DebugLogger::TraceLog(std::format("formatName {}, format {}", UTF8::Encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);


    auto type = ClipboardFormatHandler::GetTypeFromCF(pformatetc->cfFormat);



    auto file = (PboPidl*)m_apidl[0].GetRef();
    EXPECT_SINGLE_PIDL(file);
    if (file && file->IsFolder()) {
        // folder is very limited

        if (type == ClipboardFormatHandler::ClipboardFormatType::PreferredDropEffect
            || type == ClipboardFormatHandler::ClipboardFormatType::FileGroupDescriptor
            || type == ClipboardFormatHandler::ClipboardFormatType::ShellIDList
            )
            return S_OK;
    }



    if (type == ClipboardFormatHandler::ClipboardFormatType::PreferredDropEffect
        || type == ClipboardFormatHandler::ClipboardFormatType::FileGroupDescriptor
        || type == ClipboardFormatHandler::ClipboardFormatType::FileContents
        || type == ClipboardFormatHandler::ClipboardFormatType::ShellIDList
        || type == ClipboardFormatHandler::ClipboardFormatType::OleClipboardPersistOnFlush
        || pformatetc->cfFormat == CF_UNICODETEXT
        || pformatetc->cfFormat == CF_HDROP // We don't actually offer HDROP as its inefficient, but if you really want it we can provide a tempfile. https://msdn.microsoft.com/en-us/library/bb773269.aspx 
        || type == ClipboardFormatHandler::ClipboardFormatType::FileName // This depends on tempfile just like HDROP
        )
        return S_OK;

    //DebugLogger::TraceLog(std::format("formatName {}, format {} NOT AVAIL", UTF8::Encode(buffer), pformatetc->cfFormat), std::source_location::current(), __FUNCTION__);


    return DV_E_FORMATETC;
}

HRESULT PboDataObject::GetCanonicalFormatEtc(FORMATETC*, FORMATETC*)
{
    return(E_NOTIMPL);
}

HRESULT PboDataObject::SetData(FORMATETC* tf, STGMEDIUM* med, BOOL b)
{
    ProfilingScope pScope;
    wchar_t buffer[128];
    GetClipboardFormatNameW(tf->cfFormat, buffer, 127);


    DebugLogger::TraceLog(std::format("formatName {}, format {}", UTF8::Encode(buffer), tf->cfFormat), std::source_location::current(), __FUNCTION__);



    // https://docs.microsoft.com/en-us/windows/win32/shell/clipboard#cfstr_preferreddropeffect
    // Because the target is not obligated to honor the request, the target must call the source's IDataObject::SetData method with a CFSTR_PERFORMEDDROPEFFECT format to tell the data object which operation was actually performed.

    // it will tell us here if a drag-out of a file is supposed to be a move, in which case we need to trigger a delete

    auto type = ClipboardFormatHandler::GetTypeFromCF(tf->cfFormat);
    if (type == ClipboardFormatHandler::ClipboardFormatType::PreferredDropEffect) {

        LPDWORD pdw = (LPDWORD)GlobalLock(med->hGlobal);
        if (*pdw == DROPEFFECT_MOVE){
            // We did a move out, we need to delete the file from the pbo!
            currentDropMode = *pdw;
            //#TODO store current mode
            Util::TryDebugBreak();
        }
        GlobalUnlock(med->hGlobal);
    }
    else if (type == ClipboardFormatHandler::ClipboardFormatType::DropDescription) {
        auto pdw = (DROPDESCRIPTION*)GlobalLock(med->hGlobal);

        GlobalUnlock(med->hGlobal);
    }
    else if (type == ClipboardFormatHandler::ClipboardFormatType::PasteSucceeded) {
        Util::TryDebugBreak();
        //auto pdw = (DROPDESCRIPTION*)GlobalLock(med->hGlobal);
        //GlobalUnlock(med->hGlobal);
            // if (currentDropMode == DROPEFFECT_MOVE) then delete the file
        if (hdrop_tempfile) {
            hdrop_tempfile.reset();
        }

    }

    else if (type == ClipboardFormatHandler::ClipboardFormatType::PerformedDropEffect) {
        // https://docs.microsoft.com/en-us/windows/win32/shell/clipboard#cfstr_performeddropeffect

        //#TODO know if we were a cut (also implement ::Save to save whether operation is a cut op) and delete file and do stuffz

        LPDWORD pdw = (LPDWORD)GlobalLock(med->hGlobal);
        if (*pdw == DROPEFFECT_MOVE) {
            // We did a move out, we need to delete the file from the pbo!
            currentDropMode = *pdw;
            //#TODO store current mode
            Util::TryDebugBreak();
        }
        GlobalUnlock(med->hGlobal);

    }
    else if (tf->tymed == TYMED_HGLOBAL) {
        wchar_t buffer[128];
        GetClipboardFormatNameW(tf->cfFormat, buffer, 127);

        LPDWORD pdw = (LPDWORD)GlobalLock(med->hGlobal);
        //__debugbreak();
        GlobalUnlock(med->hGlobal);
    }

    if (b)
        ReleaseStgMedium(med);

    return S_OK;
}

class FormatEnumerator : GlobalRefCounted, public RefCountedCOM<FormatEnumerator, IEnumFORMATETC> {

public:
    struct Format {
        CLIPFORMAT cfFormat;
        tagTYMED tymed;
    };

    std::vector<Format> formats;
    int m_pos = 0;


    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    // IEnumFORMATETC
    HRESULT STDMETHODCALLTYPE Next(
        ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) override;
    HRESULT STDMETHODCALLTYPE Skip(DWORD celt) override;
    HRESULT STDMETHODCALLTYPE Reset(void) override;
    HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** ppenum) override;

    ~FormatEnumerator() override {}
};


// IUnknown
HRESULT FormatEnumerator::QueryInterface(REFIID riid, void** ppvObject)
{
    ProfilingScope pScope;
    pScope.SetValue(DebugLogger::GetGUIDName(riid).first);
    DebugLogger_OnQueryInterfaceEntry(riid);
    if (COMJoiner::QueryInterfaceJoiner(riid, ppvObject)) {
        AddRef();
        return S_OK;
    }
    else RIID_IGNORED(IID_IExternalConnection);
    else RIID_IGNORED(IID_IProvideClassInfo);
    //else RIID_IGNORED(IID_IInspectable);
    else RIID_IGNORED(IID_ICallFactory);
    else
    {
        DebugLogger_OnQueryInterfaceExitUnhandled(riid);
        *ppvObject = nullptr;
        return(E_NOINTERFACE);
    }

    return(S_OK);
}


// IEnumFORMATETC
HRESULT FormatEnumerator::Next(
    ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched)
{
    ULONG i = 0;

    auto toFetch = std::min((size_t)celt, formats.size() - m_pos);

    for (size_t i = 0; i < toFetch; i++)
    {
        auto& format = formats[i + m_pos];

        FORMATETC fe = { 0, nullptr,DVASPECT_CONTENT,-1,0 };
        fe.cfFormat = format.cfFormat;
        fe.tymed = format.tymed;
        rgelt[i] = fe;
    }

    m_pos += toFetch;

    if (pceltFetched)
        *pceltFetched = toFetch;

    return (celt == toFetch ? S_OK : S_FALSE);
}

HRESULT FormatEnumerator::Skip(DWORD celt)
{
    m_pos += celt;
    return(S_OK);
}
HRESULT FormatEnumerator::Reset(void)
{
    m_pos = 0;
    return(S_OK);
}
HRESULT FormatEnumerator::Clone(IEnumFORMATETC** ppenum)
{
    auto newEnumerator = ComRef<FormatEnumerator>::CreateForReturn<IEnumFORMATETC>(ppenum);

    newEnumerator->formats = formats;
    newEnumerator->m_pos = m_pos;

    return S_OK;
}


HRESULT PboDataObject::EnumFormatEtc(
    DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc)
{
    if (dwDirection != DATADIR_GET) return(E_NOTIMPL);

    auto newEnumerator = ComRef<FormatEnumerator>::CreateForReturn<IEnumFORMATETC>(ppenumFormatEtc);

    if (m_apidl.empty())
        return S_OK;

    auto file = (PboPidl*)m_apidl[0].GetRef();
    EXPECT_SINGLE_PIDL(file);

    if (file->IsFolder()) {
        newEnumerator->formats.emplace_back(FormatEnumerator::Format{ ClipboardFormatHandler::GetCFFromType(ClipboardFormatHandler::ClipboardFormatType::FileGroupDescriptor) , TYMED_HGLOBAL });
        newEnumerator->formats.emplace_back(FormatEnumerator::Format{ ClipboardFormatHandler::GetCFFromType(ClipboardFormatHandler::ClipboardFormatType::ShellIDList) , TYMED_HGLOBAL });
    } else {
        newEnumerator->formats.emplace_back(FormatEnumerator::Format{ ClipboardFormatHandler::GetCFFromType(ClipboardFormatHandler::ClipboardFormatType::FileGroupDescriptor) , TYMED_HGLOBAL });
        newEnumerator->formats.emplace_back(FormatEnumerator::Format{ ClipboardFormatHandler::GetCFFromType(ClipboardFormatHandler::ClipboardFormatType::FileContents) , TYMED_ISTREAM });
        newEnumerator->formats.emplace_back(FormatEnumerator::Format{ ClipboardFormatHandler::GetCFFromType(ClipboardFormatHandler::ClipboardFormatType::PreferredDropEffect) , TYMED_HGLOBAL });
        newEnumerator->formats.emplace_back(FormatEnumerator::Format{ ClipboardFormatHandler::GetCFFromType(ClipboardFormatHandler::ClipboardFormatType::OleClipboardPersistOnFlush), TYMED_HGLOBAL });
    }

   
    //newEnumerator->formats.emplace_back(FormatEnumerator::Format{ CF_UNICODETEXT, TYMED_HGLOBAL });

   

    wchar_t buffer[128];
    GetClipboardFormatNameW(ClipboardFormatHandler::GetCFFromType(ClipboardFormatHandler::ClipboardFormatType::OleClipboardPersistOnFlush), buffer, 127);
    return S_OK;
}
HRESULT PboDataObject::DAdvise(
    FORMATETC* format, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection)
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
HRESULT PboDataObject::IsDirty(void)
{
    return S_FALSE; // We can't change
}

HRESULT PboDataObject::Load(IStream* pStm)
{
    ProfilingScope pScope;
    HRESULT hr;
    if (FAILED(hr = QLoadFromStream(pStm, &m_pidlRoot)))
        return(hr);

    uint32_t pidlCount;
    DWORD dwRead;
    if (FAILED(hr = pStm->Read(&pidlCount, sizeof(pidlCount), &dwRead)))
        return(hr);

    m_apidl.resize(pidlCount);

    for (size_t i = 0; i < pidlCount; i++)
    {
        if(FAILED(hr = QLoadFromStream(pStm, &m_apidl[i])))
            return(hr);
    }

    uint16_t pboPathLen;
    if (FAILED(hr = pStm->Read(&pboPathLen, sizeof(pboPathLen), NULL)))
        return hr;
    std::wstring pboPath;
    pboPath.resize(pboPathLen);
    if (FAILED(hr = pStm->Read(pboPath.data(), pboPathLen * 2, NULL)))
        return hr;

    uint16_t subFolderPathLen;
    if (FAILED(hr = pStm->Read(&subFolderPathLen, sizeof(subFolderPathLen), NULL)))
        return hr;
    std::wstring subFolderPath;
    subFolderPath.resize(subFolderPathLen);
    if (FAILED(hr = pStm->Read(subFolderPath.data(), subFolderPathLen * 2, NULL)))
        return hr;

    // find our pbo file
    pboFile = GPboFileDirectory.GetPboFile(pboPath);
    if (!pboFile)
        return E_FAIL; // pbo doesn't exist anymore

    // find our subdirectory, if we have one
    rootFolder = pboFile->GetFolderByPath(subFolderPath);

    if (!rootFolder)
        return E_FAIL; // parent directory inside the pbo doesn't exist anymore.


    // Verify that our actual file still exists.

    for (auto& it : m_apidl) {
        auto file = (PboPidl*)it.GetRef();
        EXPECT_SINGLE_PIDL(file);
        if (!file->IsValidPidl())
            return E_FAIL; // no idea what this is, but its not a valid PboPidl

        if (file->IsFile() && !pboFile->GetFileByPath(file->GetFilePath())) //#TODO tryDebugBreak on fail
            return E_FAIL; // File inside the pbo doesn't exist anymore
        else if (file->IsFolder() && !pboFile->GetFolderByPath(file->GetFilePath()))
            return E_FAIL; // Folder inside the pbo doesn't exist anymore
    }



    return S_OK;
}
 
HRESULT PboDataObject::Save(IStream* pStm, BOOL)
{
    ProfilingScope pScope;
    HRESULT hr;
    if (FAILED(hr = QSaveToStream(pStm, m_pidlRoot)))
        return(hr);

    uint32_t pidlCount = m_apidl.size();
    if (FAILED(hr = pStm->Write(&pidlCount, sizeof(pidlCount), NULL)))
        return(hr);

    for (auto& it : m_apidl)
    {
        if (FAILED(hr = QSaveToStream(pStm, it)))
            return(hr);
    }

    std::wstring pboPath = pboFile->GetPboDiskPath().native();
    uint16_t pboPathLen = pboPath.length();
    if (FAILED(hr = pStm->Write(&pboPathLen, sizeof(pboPathLen), NULL)))
        return hr;
    if (FAILED(hr = pStm->Write(pboPath.data(), pboPathLen*2, NULL)))
        return hr;

    std::wstring subFolderPath = rootFolder->fullPath.native();
    uint16_t subFolderPathLen = subFolderPath.length();
    if (FAILED(hr = pStm->Write(&subFolderPathLen, sizeof(subFolderPathLen), NULL)))
        return hr;
    if (FAILED(hr = pStm->Write(subFolderPath.data(), subFolderPathLen * 2, NULL)))
        return hr;

    //#TODO persist current drop mode (cut or copy)


    return S_OK;
}

HRESULT PboDataObject::GetSizeMax(ULARGE_INTEGER* res)
{
    res->QuadPart = 
        (uint64_t)ILGetSize(m_pidlRoot)
        + std::accumulate(m_apidl.begin(), m_apidl.end(), 0ul, [](uint32_t inp, const CoTaskMemRefS<ITEMIDLIST>& pidl) { return ILGetSize(pidl); })
        + pboFile->GetPboDiskPath().native().length()
        + rootFolder->fullPath.native().length()
        + 16 // its fake, but... we want a safe max ¯\_(ツ)_/¯
        ;

    return S_OK;
}


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
