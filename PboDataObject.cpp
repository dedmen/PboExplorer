#define _CRT_SECURE_NO_WARNINGS
#include "PboDataObject.hpp"

#define NOMINMAX
#include <utility>

#include "PboFileStream.hpp"
#include "PboPidl.hpp"

#ifndef FD_PROGRESSUI
#define FD_PROGRESSUI 0x00004000
#endif

#ifndef CFSTR_OLECLIPBOARDPERSISTONFLUSH
#define CFSTR_OLECLIPBOARDPERSISTONFLUSH TEXT("OleClipboardPersistOnFlush")
#endif

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
    if (IsEqualIID(riid, IID_IDataObject))
        *ppvObject = (IDataObject*)this;
    else if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = (IUnknown*)(IDataObject*)this;
    else if (IsEqualIID(riid, IID_IPersistStream))
        *ppvObject = (IPersistStream*)this;
    //else if (IsEqualIID(riid, IID_IAsyncOperation))
    //    *ppvObject = (IAsyncOperation*)this;
    else
    {
        *ppvObject = nullptr;
        //__debugbreak();
        return(E_NOINTERFACE);
    }

    AddRef();
    return(S_OK);
}


// IDataObject
HRESULT PboDataObject::GetData(
    FORMATETC* pformatetc, STGMEDIUM* pmedium)
{
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

        
        //#TODO if m_cidl == 1, we only have a single file (for example drag&drop), we should only return that one file
    	
        int dirCount = rootFolder->subfolders.size();
        int fileCount = rootFolder->subfiles.size();
        int count = dirCount + fileCount;
        size_t memSize = sizeof(FILEGROUPDESCRIPTOR) + (count - 1) * sizeof(FILEDESCRIPTOR);

        HGLOBAL hMem = GlobalAlloc(
            GMEM_MOVEABLE | GMEM_SHARE | GMEM_DISCARDABLE | GMEM_ZEROINIT, memSize);
        if (!hMem) return(E_OUTOFMEMORY);

        FILEGROUPDESCRIPTOR* fgd = (FILEGROUPDESCRIPTOR*)GlobalLock(hMem);

        fgd->cItems = count;

        for (int i = 0; i < dirCount; i++)
        {
            FILEDESCRIPTOR* fd = &fgd->fgd[i];

            if (m_offset)
                wcscpy(fd->cFileName, m_offset);
            wcsncpy(fd->cFileName + offsetLen, rootFolder->subfolders[i]->filename.c_str(), MAX_PATH - offsetLen);
            fd->cFileName[MAX_PATH - 1] = 0;

            fd->dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI;
            fd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        }

        for (int i = 0; i < fileCount; i++)
        {
            FILEDESCRIPTOR* fd = &fgd->fgd[dirCount + i];

            if (m_offset)
                wcscpy(fd->cFileName, m_offset);
            wchar_t* fileName = fd->cFileName + offsetLen;
            wcsncpy(fileName, rootFolder->subfiles[i].filename.c_str(), MAX_PATH - offsetLen);
            fd->cFileName[MAX_PATH - 1] = 0;

            fd->dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_PROGRESSUI;
            fd->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

            //wchar_t* delim = wcsrchr(fileName, '\\');

            //if (delim)
            //{
            //    delim[0] = 0;
            //    const wchar_t* rootName = rootFolder->filename.c_str();
            //    //wchar_t* fullName = (wchar_t*)malloc(
            //    //    (wcslen(rootName) + wcslen(fileName) + 2) * sizeof(wchar_t));
            //    //wcscpy_s(fullName, 500, rootName);
            //    //wcscat_s(fullName, 500, L"\\");
            //    //wcscat_s(fullName, 500, fileName);
            //
            //    //dirMutex.lock();
            //    //dir = Dir::getAbsDir(fullName);
            //    //dirMutex.unlock();
            //
            //    //free(fullName);
            //    delim++[0] = '\\';
            //}
            //else
            //{
            //    //dir = m_dir;
            //    //dirMutex.lock();
            //    //dir->countUp();
            //    //dirMutex.unlock();
            //
            //    delim = fileName;
            //}
        	
            //if (type)
            //{
                fd->dwFlags |= FD_FILESIZE;

                uint64_t size = rootFolder->subfiles[i].dataSize;
                fd->nFileSizeLow = (DWORD)size;
                fd->nFileSizeHigh = (DWORD)(size >> 32);
            //}

            //int64_t date = type == 1 ? dir->getArchiveDate(idx) :
            //    (type == 2 ? dir->getStreamDate(idx) : dir->getDirDate(idx));
            //if (getFileTimeFromDate(date, &fd->ftLastWriteTime))
            //    fd->dwFlags |= FD_WRITESTIME;

            //dirMutex.lock();
            //dir->countDown();
            //dirMutex.unlock();
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

        int dirCount = rootFolder->subfolders.size();
        int fileCount = rootFolder->subfiles.size();
        LONG index = pformatetc->lindex;
        if (index < dirCount || index >= dirCount + fileCount)
            return(DV_E_LINDEX);

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

        IStream* qs = new PboFileStream(pboFolder->pboFile, rootFolder->subfiles[index - dirCount].fullPath);
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

    return(DV_E_FORMATETC);
}
HRESULT PboDataObject::GetDataHere(FORMATETC*, STGMEDIUM*)
{
    return(E_NOTIMPL);
}
HRESULT PboDataObject::QueryGetData(FORMATETC*)
{
    return(E_NOTIMPL);
}
HRESULT PboDataObject::GetCanonicalFormatEtc(FORMATETC*, FORMATETC*)
{
    return(E_NOTIMPL);
}
HRESULT PboDataObject::SetData(FORMATETC*, STGMEDIUM*, BOOL)
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
    if (IsEqualIID(riid, IID_IEnumFORMATETC))
        *ppvObject = (IEnumFORMATETC*)this;
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
