#pragma once
#define NOMINMAX
#include <ShlObj.h>
#include <Windows.h>
#include <filesystem>

#include "ComRef.hpp"
#include "PboPidl.hpp"
#include "TempDiskFile.hpp"

#include "PboFileDirectory.hpp"

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
                         //,IShellItem //#TODO finish this? Maybe?
	>
{
    bool checkInit(void);

    CoTaskMemRefS<ITEMIDLIST> m_pidl;

    std::filesystem::path m_tempDir;
    IShellBrowser* m_shellBrowser;
    HWND lastHwnd;
public:

    std::shared_ptr<IPboFolder> pboFile;
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


    // IShellItem
    virtual HRESULT STDMETHODCALLTYPE BindToHandler(
        /* [unique][in] */ __RPC__in_opt IBindCtx* pbc,
        /* [in] */ __RPC__in REFGUID bhid,
        /* [in] */ __RPC__in REFIID riid,
        /* [iid_is][out] */ __RPC__deref_out_opt void** ppv);

    virtual HRESULT STDMETHODCALLTYPE GetParent(
        /* [out] */ __RPC__deref_out_opt IShellItem** ppsi);

    virtual HRESULT STDMETHODCALLTYPE GetDisplayName(
        /* [in] */ SIGDN sigdnName,
        /* [annotation][string][out] */
        _Outptr_result_nullonfailure_  LPWSTR* ppszName);

    virtual HRESULT STDMETHODCALLTYPE GetAttributes(
        /* [in] */ SFGAOF sfgaoMask,
        /* [out] */ __RPC__out SFGAOF* psfgaoAttribs);

    virtual HRESULT STDMETHODCALLTYPE Compare(
        /* [in] */ __RPC__in_opt IShellItem* psi,
        /* [in] */ SICHINTF hint,
        /* [out] */ __RPC__out int* piOrder);



    std::filesystem::path GetTempDir();
    std::vector<std::shared_ptr<TempDiskFile>> tempFileRefs; //#TODO private
    void KeepTempFileRef(const std::shared_ptr<TempDiskFile>& shared) {
        tempFileRefs.emplace_back(shared);
    }

    //#TODO move to seperate IDropTarget class
    DWORD dropEffect;

};