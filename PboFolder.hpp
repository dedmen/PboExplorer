#pragma once
#define NOMINMAX
#include <ShlObj.h>
#include <Windows.h>
#include <filesystem>

#include "ComRef.hpp"
#include "PboPidl.hpp"
#include "TempDiskFile.hpp"


class IPboSub
{
public:
    std::wstring filename;
    std::filesystem::path fullPath;
};

class PboSubFile : public IPboSub
{
public:
    uint32_t filesize;
    uint32_t dataSize;
    uint32_t startOffset;
};

class PboSubFolder : public IPboSub
{
public:
    std::vector<PboSubFile> subfiles;
    std::vector<std::shared_ptr<PboSubFolder>> subfolders;

    std::optional<std::reference_wrapper<const PboSubFile>> GetFileByPath(std::filesystem::path inputPath) const;
};


class PboFile
{
public:
    PboFile();
    std::filesystem::path diskPath;
    std::shared_ptr<PboSubFolder> rootFolder;
    std::filesystem::path selfRelPath;
	
    void ReadFrom(std::filesystem::path inputPath);
    void ReloadFrom(std::filesystem::path inputPath);
    std::optional<std::reference_wrapper<PboSubFile>> GetFileByPath(std::filesystem::path inputPath) const;
    std::shared_ptr<PboSubFolder> GetFolderByPath(std::filesystem::path inputPath) const;
    std::vector<PboPidl> GetPidlListFromPath(std::filesystem::path inputPath) const;
};





//todo IDropTarget
//https ://docs.microsoft.com/en-us/windows/win32/api/oleidl/nn-oleidl-idroptarget
class PboFolder:
	GlobalRefCounted,
	public RefCountedCOM<PboFolder,
	                     IShellFolder2,
	                     IPersistFolder3,
	                     IShellFolderViewCB,
	                     IThumbnailHandlerFactory,
                         IDropTarget
	>
{
    bool checkInit(void);

    CoTaskMemRefS<ITEMIDLIST> m_pidl;

    std::filesystem::path m_tempDir;
    IShellBrowser* m_shellBrowser;

public:

    std::shared_ptr<PboFile> pboFile;
	PboFolder();
    ~PboFolder();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    // IShellFolder
    HRESULT STDMETHODCALLTYPE ParseDisplayName(
        HWND hwnd, LPBC pbc, LPOLESTR pszDisplayName,
        ULONG* pchEaten, LPITEMIDLIST* ppidl, ULONG* pdwAttributes);
    HRESULT STDMETHODCALLTYPE EnumObjects(
        HWND hwnd, DWORD grfFlags, IEnumIDList** ppenumIDList);
    HRESULT STDMETHODCALLTYPE BindToObject(
        LPCITEMIDLIST pidl, LPBC, REFIID riid, void** ppv);
    HRESULT STDMETHODCALLTYPE BindToStorage(
        LPCITEMIDLIST pidl, LPBC pbc, REFIID riid, void** ppv);
    HRESULT STDMETHODCALLTYPE CompareIDs(
        LPARAM lParam, LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2);
    HRESULT STDMETHODCALLTYPE CreateViewObject(
        HWND, REFIID riid, void** ppv);
    HRESULT STDMETHODCALLTYPE GetAttributesOf(
        UINT cidl, LPCITEMIDLIST* apidl, SFGAOF* rgfInOut);
    HRESULT STDMETHODCALLTYPE GetUIObjectOf(
        HWND hwndOwner, UINT cidl, LPCITEMIDLIST* apidl,
        REFIID riid, UINT* rgfReserved, void** ppv);
    HRESULT STDMETHODCALLTYPE GetDisplayNameOf(
        LPCITEMIDLIST pidl, SHGDNF uFlags, STRRET* pName);
    HRESULT STDMETHODCALLTYPE SetNameOf(
        HWND hwnd, LPCITEMIDLIST pidl, LPCOLESTR pszName,
        SHGDNF uFlags, LPITEMIDLIST* ppidlOut);

    // IShellFolder2
    HRESULT STDMETHODCALLTYPE GetDefaultSearchGUID(GUID*);
    HRESULT STDMETHODCALLTYPE EnumSearches(IEnumExtraSearch** ppEnum);
    HRESULT STDMETHODCALLTYPE GetDefaultColumn(
        DWORD, ULONG* pSort, ULONG* pDisplay);
    HRESULT STDMETHODCALLTYPE GetDefaultColumnState(
        UINT iColumn, SHCOLSTATEF* pcsFlags);
    HRESULT STDMETHODCALLTYPE GetDetailsEx(
        LPCITEMIDLIST pidl, const SHCOLUMNID* pscid, VARIANT* pv);
    HRESULT STDMETHODCALLTYPE GetDetailsOf(
        LPCITEMIDLIST pidl, UINT iColumn, SHELLDETAILS* psd);
    HRESULT STDMETHODCALLTYPE MapColumnToSCID(
        UINT iColumn, SHCOLUMNID* pscid);

    // IPersist
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* pClassID);

    // IPersistFolder
    HRESULT STDMETHODCALLTYPE Initialize(LPCITEMIDLIST pidl);

    // IPersistFolder2
    HRESULT STDMETHODCALLTYPE GetCurFolder(LPITEMIDLIST* ppidl);

    // IPersistFolder3
    HRESULT STDMETHODCALLTYPE InitializeEx(
        IBindCtx* pbc, LPCITEMIDLIST pidlRoot,
        const PERSIST_FOLDER_TARGET_INFO* ppfti);
    HRESULT STDMETHODCALLTYPE GetFolderTargetInfo(
        PERSIST_FOLDER_TARGET_INFO* ppfti);

    // IShellFolderViewCB
    HRESULT STDMETHODCALLTYPE MessageSFVCB(
        UINT uMsg, WPARAM wParam, LPARAM lParam);

    // IThumbnailHandlerFactory
    HRESULT WINAPI GetThumbnailHandler(
        PCUITEMID_CHILD pidlChild, IBindCtx* pbc, REFIID riid, void** ppv);

    // Inherited via IDropTarget
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragLeave(void) override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;


    std::filesystem::path GetTempDir();
    std::vector<std::shared_ptr<TempDiskFile>> tempFileRefs; //#TODO private
    void KeepTempFileRef(const std::shared_ptr<TempDiskFile>& shared) {
        tempFileRefs.emplace_back(shared);
    }

    };