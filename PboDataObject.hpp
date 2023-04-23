#pragma once
#define NOMINMAX
#include <shlobj.h>
import ComRef;
#include <memory>
#include "PboFileDirectory.hpp"

class PboFolder;
class TempDiskFile;

class PboDataObject :
	GlobalRefCounted,
	public RefCountedCOM<PboDataObject,
			      IDataObject,
                  IPersistStream
	>
    //,public IAsyncOperation
{
public:
    PboDataObject(void);
    PboDataObject(std::shared_ptr<PboSubFolder> rootfolder, PboFolder* mainFolder, LPCITEMIDLIST pidl, UINT cidl, LPCITEMIDLIST* apidl);
    ~PboDataObject() override;


    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    // IDataObject
    HRESULT STDMETHODCALLTYPE GetData(
        FORMATETC* pformatetc, STGMEDIUM* pmedium) override;
    HRESULT STDMETHODCALLTYPE GetDataHere(
        FORMATETC* pformatetc, STGMEDIUM* pmedium) override;
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformatetc) override;
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(
        FORMATETC* pformatetcIn, FORMATETC* pformatetcOut) override;
    HRESULT STDMETHODCALLTYPE SetData(
        FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) override;
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(
        DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override;
    HRESULT STDMETHODCALLTYPE DAdvise(
        FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink,
        DWORD* pdwConnection) override;
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection) override;
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA** ppenumAdvise) override;

    // IPersist
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* pClassID) override;

    // IPersistStream
    HRESULT STDMETHODCALLTYPE IsDirty(void) override;
    HRESULT STDMETHODCALLTYPE Load(IStream* pStm) override;
    HRESULT STDMETHODCALLTYPE Save(IStream* pStm, BOOL fClearDirty) override;
    HRESULT STDMETHODCALLTYPE GetSizeMax(ULARGE_INTEGER* pcbSize) override;

    // IAsyncOperation
    HRESULT STDMETHODCALLTYPE SetAsyncMode(BOOL fDoOpAsync);
    HRESULT STDMETHODCALLTYPE GetAsyncMode(BOOL* pfIsOpAsync);
    HRESULT STDMETHODCALLTYPE StartOperation(IBindCtx* pbcReserved);
    HRESULT STDMETHODCALLTYPE InOperation(BOOL* pfInAsyncOp);
    HRESULT STDMETHODCALLTYPE EndOperation(
        HRESULT hResult, IBindCtx* pbcReserved, DWORD dwEffects);

private:
 
    //void initLists(void);
    //void addFilesToLists(Dir* dir, const wchar_t* offset);


    std::shared_ptr<PboSubFolder> rootFolder;
    std::shared_ptr<IPboFolder> pboFile;

    BOOL m_asyncMode;
    BOOL m_inAsyncOperation;
    CoTaskMemRefS<ITEMIDLIST> m_pidlRoot;
    std::vector<CoTaskMemRefS<ITEMIDLIST>> m_apidl;
    bool m_listsInit;
    bool m_postQuit;

    uint8_t currentDropMode = 0; // DROPEFFECT_NONE

    // in case someone specifically requests hdrop
    std::shared_ptr<TempDiskFile> hdrop_tempfile;
};
