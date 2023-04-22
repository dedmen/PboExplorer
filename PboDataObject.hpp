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
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    // IDataObject
    HRESULT STDMETHODCALLTYPE GetData(
        FORMATETC* pformatetc, STGMEDIUM* pmedium);
    HRESULT STDMETHODCALLTYPE GetDataHere(
        FORMATETC* pformatetc, STGMEDIUM* pmedium);
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformatetc);
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(
        FORMATETC* pformatetcIn, FORMATETC* pformatetcOut);
    HRESULT STDMETHODCALLTYPE SetData(
        FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease);
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(
        DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc);
    HRESULT STDMETHODCALLTYPE DAdvise(
        FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink,
        DWORD* pdwConnection);
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection);
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA** ppenumAdvise);

    // IPersist
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* pClassID);

    // IPersistStream
    HRESULT STDMETHODCALLTYPE IsDirty(void);
    HRESULT STDMETHODCALLTYPE Load(IStream* pStm);
    HRESULT STDMETHODCALLTYPE Save(IStream* pStm, BOOL fClearDirty);
    HRESULT STDMETHODCALLTYPE GetSizeMax(ULARGE_INTEGER* pcbSize);

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
