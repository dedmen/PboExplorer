module;
#include <windows.h>
#include <shlobj.h>
#include <thumbcache.h>

#include "PboFolder.hpp"
#include "PboDataObject.hpp"
#include "ShellExt.hpp"

#include "DebugLogger.hpp"


export module ClassFactory;


import ComRef;
import Tracy;
import PaaThumbnailProvider;

extern HINSTANCE  g_hInst;

export class ClassFactory :
	GlobalRefCounted,
	public RefCountedCOM<ClassFactory, IClassFactory>
{
public:
	ClassFactory()
	{
	}

    ~ClassFactory() override {
	}

	//IUnknown methods
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		ProfilingScope pScope;
		pScope.SetValue(DebugLogger::GetGUIDName(riid).first);

		DebugLogger_OnQueryInterfaceEntry(riid);
		*ppvObject = nullptr;

		if (IsEqualIID(riid, IID_IUnknown))
			*ppvObject = this;
		else if (IsEqualIID(riid, IID_IClassFactory))
			*ppvObject = (IClassFactory*)this;
		else
			__debugbreak();


		if (*ppvObject)
		{
			LPUNKNOWN pUnk = (LPUNKNOWN)(*ppvObject);
			pUnk->AddRef();
			return S_OK;
		}

		DebugLogger_OnQueryInterfaceExitUnhandled(riid);

		return E_NOINTERFACE;
	}

	//IClassFactory methods

	HRESULT STDMETHODCALLTYPE CreateInstance(
		IUnknown* pUnkOuter,
		REFIID riid,
		void** ppvObject) override
	{
		ProfilingScope pScope;
		pScope.SetValue(DebugLogger::GetGUIDName(riid).first);

		*ppvObject = nullptr;
		if (pUnkOuter)
			return CLASS_E_NOAGGREGATION;

		if (IsEqualIID(riid, CATID_BrowsableShellExt) || IsEqualIID(riid, IID_IShellFolder) || IsEqualIID(riid, IID_IShellFolder2))
		{
			ComRef<PboFolder>::CreateForReturn<IShellFolder2>(ppvObject);
			return S_OK;
		}

		// DataObject loaded from IPersistStream
		if (IsEqualIID(riid, IID_IDataObject))
		{
			ComRef<PboDataObject>::CreateForReturn<IDataObject>(ppvObject);
			return S_OK;
		}

		if (IsEqualIID(riid, IID_IThumbnailProvider))
		{
			ComRef<PaaThumbnailProvider>::CreateForReturn<IThumbnailProvider>(ppvObject);
			return S_OK;
		}

		if (!IsEqualIID(riid, IID_IUnknown) && !IsEqualIID(riid, IID_IContextMenu) && !IsEqualIID(riid, IID_IShellPropSheetExt))
			__debugbreak();


		// creates the namespace's main class
		auto pShellExt = ComRef<ShellExt>::Create();
		if (!pShellExt)
			return E_OUTOFMEMORY;

		// query interface for the return value
		HRESULT hResult = pShellExt->QueryInterface(riid, ppvObject);
		return hResult;
	}

    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override {
		return E_NOTIMPL;
	}
};