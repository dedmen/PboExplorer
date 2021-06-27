#pragma once

#include <windows.h>
#include <shlobj.h>

#include "ComRef.hpp"
#include "PboFolder.hpp"
#include "ShellExt.hpp"

#include "DebugLogger.hpp"

extern HINSTANCE  g_hInst;

class ClassFactory :
	GlobalRefCounted,
	public RefCountedCOM<ClassFactory, IClassFactory>
{
public:
	ClassFactory()
	{
	}
	virtual ~ClassFactory()
	{
	}

	//IUnknown methods
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
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
		*ppvObject = nullptr;
		if (pUnkOuter)
			return CLASS_E_NOAGGREGATION;

		if (IsEqualIID(riid, CATID_BrowsableShellExt) || IsEqualIID(riid, IID_IShellFolder) || IsEqualIID(riid, IID_IShellFolder2))
		{
			ComRef<PboFolder>::CreateForReturn<IShellFolder2>(ppvObject);
			return S_OK;
		}
		
		

		if (!IsEqualIID(riid, IID_IUnknown) && !IsEqualIID(riid, IID_IContextMenu) && !IsEqualIID(riid, IID_IShellPropSheetExt))
			__debugbreak();




		
		// creates the namespace's main class
		ComRef<ShellExt> pShellExt = new ShellExt();
		if (!pShellExt)
			return E_OUTOFMEMORY;

		// query interface for the return value
		HRESULT hResult = pShellExt->QueryInterface(riid, ppvObject);
		return hResult;
	}

	virtual  HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) {
		return E_NOTIMPL;
	}
};