#pragma once
#include <ShlObj.h>
#include <Windows.h>
#include "guid.hpp"

#include "lib/ArmaPboLib/src/pbo.hpp"
#include <filesystem>

#include "ComRef.hpp"


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
};


class PboFile
{
public:
    PboFile();
    std::filesystem::path diskPath;
    std::shared_ptr<PboSubFolder> rootFolder;
    std::filesystem::path selfRelPath;
	
    void ReadFrom(std::filesystem::path inputPath);
    PboSubFile& GetFileByPath(std::filesystem::path inputPath) const;
    std::shared_ptr<PboSubFolder> GetFolderByPath(std::filesystem::path inputPath) const;
};






class PboFolder:
	GlobalRefCounted,
	public RefCountedCOM<PboFolder,
	                     IShellFolder2,
	                     IPersistFolder3,
	                     IShellFolderViewCB,
	                     IThumbnailHandlerFactory
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



    std::filesystem::path GetTempDir();
};