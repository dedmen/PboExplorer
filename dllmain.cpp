#include <Windows.h>
#include <ShlObj.h>
#include <OleCtl.h>


#pragma data_seg(".text")
#define INITGUID
#include <initguid.h>
#include <ShlGuid.h>
#include <string>

#include "ClassFactory.hpp"
#include "FileWatcher.hpp"
#include "guid.hpp"
#include "PboFolder.hpp"
#pragma data_seg()

// data
HINSTANCE   g_hInst;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = hModule;
		GFileWatcher.Startup();
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

/*---------------------------------------------------------------*/
// DllCanUnloadNow()
/*---------------------------------------------------------------*/
STDAPI DllCanUnloadNow(VOID)
{
	return (g_DllRefCount ? S_FALSE : S_OK);
}

/*---------------------------------------------------------------*/
// DllGetClassObject()
/*---------------------------------------------------------------*/
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppReturn)
{
	//while (!IsDebuggerPresent())
	//	Sleep(10);

	//__debugbreak();

	*ppReturn = nullptr;

	// do we support the CLSID?
	if (!IsEqualCLSID(rclsid, CLSID_PboExplorer))
		return CLASS_E_CLASSNOTAVAILABLE;


	if (IsEqualIID(riid, IID_IShellFolder2) || IsEqualIID(riid, IID_IShellFolder))
	{
		*ppReturn = static_cast<IShellFolder2*>(new PboFolder());
	}
		

	if (!IsEqualIID(riid, IID_IClassFactory))
		__debugbreak();
	// riid == IID_IClassFactory
	
	// call the factory
	auto pClassFactory = ComRef<ClassFactory>::Create();
	if (!pClassFactory)
		return E_OUTOFMEMORY;

	// query interface for the a pointer
	HRESULT hResult = pClassFactory->QueryInterface(riid, ppReturn);
	return hResult;
}


/*---------------------------------------------------------------*/
// DllGetRegisterServer()
/*---------------------------------------------------------------*/

typedef struct {
	HKEY  hRootKey;
	std::wstring lpszSubKey;
	std::wstring lpszValueName;
	std::wstring lpszData;
}REGSTRUCT, * LPREGSTRUCT;

STDAPI DllRegisterServer(VOID)
{
	//#TODO needs to be admin
	//while (!IsDebuggerPresent())
	//	Sleep(10);
	//
	//__debugbreak();
	

	HKEY hKey;
	LRESULT lResult;
	DWORD dwDisp;
	TCHAR szSubKey[MAX_PATH];
	
	//TCHAR szCLSID[MAX_PATH];
	TCHAR szModule[MAX_PATH];
	LPWSTR pwsz;

	// get the CLSID in string form
	StringFromIID(CLSID_PboExplorer, &pwsz);
	
	std::wstring szCLSID(pwsz);

	if (pwsz)
	{
		//WideCharToLocal(szCLSID, pwsz, ARRAYSIZE(szCLSID));
		LPMALLOC pMalloc;
		CoGetMalloc(1, &pMalloc);
		if (pMalloc)
		{
			pMalloc->Free(pwsz);
			pMalloc->Release();
		}
	}

	// get this DLL's path and file name
	GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule));

	// CLSID entries
	REGSTRUCT ClsidEntries[] = {
		HKEY_CLASSES_ROOT, L"CLSID\\%s",                  L"",                   L"PboExplorer",
		HKEY_CLASSES_ROOT, L"CLSID\\%s",                  L"InfoTip",        L"PboExplorer.",
		HKEY_CLASSES_ROOT, L"CLSID\\%s\\InprocServer32",  L"",                   L"%s",
		HKEY_CLASSES_ROOT, L"CLSID\\%s\\InprocServer32",  L"ThreadingModel", L"Apartment",
		HKEY_CLASSES_ROOT, L"CLSID\\%s\\DefaultIcon",     L"",                   L"%s,0",
		HKEY_CLASSES_ROOT, L"PboExplorer",     L"",                   L"PboExplorer",
		HKEY_CLASSES_ROOT, L"PboExplorer\\CLSID",     L"",                   szCLSID,
		HKEY_CLASSES_ROOT, L"PboExplorer\\DefaultIcon",     L"",                   L"%s,0",
		//HKEY_CLASSES_ROOT, L"PboExplorer\\shell\\open\\command",     L"DelegateExecute",                   szCLSID,
		HKEY_CLASSES_ROOT, L"PboExplorer\\shell",     L"",                   L"open",
		HKEY_CLASSES_ROOT, L"PboExplorer\\shellex\\PropertySheetHandlers\\%s",     L"",                   L"",


	};

	for (auto& it : ClsidEntries)
	{
		// create the sub key string.
		wsprintf(szSubKey, it.lpszSubKey.data(), szCLSID.data());
		lResult = RegCreateKeyEx(it.hRootKey,
			szSubKey,
			0,
			nullptr,
			REG_OPTION_NON_VOLATILE,
			KEY_WRITE,
			nullptr,
			&hKey,
			&dwDisp);


		//E_ACCESSDENIED non admin
		if (lResult == NOERROR)
		{
			TCHAR szData[MAX_PATH];
			wsprintf(szData, it.lpszData.data(), szModule);
			lResult = RegSetValueEx(hKey,
				it.lpszValueName.data(),
				0,
				REG_SZ,
				(LPBYTE)szData,
				wcslen(szData)*2 + 2);

			RegCloseKey(hKey);
		}
		else
			return SELFREG_E_CLASS;
	}


	// dunno what this is
	wsprintf(szSubKey, L"CLSID\\%s\\ShellFolder", szCLSID.data());

	if (RegCreateKeyExW(HKEY_CLASSES_ROOT, szSubKey, 0, nullptr,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr))
		return(E_ACCESSDENIED);

	DWORD attr = SFGAO_FOLDER | SFGAO_FILESYSANCESTOR;
	RegSetValueExW(hKey, L"Attributes", 0, REG_DWORD,
		(BYTE*)&attr, (DWORD)sizeof(DWORD));

	RegCloseKey(hKey);



	// https://stackoverflow.com/q/21239003 Update 2
	wsprintf(szSubKey, L"CLSID\\%s\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}", szCLSID.data());

	if (RegCreateKeyExW(HKEY_CLASSES_ROOT, szSubKey, 0, nullptr,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr))
		return(E_ACCESSDENIED);
	RegCloseKey(hKey);

	
	// Context Menu
	lstrcpy(szSubKey, TEXT("Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer"));
	lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT,
		szSubKey,
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		nullptr,
		&hKey,
		&dwDisp);

	if (lResult == NOERROR)
	{
		lResult = RegSetValueEx(hKey,
			nullptr,
			0,
			REG_SZ,
			(LPBYTE)szCLSID.data(),
			szCLSID.length()*2 + 2);

		RegCloseKey(hKey);
	}
	else
		return SELFREG_E_CLASS;


	// file extension assoc
	lstrcpy(szSubKey, TEXT(".pbox"));
	lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT,
		szSubKey,
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		nullptr,
		&hKey,
		&dwDisp);

	if (lResult == NOERROR)
	{
		lResult = RegSetValueEx(hKey,
			nullptr,
			0,
			REG_SZ,
			(LPBYTE)L"PboExplorer",
			wcslen(L"PboExplorer") * 2 + 2); //#TODO fix other lengths too

		RegCloseKey(hKey);
	}
	else
		return SELFREG_E_CLASS;

	

	// file extension assoc
	lstrcpy(szSubKey, TEXT("SystemFileAssociations\\.pbox\\CLSID"));
	lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT,
		szSubKey,
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		nullptr,
		&hKey,
		&dwDisp);

	if (lResult == NOERROR)
	{
		lResult = RegSetValueEx(hKey,
			nullptr,
			0,
			REG_SZ,
			(LPBYTE)szCLSID.data(),
			szCLSID.length()*2 + 2); //#TODO fix other lengths too

		RegCloseKey(hKey);
	}
	else
		return SELFREG_E_CLASS;







	
	

	// register the extension as approved by NT
	//OSVERSIONINFO  osvi;
	//osvi.dwOSVersionInfoSize = sizeof(osvi);
	//GetVersionEx(&osvi);
	//
	//if (VER_PLATFORM_WIN32_NT == osvi.dwPlatformId)
	//{
	//	lstrcpy(szSubKey, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"));
	//	lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
	//		szSubKey,
	//		0,
	//		NULL,
	//		REG_OPTION_NON_VOLATILE,
	//		KEY_WRITE,
	//		NULL,
	//		&hKey,
	//		&dwDisp);
	//
	//	if (lResult == NOERROR)
	//	{
	//		TCHAR szData[MAX_PATH];
	//		lstrcpy(szData, TEXT("PboExplorer"));
	//		lResult = RegSetValueEx(hKey,
	//			szCLSID.data(),
	//			0,
	//			REG_SZ,
	//			(LPBYTE)szData,
	//			lstrlen(szData) + 1);
	//
	//		RegCloseKey(hKey);
	//	}
	//	else
	//		return SELFREG_E_CLASS;
	//}



	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);


	return S_OK;
}

/*---------------------------------------------------------------*/
// DllUnregisterServer()
/*---------------------------------------------------------------*/
STDAPI DllUnregisterServer(VOID)
{
	//	while (!IsDebuggerPresent())
	//	Sleep(10);
	//
	//__debugbreak();
	
	
	
	//HKEY hKey;
	LRESULT lResult;
	//DWORD dwDisp;
	TCHAR szSubKey[MAX_PATH];
	//TCHAR szCLSID[MAX_PATH];
	TCHAR szModule[MAX_PATH];
	LPWSTR pwsz;

	// get the CLSID in string form
	StringFromIID(CLSID_PboExplorer, &pwsz);
	std::wstring szCLSID(pwsz);

	if (pwsz)
	{
		//WideCharToLocal(szCLSID, pwsz, ARRAYSIZE(szCLSID));
		LPMALLOC pMalloc;
		CoGetMalloc(1, &pMalloc);
		if (pMalloc)
		{
			pMalloc->Free(pwsz);
			pMalloc->Release();
		}
	}

	// get this DLL's path and file name
	GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule));

	// CLSID entries
	REGSTRUCT ClsidEntries[] = {
		HKEY_CLASSES_ROOT, L"CLSID\\%s",                  L"",                   L"PboExplorer",
		HKEY_CLASSES_ROOT, L"CLSID\\%s",                  L"InfoTip",        L"PboExplorer.",
		HKEY_CLASSES_ROOT, L"CLSID\\%s\\InprocServer32",  L"",                   L"%s",
		HKEY_CLASSES_ROOT, L"CLSID\\%s\\InprocServer32",  L"ThreadingModel", L"Apartment",
		HKEY_CLASSES_ROOT, L"CLSID\\%s\\DefaultIcon",     L"",                   L"%s,0"};

	for (auto& it : ClsidEntries)
	{
		// create the sub key string.
		wsprintf(szSubKey, it.lpszSubKey.data(), szCLSID.data());
		lResult = RegDeleteKey(it.hRootKey, szSubKey);
	}


	// Context Menu
	lstrcpy(szSubKey, TEXT("Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer"));
	lResult = RegDeleteKey(HKEY_CLASSES_ROOT, szSubKey);


	// file extension assoc
	lstrcpy(szSubKey, TEXT("SystemFileAssociations\\.pbox"));
	lResult = RegDeleteKey(HKEY_CLASSES_ROOT, szSubKey);

	// file extension assoc
	lstrcpy(szSubKey, TEXT(".pbox"));
	lResult = RegDeleteKey(HKEY_CLASSES_ROOT, szSubKey);
	
	// register the extension as approved by NT
	//OSVERSIONINFO  osvi;
	//osvi.dwOSVersionInfoSize = sizeof(osvi);
	//GetVersionEx(&osvi);
	//
	//if (VER_PLATFORM_WIN32_NT == osvi.dwPlatformId)
	//{
	//	lResult = RegDeleteKey(HKEY_LOCAL_MACHINE, szSubKey);
	//}

	return S_OK;
}