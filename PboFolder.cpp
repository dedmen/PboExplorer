#include "PboFolder.hpp"

#include <codecvt>
#include <Shlwapi.h>
#pragma comment( lib, "Shlwapi" )

#include "ClassFactory.hpp"
#include <functional>
#include <mutex>

#include "PboContextMenu.hpp"
#include "PboDataObject.hpp"
#include "PboFileStream.hpp"
#include "PboPidl.hpp"
#include <string_view>

#include "Util.hpp"

#include <ranges>
#include <utility>

PboFile::PboFile()
{
    rootFolder = std::make_shared<PboSubFolder>();
}

void PboFile::ReadFrom(std::filesystem::path inputPath)
{
    diskPath = inputPath;
    std::ifstream readStream(inputPath, std::ios::in | std::ios::binary);
	
    PboReader reader(readStream);
    reader.readHeaders();
    std::vector<std::filesystem::path> segments;
    for (auto& it : reader.getFiles())
    {
        std::filesystem::path filePath(Util::utf8_decode(it.name));
        segments.clear();
   
        for (auto& it : filePath)
        {
            segments.emplace_back(it);
        }

        auto fileName = segments.back();
        segments.pop_back();

        std::shared_ptr<PboSubFolder> curFolder = rootFolder;
    	for (auto& it : segments)
    	{
            auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
                {
                    return subf->filename == it;
                });
    		
            if (subfolderFound != curFolder->subfolders.end())
            {
                curFolder = *subfolderFound;
                continue;
            }
            auto newSub = std::make_shared<PboSubFolder>();
            newSub->filename = it;
            newSub->fullPath = filePath.parent_path();
            curFolder->subfolders.emplace_back(std::move(newSub));
            curFolder = curFolder->subfolders.back();
    	}

        PboSubFile newFile;
        newFile.filename = fileName;
        newFile.filesize = it.original_size;
        newFile.dataSize = it.data_size;
        newFile.startOffset = it.startOffset;
        newFile.fullPath = filePath;
        curFolder->subfiles.emplace_back(std::move(newFile));	
    }
}



//#TODO We need to make PboFile's re-scan if their origin pbo was repacked by FileWatcher
std::optional<std::reference_wrapper<PboSubFile>> PboFile::GetFileByPath(std::filesystem::path inputPath) const
{
    std::shared_ptr<PboSubFolder> curFolder = rootFolder;

	// get proper root directory in case we are a subfolder inside a pbo
    auto relPath = inputPath.lexically_relative(rootFolder->fullPath);

	for (auto& it : relPath)
	{
	    auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
	        {
	            return subf->filename == it;
	        });

        if (subfolderFound != curFolder->subfolders.end())
        {
            curFolder = *subfolderFound;
            continue;
        }

        auto subfileFound = std::find_if(curFolder->subfiles.begin(), curFolder->subfiles.end(), [it](const PboSubFile& subf)
            {
                return subf.filename == it;
            });

        if (subfileFound == curFolder->subfiles.end())
            return {};

        return *subfileFound;		
	}
    return {};
}

std::shared_ptr<PboSubFolder> PboFile::GetFolderByPath(std::filesystem::path inputPath) const
{
    std::shared_ptr<PboSubFolder> curFolder = rootFolder;
    for (auto& it : inputPath)
    {
        auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
            {
                return subf->filename == it;
            });

        if (subfolderFound != curFolder->subfolders.end())
        {
            curFolder = *subfolderFound;
        }
    }
    return curFolder;
}

std::vector<PboPidl> PboFile::GetPidlListFromPath(std::filesystem::path inputPath) const {

    std::vector<PboPidl> resultPidl;

    std::shared_ptr<PboSubFolder> curFolder = rootFolder;

    // get proper root directory in case we are a subfolder inside a pbo
    auto relPath = inputPath.lexically_relative(rootFolder->fullPath);

    for (auto& it : relPath)
    {
        auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
            {
                return subf->filename == it;
            });

        if (subfolderFound != curFolder->subfolders.end())
        {
            // every pidl has fullpath, can't we just shorten this to 1 pidl, lets try...
            //resultPidl.emplace_back(PboPidl{ sizeof(PboPidl), PboPidlFileType::Folder, (*subfolderFound)->fullPath, -1 });
            curFolder = *subfolderFound;
            continue;
        }

        auto subfileFound = std::find_if(curFolder->subfiles.begin(), curFolder->subfiles.end(), [it](const PboSubFile& subf)
            {
                return subf.filename == it;
            });


        if (subfileFound != curFolder->subfiles.end()) {

            resultPidl.emplace_back(PboPidl{ sizeof(PboPidl), PboPidlFileType::File, (*subfileFound).fullPath, -1 });
            return resultPidl;
        }
    }


    Util::TryDebugBreak();
    // not found, pidl is still valid, just not for full path
    return resultPidl;
}


#define CHECK_INIT() \
  if( !checkInit() ) return( E_FAIL )

PboFolder::PboFolder()
{
    m_shellBrowser = nullptr;
}


PboFolder::~PboFolder()
{
}


HRESULT PboFolder::QueryInterface(const IID& riid, void** ppvObject)
{
    if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = (IUnknown*)(IShellFolder*)this;
    else if (IsEqualIID(riid, IID_IShellFolder))
        *ppvObject = (IShellFolder*)this;
    else if (IsEqualIID(riid, IID_IShellFolder2))
        *ppvObject = (IShellFolder2*)this;
    else if (IsEqualIID(riid, IID_IPersist))
        *ppvObject = (IPersist*)this;
    else if (IsEqualIID(riid, IID_IPersistFolder))
        *ppvObject = (IPersistFolder*)this;
    else if (IsEqualIID(riid, IID_IPersistFolder2))
        *ppvObject = (IPersistFolder2*)this;
    else if (IsEqualIID(riid, IID_IPersistFolder3))
        *ppvObject = (IPersistFolder3*)this;
    else if (IsEqualIID(riid, IID_IThumbnailHandlerFactory))
        *ppvObject = (IThumbnailHandlerFactory*)this;

    //IID_IObjectWithFolderEnumMode 
	
    else
    {
        *ppvObject = nullptr;
        return(E_NOINTERFACE);
    }

    AddRef();
    return(S_OK);
}

HRESULT PboFolder::ParseDisplayName(HWND hwnd, LPBC pbc, LPOLESTR pszDisplayName, ULONG* pchEaten, LPITEMIDLIST* ppidl,
	ULONG* pdwAttributes)
{
    CHECK_INIT();


    std::filesystem::path subPath(pszDisplayName);
    auto pidlList = pboFile->GetPidlListFromPath(subPath.lexically_normal());


    if (pidlList.empty())
        return(E_FAIL); // doesn't exist


    //#TODO if we didn't find the final file (might be a file inside a zip file INSIDE a pbo that we don't have access to) we need to set pchEaten to the end of the path we were able to resolve...
    // just get relative path between last pidlList entry and pszDisplayName, then scan from end? problem is stuff like /../ which can be anywhere in path



    auto resPidl = (LPITEMIDLIST)CoTaskMemAlloc(pidlList.size() * sizeof(PboPidl) + sizeof(PboPidl::cb));
    std::memset(resPidl, 0, pidlList.size() * sizeof(PboPidl) + sizeof(PboPidl::cb));
    *ppidl = resPidl;

    char* outputBuf = (char*)resPidl;
    for (auto& it : pidlList) {
        *((PboPidl*)outputBuf) = it; //#TODO convert pidl to store filepath as dynamic length string, we cannot store std::filesystem::path pointer to disk
        outputBuf += sizeof(it);
    }
    ((PboPidl*)outputBuf)->cb = 0;


    PboPidl* qp = (PboPidl*)ILFindLastID(*ppidl);
    ULONG eaten = 0;
    if (qp)
    {
        if (pdwAttributes)
            GetAttributesOf(1, (LPCITEMIDLIST*)&qp, pdwAttributes);
    
        eaten = (ULONG)wcslen(pszDisplayName); // #TODO do this properly
    }
    
    if (pchEaten) *pchEaten = eaten;
    return(S_OK);
}






class QiewerEnumIDList :
    GlobalRefCounted,
	public RefCountedCOM<QiewerEnumIDList, IEnumIDList>
{
public:
    QiewerEnumIDList(PboFolder& owner, std::shared_ptr<PboSubFolder>, DWORD flags);
    ~QiewerEnumIDList();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    // IEnumIDList
    HRESULT STDMETHODCALLTYPE Next(
        ULONG celt, LPITEMIDLIST* rgelt, ULONG* pceltFetched);
    HRESULT STDMETHODCALLTYPE Skip(DWORD celt);
    HRESULT STDMETHODCALLTYPE Reset(void);
    HRESULT STDMETHODCALLTYPE Clone(IEnumIDList** ppenum);

private:

    DWORD flags;
    std::shared_ptr<PboSubFolder> subFolder;
    PboFolder* owner;
    int m_typePos;
    int m_idxPos;
};


QiewerEnumIDList::QiewerEnumIDList(PboFolder& owner, std::shared_ptr<PboSubFolder> count1, DWORD flags): flags(flags), subFolder(std::move(count1)), owner(&owner)
{
    m_typePos = m_idxPos = 0;
    
    owner.AddRef();
}
QiewerEnumIDList::~QiewerEnumIDList()
{
    owner->Release();
}


// IUnknown
HRESULT QiewerEnumIDList::QueryInterface(REFIID riid, void** ppvObject)
{
    if (IsEqualIID(riid, IID_IEnumIDList))
        *ppvObject = (IEnumIDList*)this;
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


// IEnumIDList
HRESULT QiewerEnumIDList::Next(
    ULONG celt, LPITEMIDLIST* rgelt, ULONG* pceltFetched)
{
    ULONG i = 0;
    for (; m_typePos < 2; )
    {
        int qty = !m_typePos ? subFolder->subfolders.size() : subFolder->subfiles.size();

        //https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-_shcontf
        if (!(flags & SHCONTF_STORAGE))
        {
            if (!(flags & SHCONTF_FOLDERS) && !m_typePos) qty = 0;
            if (!(flags & SHCONTF_NONFOLDERS) && m_typePos) qty = 0;
        }


        for (; i < celt && m_idxPos < qty; m_idxPos++)
        {
            auto structSize = sizeof(PboPidl) + sizeof(USHORT);
            PboPidl* qp = (PboPidl*)CoTaskMemAlloc(structSize);
            memset(qp, 0, structSize);
            qp->cb = (USHORT)sizeof(PboPidl);
            qp->type = m_typePos ? PboPidlFileType::File : PboPidlFileType::Folder;
            //qp->idx = m_idxPos;
            if (m_typePos)
                qp->filePath = subFolder->subfiles[m_idxPos].fullPath.wstring(); // I don't know why plain copy assign doesn't work. This is needed or the qp->filePath is corrupt
            else
                qp->filePath = subFolder->subfolders[m_idxPos]->fullPath.wstring();
            qp->dbgIdx = m_idxPos;
        	
            //qp->filePath = !m_typePos ? subFolder->subfolders[m_idxPos]->fullPath : subFolder->subfiles[m_idxPos].fullPath;
            rgelt[i] = (LPITEMIDLIST)qp;
            i++;
        }

        if (m_idxPos >= qty)
        {
            m_typePos++;
            m_idxPos = 0;
        }

        if (i >= celt) break;
    }

    if (pceltFetched)
        *pceltFetched = i;

    return(celt == i ? S_OK : S_FALSE);
}
HRESULT QiewerEnumIDList::Skip(DWORD celt)
{
    while (m_typePos < 2 && celt>0)
    {
        int qty = m_typePos ? subFolder->subfolders.size() : subFolder->subfiles.size();

        int typeRest = qty - m_idxPos;
        if (typeRest > (int)celt) typeRest = celt;

        celt -= typeRest;
        m_idxPos += typeRest;

        if (m_idxPos >= qty)
        {
            m_typePos++;
            m_idxPos = 0;
        }
    }

    return(S_OK);
}
HRESULT QiewerEnumIDList::Reset(void)
{
    m_typePos = m_idxPos = 0;
    return(S_OK);
}
HRESULT QiewerEnumIDList::Clone(IEnumIDList** ppenum)
{
    *ppenum = nullptr;
    return(E_NOTIMPL);
}











HRESULT PboFolder::EnumObjects(HWND hwnd, DWORD grfFlags, IEnumIDList** ppenumIDList)
{
    CHECK_INIT();

	// we want to split folder and non-folder here
    
    
    //#TODO forward flags
    //int dirQty = pboFile->rootFolder
    ////int streamDirQty = m_dir->getArchiveQty();
    //int streamQty = 0;
    //if (!(grfFlags & SHCONTF_STORAGE))
    //{
    //    if (!(grfFlags & SHCONTF_FOLDERS)) dirQty = 0;
    //    if (!(grfFlags & SHCONTF_NONFOLDERS)) streamQty = 0;
    //}
    *ppenumIDList = new QiewerEnumIDList(*this, pboFile->rootFolder, grfFlags);
    
    return(S_OK);
}

HRESULT PboFolder::BindToObject(LPCITEMIDLIST pidl, LPBC bindContext, const IID& riid, void** ppv)
{
    CHECK_INIT();

#ifdef _DEBUG
    if (IsEqualIID(riid, IID_IPropertyStoreCache) ||
        IsEqualIID(riid, IID_IPropertyStoreFactory) ||
        IsEqualIID(riid, IID_IPreviewItem) ||
        IsEqualIID(riid, IID_IPropertyStore)
        )
        return(E_NOINTERFACE);
#endif
	
    if (IsEqualIID(riid, IID_IShellFolder) ||
        IsEqualIID(riid, IID_IShellFolder2))
    {
        auto subPidl = (const PboPidl*)pidl;

    	if (subPidl->IsFile()) // cannot enter file as if its a folder
            return(E_FAIL);
    	
        //Dir* dir = childDir(m_dir, (const PboPidl*)pidl);
        //if (!dir) return(E_FAIL);
        
        auto qf = new PboFolder();
        qf->pboFile = std::make_shared<PboFile>();
        qf->pboFile->rootFolder = pboFile->GetFolderByPath(subPidl->filePath);
        qf->pboFile->diskPath = pboFile->diskPath;
    	
        if (!qf)
        {
            return(E_OUTOFMEMORY);
        }
        
        LPITEMIDLIST pidlAbs = ILCombine(m_pidl, pidl);
        qf->m_pidl = pidlAbs;

    	
        auto hr = qf->QueryInterface(riid, ppv);
        
        //qf->Release();
        
        return(hr);
    }
    else if (IsEqualIID(riid, IID_IStream))
    {

        Util::WaitForDebuggerPrompt();

        LPITEMIDLIST pidld = ILClone(pidl);
        ILRemoveLastID(pidld);
        const PboPidl* qp = (const PboPidl*)pidld;
        //Dir* dir = childDir(m_dir, qp);
        CoTaskMemFree(pidld);
        //if (!dir)
        //    return(E_FAIL);
        
        //Stream* stream = NULL;
        
        qp = (const PboPidl*)ILFindLastID(pidl);


        auto stream = new PboFileStream(pboFile, qp->filePath);
    	
        //dirMutex.lock();
        //if (qp->type == 1)
        //    stream = dir->getArchiveStream(qp->idx);
        //else if (qp->type == 2)
        //    stream = dir->getStream(qp->idx);
        //dirMutex.unlock();
        
        HRESULT hr = E_FAIL;
        if (stream)
        {
            stream->AddRef();
            *ppv = static_cast<IStream*>(stream);
            hr = S_OK;
        }
        
        //dirMutex.lock();
        //dir->countDown();
        //dirMutex.unlock();
        
        return(hr);
    }

    return(E_NOINTERFACE);
}

HRESULT PboFolder::BindToStorage(LPCITEMIDLIST pidl, LPBC pbc, const IID& riid, void** ppv)
{
    return(E_NOTIMPL);
}

HRESULT PboFolder::CompareIDs(LPARAM lParam, LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
    CHECK_INIT();


	
    LPITEMIDLIST pidl1d = ILClone(pidl1);
    //ILRemoveLastID(pidl1d);
    const PboPidl* qp1 = (const PboPidl*)pidl1d;
    //Dir* dir1 = childDir(m_dir, qp1);
    CoTaskMemFree(pidl1d);
    //if (!dir1)
    //    return(MAKE_HRESULT(0, 0, 0));
    //
    LPITEMIDLIST pidl2d = ILClone(pidl2);
    ILRemoveLastID(pidl2d);
    const PboPidl* qp2 = (const PboPidl*)pidl2d;
    //Dir* dir2 = childDir(m_dir, qp2);
    CoTaskMemFree(pidl2d);
    //if (!dir2)
    //{
    //    dirMutex.lock();
    //    dir1->countDown();
    //    dirMutex.unlock();
    //    return(MAKE_HRESULT(0, 0, 0));
    //}
    
    qp1 = (const PboPidl*)ILFindLastID(pidl1);
    qp2 = (const PboPidl*)ILFindLastID(pidl2);
    
    short res;
    //int type1 = qp1->type;
    //int idx1 = qp1->idx;
    //int type2 = qp2->type;
    //int idx2 = qp2->idx;
    //#TODO sorted by column! Name, Size, Date

    res = -1;
	
    //switch (lParam & 0xff)
    //{
    //default:
    //{
    //    if (type1 != type2)
    //    {
    //        res = type1 < type2 ? -1 : 1;
    //        break;
    //    }
    //
    //    if (dir1 == dir2)
    //    {
    //        res = idx1 < idx2 ? -1 : (idx1 > idx2 ? 1 : 0);
    //        break;
    //    }
    //
    //    const wchar_t* name1 = type1 == 1 ? dir1->getArchiveName(idx1) :
    //        (type1 == 2 ? dir1->getStreamName(idx1) : dir1->getDirName(idx1));
    //    const wchar_t* name2 = type2 == 1 ? dir2->getArchiveName(idx2) :
    //        (type2 == 2 ? dir2->getStreamName(idx2) : dir2->getDirName(idx2));
    //    res = (short)LARRA_NAME_SORT(name1, name2);
    //
    //    if (!res)
    //        res = (short)LARRA_NAME_SORT(dir1->getName(), dir2->getName());
    //}
    //break;
    //case 1:
    //{
    //    qsize size1 = type1 == 1 ? dir1->getArchiveSize(idx1) :
    //        (type1 == 2 ? dir1->getStreamSize(idx1) : -1);
    //    qsize size2 = type2 == 1 ? dir2->getArchiveSize(idx2) :
    //        (type2 == 2 ? dir2->getStreamSize(idx2) : -1);
    //    res = size1 < size2 ? -1 : (size1 > size2 ? 1 : 0);
    //}
    //break;
    //case 2:
    //{
    //    int64_t date1 = type1 == 1 ? dir1->getArchiveDate(idx1) :
    //        (type1 == 2 ? dir1->getStreamDate(idx1) : dir1->getDirDate(idx1));
    //    int64_t date2 = type2 == 1 ? dir2->getArchiveDate(idx2) :
    //        (type2 == 2 ? dir2->getStreamDate(idx2) : dir2->getDirDate(idx2));
    //    res = date1 < date2 ? -1 : (date1 > date2 ? 1 : 0);
    //}
    //break;
    //}
    
    //dirMutex.lock();
    //dir1->countDown();
    //dir2->countDown();
    //dirMutex.unlock();
    
    return(MAKE_HRESULT(0, 0, (unsigned short)res));
    //return(E_NOTIMPL);
}

HRESULT PboFolder::CreateViewObject(HWND hwnd, const IID& riid, void** ppv)
{
    *ppv = nullptr;

#ifdef _DEBUG
    //if (IsEqualIID(riid, IID_ITopViewAwareItem))
    //    return(E_NOTIMPL);
    //if (IsEqualIID(riid, IID_IFrameLayoutDefinition))
    //    return(E_NOTIMPL);
#endif

    if (IsEqualIID(riid, IID_IShellView))
    {
        HRESULT hr = E_FAIL;

        HMODULE mod = GetModuleHandle(L"shell32.dll");
        if (mod)
        {
            typedef HRESULT WINAPI csfv_func(
                const SFV_CREATE*, IShellView**);
            csfv_func* csfvf = (csfv_func*)GetProcAddress(
                mod, "SHCreateShellFolderView");

            if (csfvf)
            {
                SFV_CREATE csfv = { (UINT)sizeof(csfv),(IShellFolder*)this, nullptr,
                  (IShellFolderViewCB*)this };
                hr = csfvf(&csfv, (IShellView**)ppv);
            }
        }

        if (!m_shellBrowser)
        {
#define WM_GETISHELLBROWSER (WM_USER+7)
            m_shellBrowser = (IShellBrowser*)SendMessageW(
                hwnd, WM_GETISHELLBROWSER, 0, 0);
        }

        return(hr);
    }


    //#TODO https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/winui/shell/shellextensibility/explorerdataprovider/ExplorerDataProvider.cpp#L606
	

    return(E_NOTIMPL);
}

HRESULT PboFolder::GetAttributesOf(UINT cidl, LPCITEMIDLIST* apidl, SFGAOF* rgfInOut)
{
    if (cidl != 1)
    {
        *rgfInOut &= SFGAO_CANCOPY;
        return(S_OK);
    }

    const PboPidl* qp = (const PboPidl*)apidl[0];
    //#TODO update types. Folder, File. Not Folder, Folder/File, File, I already commented it out, should be fine
	switch (qp->type)
    {
    case PboPidlFileType::Folder:
        *rgfInOut =
            SFGAO_BROWSABLE | SFGAO_HASSUBFOLDER | SFGAO_CANCOPY |
            SFGAO_FOLDER | SFGAO_FILESYSANCESTOR | SFGAO_STORAGEANCESTOR;
        break;
    
    //case 1:
    //    *rgfInOut &=
    //        SFGAO_BROWSABLE | SFGAO_HASSUBFOLDER | SFGAO_CANCOPY |
    //        SFGAO_FOLDER | SFGAO_FILESYSANCESTOR | SFGAO_STREAM;
    //    break;
    //
    case PboPidlFileType::File:
        *rgfInOut = SFGAO_CANCOPY | SFGAO_STREAM;
        break;
    }

    return(S_OK);
}

HRESULT PboFolder::GetUIObjectOf(HWND hwndOwner, UINT cidl, LPCITEMIDLIST* apidl, const IID& riid, UINT* rgfReserved,
	void** ppv)
{

    // https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/winui/shell/shellextensibility/explorerdataprovider/ExplorerDataProvider.cpp#L673

	
    if (IsEqualIID(riid, IID_IContextMenu))
    {


        auto dcm = new DEFCONTEXTMENU{ hwndOwner, nullptr, m_pidl, this, 1, apidl, nullptr,0 ,nullptr };
        //return SHCreateDefaultContextMenu(dcm, riid, ppv);

        auto ret = new PboContextMenu(this, hwndOwner, m_pidl, cidl, apidl);
        ret->AddRef();
        *ppv = static_cast<IContextMenu*>(ret);
        return(S_OK);
    }
    //else if (IsEqualIID(riid, IID_IExtractIconW))
    //{
    //    const PboPidl* qp = (const PboPidl*)apidl[0];
    //    *ppv = (IExtractIconW*)&icons[qp->type];
    //    return(S_OK);
    //}
    else if (IsEqualIID(riid, IID_IDataObject))
    {
        CHECK_INIT();

        auto ret = new PboDataObject(pboFile->rootFolder, this, m_pidl, cidl, apidl);
        ret->AddRef();
        *ppv = (IDataObject*)ret;
        return(S_OK);
    }

    else if (riid == IID_IExtractIconW)
    {
        ComRef<IDefaultExtractIconInit> pdxi;
        auto hr = SHCreateDefaultExtractIcon(IID_PPV_ARGS(&pdxi));
        
        if (SUCCEEDED(hr))
        {
            const PboPidl* qp = (const PboPidl*)apidl[0];

            ComRef<IQueryAssociations> qa;

            HRESULT hr;
            if (qp->IsFolder())
            {
                ASSOCIATIONELEMENT const rgAssocFolder[] =
                {
                    { ASSOCCLASS_PROGID_STR, nullptr, L"Folder"},
                    { ASSOCCLASS_FOLDER, nullptr, nullptr},
                };
                hr = AssocCreateForClasses(rgAssocFolder, ARRAYSIZE(rgAssocFolder), IID_IQueryAssociations, qa.AsQueryInterfaceTarget());
            }
            else
            {
                auto dest = qp->filePath.filename();

                const wchar_t* name = dest.c_str();
                if (!name) return(E_FAIL);

                const wchar_t*  point = wcsrchr(name, '.');


                ASSOCIATIONELEMENT const rgAssocItem[] =
                {
                    { ASSOCCLASS_PROGID_STR, nullptr, point},
                    { ASSOCCLASS_STAR, nullptr, nullptr},
                    { ASSOCCLASS_SYSTEM_STR, nullptr, point},


                    // ASSOCCLASS_SYSTEM_STR The pszClass member names a key found as HKEY_CLASSES_ROOT\SystemFileAssociations\pszClass.
                };
                hr = AssocCreateForClasses(rgAssocItem, ARRAYSIZE(rgAssocItem), IID_IQueryAssociations, qa.AsQueryInterfaceTarget());
            }


            wchar_t buffer[512];
            DWORD bufferSz = 510;
            hr = qa->GetString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_DEFAULTICON, nullptr, buffer, &bufferSz); bufferSz = 510;

            if (!SUCCEEDED(hr))
                return hr;
        	
            std::wstring_view iconPath(buffer);

            auto iconIDIndex = iconPath.find_last_of(',');

            int iconIndex = 0;

            if (iconIDIndex != std::string::npos)
            {
                auto substring = iconPath.substr(iconIDIndex+1);
                iconIndex = std::stoi(std::wstring(substring));
                iconPath = iconPath.substr(0, iconIDIndex);
            }

            std::wstring iconPS(iconPath);
     
            hr = pdxi->SetNormalIcon(iconPS.c_str(), iconIndex);
            hr = pdxi->SetDefaultIcon(iconPS.c_str(), iconIndex);
            hr = pdxi->SetOpenIcon(iconPS.c_str(), iconIndex);
            
            if (SUCCEEDED(hr))
            {
                hr = pdxi->QueryInterface(riid, ppv);
            }
        }
        return hr;
    }
	
    else if (IsEqualIID(riid, IID_IQueryAssociations))
    {
    // ??????!
    // We can use SHAssocEnumHandlers interface to get the file association with specific file extension, ex .png

    //Then use IAssocHandlerand can retrieves the full pathand file name of the executable file associated with the file type(.png).ex: ['Paint':'C:\\Windows\\system32\\mspaint.exe', ...]



        if (cidl != 1) return(E_INVALIDARG);
    
        CHECK_INIT();
        IQueryAssociations* qa;
        const PboPidl* qp = (const PboPidl*)apidl[0];
        const wchar_t* point = nullptr;

        // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-associationelement

        HRESULT hr;
        if (qp->IsFolder())
        {
            ASSOCIATIONELEMENT const rgAssocFolder[] =
            {
                { ASSOCCLASS_PROGID_STR, nullptr, L"Folder"},
                { ASSOCCLASS_FOLDER, nullptr, nullptr},
            };
            hr = AssocCreateForClasses(rgAssocFolder, ARRAYSIZE(rgAssocFolder), IID_IQueryAssociations, ppv);
            qa = (IQueryAssociations*)*ppv;
        }
        else
        {
                auto dest = qp->filePath.filename();
            
                const wchar_t* name = dest.c_str();
                if (!name) return(E_FAIL);
            
                point = wcsrchr(name, '.');

        	
            ASSOCIATIONELEMENT const rgAssocItem[] =
            {
                { ASSOCCLASS_PROGID_STR, nullptr, point},
                { ASSOCCLASS_STAR, nullptr, nullptr},
                { ASSOCCLASS_SYSTEM_STR, nullptr, point},


            	// ASSOCCLASS_SYSTEM_STR The pszClass member names a key found as HKEY_CLASSES_ROOT\SystemFileAssociations\pszClass.
            };
            hr = AssocCreateForClasses(rgAssocItem, ARRAYSIZE(rgAssocItem), IID_IQueryAssociations, ppv);
            qa = (IQueryAssociations*)*ppv;
        }






    	
        //if (qp->type == 0)
        //{
        //    point = L"Folder";
        //    HRESULT hr = AssocCreate(CLSID_QueryAssociations, riid, (void**)&qa);
        //    if (FAILED(hr)) return(hr);
        //    hr = qa->Init(ASSOCF_INIT_DEFAULTTOFOLDER, point, NULL, NULL);
        //    if (FAILED(hr))
        //    {
        //        qa->Release();
        //        return(hr);
        //    }
        //}  
        //else
        //{
        //    auto dest = qp->filePath.filename();
        //
        //    const wchar_t* name = dest.c_str();
        //    if (!name) return(E_FAIL);
        //
        //    point = wcsrchr(name, '.');
        //
        //
        //    HRESULT hr = AssocCreate(CLSID_QueryAssociations, riid, (void**)&qa);
        //    if (FAILED(hr)) return(hr);
        //    hr = qa->Init(ASSOCF_INIT_DEFAULTTOSTAR, point, NULL, NULL);
        //    if (FAILED(hr))
        //    {
        //        qa->Release();
        //        return(hr);
        //    }
        //}
        //if (!point) return(E_FAIL);



        wchar_t riidStr[40], clsidStr[40];
  
        StringFromGUID2(IID_IExtractIconA, riidStr, ARRAYSIZE(riidStr));
        DWORD clsidSize = ARRAYSIZE(clsidStr);
        hr = qa->GetString(ASSOCF_NOUSERSETTINGS, ASSOCSTR_SHELLEXTENSION, riidStr, clsidStr, &clsidSize);

        //sfsdfsfsd break
        /*
         *
    windows.storage.dll!CShellItem::BindToHandler()	Unknown
 	windows.storage.dll!_CreateThumbnailHandler()	Unknown // here
 	windows.storage.dll!CShellItem::BindToHandler()	Unknown
 	windows.storage.dll!GetExtractIconW()	Unknown
 	windows.storage.dll!_GetILIndexFromItem()	Unknown
 	windows.storage.dll!SHGetIconIndexFromPIDL()	Unknown
 	windows.storage.dll!MapIDListToIconILIndex()	Unknown
 	windows.storage.dll!CLoadSystemIconTask::InternalResumeRT()	Unknown

         *
         *
         * https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/nn-shlwapi-iqueryassociations
* https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-iqueryassociations-getkey
* https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-iqueryassociations-getstring
* https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ithumbnailhandlerfactory-getthumbnailhandler
* https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nn-shlobj_core-iextracticona
* https://docs.microsoft.com/en-us/windows/win32/shell/how-to-create-icon-handlers
* https://docs.microsoft.com/en-us/windows/win32/api/objidl/nn-objidl-ipersistfile
* https://www.google.com/search?client=firefox-b-d&q=IQueryAssociations+ERROR_NO_ASSOCIATION
* https://github.com/wine-mirror/wine/blob/master/dlls/shell32/assoc.c
* https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/ne-shlwapi-assocstr
* https://github.com/cryptoAlgorithm/nt5src/search?q=ERROR_NO_ASSOCIATION
*  !!!!!!!!!
* https://github.com/cryptoAlgorithm/nt5src/blob/daad8a087a4e75422ec96b7911f1df4669989611/Source/XPSP1/NT/shell/shell32/defviewp.h
* https://github.com/cryptoAlgorithm/nt5src/search?p=2&q=CShellBrowser
* https://github.com/cryptoAlgorithm/nt5src/search?p=1&q=CShellBrowser
* https://github.com/cryptoAlgorithm/nt5src/blob/daad8a087a4e75422ec96b7911f1df4669989611/Source/XPSP1/NT/shell/browseui/shbrows2.h
* https://github.com/cryptoAlgorithm/nt5src/blob/daad8a087a4e75422ec96b7911f1df4669989611/Source/XPSP1/NT/shell/shell32/filefldr.cpp#L123
* https://github.com/cryptoAlgorithm/nt5src/blob/daad8a087a4e75422ec96b7911f1df4669989611/Source/XPSP1/NT/shell/shell32/defview.cpp
* https://www.google.com/search?client=firefox-b-d&q=IID_IExtractIconW+ERROR_NO_ASSOCIATION
* https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfoa
         * 
         */
    
        
        wchar_t buffer[512];
        DWORD bufferSz = 510;
        hr = qa->GetString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_COMMAND, nullptr, buffer, &bufferSz); bufferSz = 510;
        hr = qa->GetString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_EXECUTABLE, nullptr, buffer, &bufferSz); bufferSz = 510;
        hr = qa->GetString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_DEFAULTICON, nullptr, buffer, &bufferSz); bufferSz = 510;
        hr = qa->GetString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_PROGID, nullptr, buffer, &bufferSz); bufferSz = 510;
        hr = qa->GetString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_APPICONREFERENCE, nullptr, buffer, &bufferSz);
    
        *ppv = qa;
        return(S_OK);
    }
    else if (IsEqualIID(riid, IID_IExtractImage))
    {
        if (cidl != 1) return(E_INVALIDARG);
    
        return(GetThumbnailHandler(apidl[0], nullptr, riid, ppv));
    }

    return(E_NOINTERFACE);
}

static HRESULT stringToStrRet(std::wstring_view str, STRRET* sr)
{
    LPWSTR copy = (LPWSTR)CoTaskMemAlloc((str.length() + 1) * sizeof(wchar_t));
    if (!copy) return(E_OUTOFMEMORY);

    wcscpy_s(copy, (str.length() + 1), str.data());

    sr->uType = STRRET_WSTR;
    sr->pOleStr = copy;

    return(S_OK);
}

HRESULT PboFolder::GetDisplayNameOf(LPCITEMIDLIST pidl, SHGDNF uFlags, STRRET* pName)
{
    CHECK_INIT();
    
    const PboPidl* qp = (const PboPidl*)pidl;

    auto dest = qp->filePath.filename();
	
    const wchar_t* name = dest.c_str();
    if (!name) return(E_FAIL);
    
    if ((uFlags & (SHGDN_FORPARSING | SHGDN_INFOLDER)) != SHGDN_FORPARSING)
        return(stringToStrRet(name, pName));

    auto fullPath = pboFile->diskPath / qp->filePath.filename();

    HRESULT hr = stringToStrRet(fullPath.wstring(), pName);

    return hr;
}

HRESULT PboFolder::SetNameOf(HWND hwnd, LPCITEMIDLIST pidl, LPCOLESTR pszName, SHGDNF uFlags, LPITEMIDLIST* ppidlOut)
{
    return(E_NOTIMPL);
}

HRESULT PboFolder::GetDefaultSearchGUID(GUID*)
{
    return(E_NOTIMPL);
}

HRESULT PboFolder::EnumSearches(IEnumExtraSearch** ppEnum)
{
    *ppEnum = nullptr;
    return(E_NOTIMPL);
}

HRESULT PboFolder::GetDefaultColumn(DWORD, ULONG* pSort, ULONG* pDisplay)
{
    *pSort = *pDisplay = 0;
    return(S_OK);
}

HRESULT PboFolder::GetDefaultColumnState(UINT iColumn, SHCOLSTATEF* pcsFlags)
{
    switch (iColumn)
    {
    case 0:
        *pcsFlags = SHCOLSTATE_ONBYDEFAULT | SHCOLSTATE_TYPE_STR;
        return(S_OK);

    case 1:
        *pcsFlags = SHCOLSTATE_ONBYDEFAULT | SHCOLSTATE_TYPE_INT;
        return(S_OK);

    case 2:
        *pcsFlags = SHCOLSTATE_ONBYDEFAULT | SHCOLSTATE_TYPE_DATE;
        return(S_OK);
    }

    return(E_INVALIDARG);
}


#define DEFINE_PROPERTYKEY( name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8,pid ) \
  EXTERN_C const PROPERTYKEY DECLSPEC_SELECTANY name = { \
    { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }

// #TODO custom GUID!
DEFINE_PROPERTYKEY(PKEY_ItemNameDisplay, 0xB725F130, 0x47EF, 0x101A, 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC, 10);
DEFINE_PROPERTYKEY(PKEY_Size, 0xB725F130, 0x47EF, 0x101A, 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC, 12);
DEFINE_PROPERTYKEY(PKEY_DateModified, 0xB725F130, 0x47EF, 0x101A, 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC, 14);
//DEFINE_PROPERTYKEY(PKEY_PerceivedType, 0x28636AA6, 0x953D, 0x11D2, 0xB5, 0xD6, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xD0, 9);

//DEFINE_PROPERTYKEY(PKEY_ItemFolderPathDisplay, 0xE3E0584C, 0xB788, 0x4A5A, 0xBB, 0x20, 0x7F, 0x5A, 0x44, 0xC9, 0xAC, 0xDD, 6);

static HRESULT stringToVariant(std::wstring_view str, VARIANT* pv)
{
    pv->vt = VT_BSTR;
    pv->bstrVal = SysAllocStringLen(str.data(), str.length());
    return(pv->bstrVal ? S_OK : E_OUTOFMEMORY);
}

HRESULT PboFolder::GetDetailsEx(LPCITEMIDLIST pidl, const SHCOLUMNID* pscid, VARIANT* pv)
{
    CHECK_INIT();

    wchar_t path[MAX_PATH];
    SHGetPathFromIDList(pidl, path);

    if (pidl->mkid.cb != sizeof(PboPidl))
        pidl = ILNext(pidl);

	
    const PboPidl* qp = (const PboPidl*)pidl;

    OutputDebugStringW(L"PboFolder::GetDetailsEx");
    OutputDebugStringW(qp->filePath.c_str());
    OutputDebugStringA("\n");
    
    if (pscid->fmtid == PKEY_ItemNameDisplay.fmtid &&
        pscid->pid == PKEY_ItemNameDisplay.pid)
    {
        return(stringToVariant(qp->filePath.filename().wstring(), pv));
    }
    else if (pscid->fmtid == PKEY_Size.fmtid && pscid->pid == PKEY_Size.pid)
    {
        if (!qp->IsFile()) return(E_FAIL);

        auto file = pboFile->GetFileByPath(qp->filePath);
        if (!file)
            return E_FAIL;

        auto size = file->get().filesize;
    
        pv->vt = VT_I8;
        pv->llVal = size;
    
        return(S_OK);
    }
    //else if (pscid->fmtid == PKEY_DateModified.fmtid &&
    //    pscid->pid == PKEY_DateModified.pid)
    //{
    //    int64_t date = qp->type == 1 ? m_dir->getArchiveDate(qp->idx) :
    //        (qp->type == 2 ? m_dir->getStreamDate(qp->idx) :
    //            m_dir->getDirDate(qp->idx));
    //    FILETIME ft;
    //    if (!getFileTimeFromDate(date, &ft)) return(E_FAIL);
    //
    //    SYSTEMTIME st;
    //    FileTimeToSystemTime(&ft, &st);
    //
    //    pv->vt = VT_DATE;
    //    SystemTimeToVariantTime(&st, &pv->date);
    //
    //    return(S_OK);
    //}
    //else if (pscid->fmtid == PKEY_PerceivedType.fmtid &&
    //    pscid->pid == PKEY_PerceivedType.pid)
    //{
    //    const wchar_t* type = L"";
    //
    //    return(stringToVariant(type, pv));
    //}
    else if (pscid->fmtid == FMTID_WebView && pscid->pid == PID_DISPLAY_PROPERTIES)
    {
        return(stringToVariant(L"prop:Name;Write;Size", pv));
    }
	//#TODO
    //else if (pscid->fmtid == FMTID_PropList)
    //{
    //    const wchar_t* props = NULL;
    //
    //    switch (pscid->pid)
    //    {
    //    case PID_PROPLIST_TILEINFO:
    //    case PID_PROPLIST_INFOTIP:
    //        props = L"prop:Size;Write";
    //        break;
    //
    //    case PID_PROPLIST_PREVIEWTITLE:
    //        props = L"prop:Name";
    //        break;
    //
    //    case PID_PROPLIST_PREVIEWDETAILS:
    //        props = L"prop:Write;Size";
    //        break;
    //
    //    case PID_PROPLIST_CONTENTVIEWMODEFORBROWSE:
    //        props = L"prop:Name;Write;Size";
    //        break;
    //    }
    //
    //    if (props)
    //        return(stringToVariant(props, pv));
    //}
    //else if (pscid->fmtid == PKEY_ItemFolderPathDisplay.fmtid &&
    //    pscid->pid == PKEY_ItemFolderPathDisplay.pid)
    //{
    //    HRESULT hr = stringToVariant(m_dir->getName(), pv);
    //    if (FAILED(hr)) return(hr);
    //
    //    return(S_OK);
    //}

	
    return(E_INVALIDARG);
}

#define COLUMN_TITLE_SIZE 32
struct columnInfo
{
    std::wstring title;
    int fmt, cxChar;
};

static columnInfo columnInfos[2] = { {L"",0,0},{L"",0,0} };


static HRESULT getColumnTitle(int idx, SHELLDETAILS* psd)
{
    HRESULT hr = S_OK;
    static std::once_flag columnTitleOnceFlag;
    static std::atomic_bool readyFlag;

    std::call_once(columnTitleOnceFlag, [&]() {
        ComRef<IShellFolder> sf;
        hr = SHGetDesktopFolder(sf.AsQueryInterfaceTarget<IShellFolder>());
        if (FAILED(hr)) return;

        ComRef<IShellFolder2> sf2;
        hr = sf->QueryInterface(IID_IShellFolder2, sf2.AsQueryInterfaceTarget());
        if (FAILED(hr)) return;

        for (int i = 0; i < 2; i++) {
            int j = i;
            if (j == 2) j = 3;

            SHELLDETAILS sd;
            hr = sf2->GetDetailsOf(nullptr, j, &sd);
            if (FAILED(hr)) break;

            columnInfos[i].fmt = sd.fmt;
            columnInfos[i].cxChar = sd.cxChar;

            CoTaskMemRefS<wchar_t> str;
            hr = StrRetToStr(&sd.str, nullptr, &str);
            if (FAILED(hr)) break;
            columnInfos[i].title = str;
        }
        readyFlag = true;
        });

    while (!readyFlag)
        YieldProcessor();

    if (FAILED(hr)) return(hr);

    const columnInfo& ci = columnInfos[idx];

    psd->fmt = ci.fmt;
    psd->cxChar = ci.cxChar;

    return(stringToStrRet(ci.title, &psd->str));
}

HRESULT PboFolder::GetDetailsOf(LPCITEMIDLIST pidl, UINT iColumn, SHELLDETAILS* psd)
{
    if (!pidl)
    {
        if (iColumn > 1) return(E_INVALIDARG);

        return(getColumnTitle(iColumn, psd));
    }

    CHECK_INIT();

    const PboPidl* qp = (const PboPidl*)pidl;

    switch (iColumn)
    {
    case 0: // Name
        return(GetDisplayNameOf(pidl, 0, &psd->str));

    case 1: // Size
    {
        if (!qp->IsFile()) return(E_FAIL);

        auto file = pboFile->GetFileByPath(qp->filePath);
        if (!file)
            return E_FAIL;
        auto size = file->get().filesize;
        if (size < 0) return(E_FAIL);

#define SIZE_STR_SIZE 40
        wchar_t sizeStr[SIZE_STR_SIZE];
        if (!StrFormatByteSizeW(size, sizeStr, SIZE_STR_SIZE))
            return(E_FAIL);

        return(stringToStrRet(sizeStr, &psd->str));
    }
    }

    return(E_FAIL);
}


HRESULT PboFolder::MapColumnToSCID(UINT iColumn, SHCOLUMNID* pscid)
{
    switch (iColumn)
    {
    case 0:
        *pscid = PKEY_ItemNameDisplay;
        return(S_OK);
    
    case 1:
        *pscid = PKEY_Size;
        return(S_OK);
    
    case 2:
        *pscid = PKEY_DateModified;
        return(S_OK);
    }

    return(E_INVALIDARG);
}

HRESULT PboFolder::GetClassID(CLSID* pClassID)
{
    *pClassID = CLSID_PboExplorer;

    return(S_OK);
}

HRESULT PboFolder::Initialize(LPCITEMIDLIST pidl)
{
    if (m_pidl)
        return(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED));

    m_pidl = ILClone(pidl);
    if (!m_pidl) return(E_OUTOFMEMORY);

    checkInit();

	
    return(S_OK);
}

HRESULT PboFolder::GetCurFolder(LPITEMIDLIST* ppidl)
{
    *ppidl = ILClone(m_pidl);
    return(*ppidl ? S_OK : E_OUTOFMEMORY);
}

HRESULT PboFolder::InitializeEx(IBindCtx* pbc, LPCITEMIDLIST pidlRoot, const PERSIST_FOLDER_TARGET_INFO* ppfti)
{
    if (ppfti)
    {
        auto subPidl = ppfti->pidlTargetFolder;

        auto qp = (const PboPidl*)ILFindLastID(m_pidl);
        int count = 0;
        while (subPidl = ILGetNext(subPidl)) {
            //subPidl = (LPITEMIDLIST)((uintptr_t)subPidl + subPidl->mkid.cb);
            auto test = (PboPidl*)subPidl;
            if (subPidl->mkid.cb == sizeof(PboPidl))
                __debugbreak();
            count++;

        }
    }
	
  


	
    return(Initialize(pidlRoot));
}

HRESULT PboFolder::GetFolderTargetInfo(PERSIST_FOLDER_TARGET_INFO* ppfti)
{
    return(E_NOTIMPL);
}

HRESULT PboFolder::MessageSFVCB(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case SFVM_DEFVIEWMODE:
        *(FOLDERVIEWMODE*)lParam = FVM_DETAILS;
        return(S_OK);
    }

    return(E_NOTIMPL);
}


HRESULT PboFolder::GetThumbnailHandler(LPCITEMIDLIST pidlChild, IBindCtx* pbc, const IID& riid, void** ppv)
{
    ComRef<IQueryAssociations> qa;
    HRESULT hr = GetUIObjectOf(nullptr, 1, &pidlChild, IID_IQueryAssociations, nullptr, qa.AsQueryInterfaceTarget());
    if (FAILED(hr)) return(hr);
    
    wchar_t riidStr[40], clsidStr[40];
    StringFromGUID2(riid, riidStr, ARRAYSIZE(riidStr));
    DWORD clsidSize = ARRAYSIZE(clsidStr);


    wchar_t buffer[512];
    DWORD bufferSz = 512;
    qa->GetString(ASSOCF_NONE, ASSOCSTR_COMMAND, nullptr, buffer, &bufferSz); bufferSz = 512;
    qa->GetString(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, nullptr, buffer, &bufferSz); bufferSz = 512;
    qa->GetString(ASSOCF_NONE, ASSOCSTR_DEFAULTICON, nullptr, buffer, &bufferSz); bufferSz = 512;
    qa->GetString(ASSOCF_NONE, ASSOCSTR_PROGID, nullptr, buffer, &bufferSz);

    qa->GetString(ASSOCF_NOUSERSETTINGS, ASSOCSTR_COMMAND, nullptr, buffer, &bufferSz); bufferSz = 512;
    qa->GetString(ASSOCF_NOUSERSETTINGS, ASSOCSTR_EXECUTABLE, nullptr, buffer, &bufferSz); bufferSz = 512;
    qa->GetString(ASSOCF_NONE, ASSOCSTR_DEFAULTICON, nullptr, buffer, &bufferSz); bufferSz = 512;
	
	//IID_IExtractIconW
    hr = qa->GetString(ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, riidStr, clsidStr, &clsidSize);

    StringFromGUID2(IID_IExtractIconA, riidStr, ARRAYSIZE(riidStr));
    clsidSize = ARRAYSIZE(clsidStr);
    hr = qa->GetString(ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, riidStr, clsidStr, &clsidSize);

    StringFromGUID2(IID_IExtractIconW, riidStr, ARRAYSIZE(riidStr));
    clsidSize = ARRAYSIZE(clsidStr);
    hr = qa->GetString(ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, L"{BB2E617C-0920-11d1-9A0B-00C04FC2D6C1}", clsidStr, &clsidSize);

    clsidSize = ARRAYSIZE(clsidStr);
    hr = qa->GetString(ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, L"", clsidStr, &clsidSize);
	
    if (SUCCEEDED(hr))
    {
        CLSID clsid;
        hr = CLSIDFromString(clsidStr, &clsid);
        if (SUCCEEDED(hr))
        {
            IUnknown* unk;
            hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, riid, reinterpret_cast<void**>(&unk));
            if (SUCCEEDED(hr))
            {
                ComRef<IInitializeWithItem> iwi;
                hr = unk->QueryInterface(IID_IInitializeWithItem, iwi.AsQueryInterfaceTarget());
                if (SUCCEEDED(hr))
                {
                    ComRef<IShellItem> si;
                    hr = SHCreateShellItem(nullptr, this, pidlChild, si.AsQueryInterfaceTarget<IShellItem>());
                    if (SUCCEEDED(hr))
                    {
                        hr = iwi->Initialize(si, STGM_READ);
                        if (SUCCEEDED(hr))
                        {
                            *ppv = unk;
                            unk = nullptr;
                        }
                    }
                }
    
                if (unk)
                    unk->Release();
            }
        }
    }
        
    return(hr);
}

bool PboFolder::checkInit()
{
    if (pboFile) return(true);
    if (!m_pidl) return(false);

    wchar_t path[MAX_PATH];
    SHGetPathFromIDList(m_pidl, path);


    OutputDebugStringW(L"PboFolder::checkInit");
    OutputDebugStringW(path);
    OutputDebugStringA("\n");


    ITEMIDLIST* subPidl = m_pidl;

    auto qp = (const PboPidl*)ILFindLastID(m_pidl);
    int count = 0;
    while (subPidl = ILGetNext(subPidl)) {
        //subPidl = (LPITEMIDLIST)((uintptr_t)subPidl + subPidl->mkid.cb);
        auto test = (PboPidl*)subPidl;
        if (subPidl->mkid.cb == sizeof(PboPidl))
            __debugbreak();
        count++;

    }


	
    pboFile = std::make_unique<PboFile>();
    pboFile->ReadFrom(path);

	
    //dirMutex.lock();
    //if (!m_dir)
    //    m_dir = Dir::getAbsDir(path);
    //dirMutex.unlock();

    //return(m_dir != NULL);
    return true;
}



std::filesystem::path PboFolder::GetTempDir()
{
    if (m_tempDir.empty())
    {

        char name2[L_tmpnam];
        tmpnam_s(name2);

    	
        m_tempDir = std::filesystem::temp_directory_path() / name2;
        std::filesystem::create_directory(m_tempDir); //#TODO cleanup on dtor
    }


    return m_tempDir;
}