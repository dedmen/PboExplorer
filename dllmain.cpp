#include <Windows.h>
#include <ShlObj.h>
#include <OleCtl.h>
#include <string>
#include <variant>


#pragma data_seg(".text")
#define INITGUID
#include <initguid.h>
#include <ShlGuid.h>
#include "guid.hpp"
#pragma data_seg()



#include "ClassFactory.hpp"
#include "FileWatcher.hpp"
#include "PboFolder.hpp"
#include "Util.hpp"




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

		wchar_t szModule[MAX_PATH];
		GetModuleFileNameW(nullptr, szModule, ARRAYSIZE(szModule));
		// don't load FileWatcher in regsvr
		if (std::wstring_view(szModule).find(L"regsvr") == std::string::npos)
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


class RegistryEntry {
	// Delete is handled by someone else
	bool ignoreDelete = false;
	// directory == key or value
	bool isDirectory = false;
	HKEY rootKey;
	std::wstring subKeyFormat;
	std::wstring valueNameFormat;
	std::variant<std::wstring, DWORD> data;

public:

	RegistryEntry(HKEY rootKey,
		std::wstring subKeyFormat,
		std::wstring valueNameFormat,
		std::wstring dataFormat,
		bool ignoreDelete = true
		) : rootKey(rootKey), subKeyFormat(subKeyFormat), valueNameFormat(valueNameFormat), data(dataFormat), ignoreDelete(ignoreDelete), isDirectory(false) {}

	RegistryEntry(HKEY rootKey,
		std::wstring subKeyFormat,
		std::wstring valueNameFormat,
		DWORD data,
		bool ignoreDelete = true
	) : rootKey(rootKey), subKeyFormat(subKeyFormat), valueNameFormat(valueNameFormat), data(data), ignoreDelete(ignoreDelete), isDirectory(false) {}

	RegistryEntry(HKEY rootKey,
		std::wstring subKeyFormat,
		bool ignoreDelete = false
	) : rootKey(rootKey), subKeyFormat(subKeyFormat), ignoreDelete(ignoreDelete), isDirectory(true) {}



	// Context for formatting, used as arguments for key/value/data formats
	struct RegistryContext {
		// {0} = "{710E6680-2905-48CC-A6F5-FFFA6FB6EB41}" - our registry CLSID
		std::wstring pboExplorerCLSID;
		// {1} = "O:\dev\pbofolder\PboExplorer\x64\Debug\PboExplorer.dll" - Full path to dll module path
		std::wstring modulePath;
	};


	HRESULT DoCreate(const RegistryContext& context) {
		std::wstring fullPath = std::format(subKeyFormat, context.pboExplorerCLSID, context.modulePath);

		HKEY hKey;
		DWORD dwDisp;

		auto lResult = RegCreateKeyEx(rootKey,
			fullPath.c_str(),
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
			// directory has no values
			if (isDirectory) return S_OK;


			std::wstring fullValueName = std::format(valueNameFormat, context.pboExplorerCLSID, context.modulePath);

			std::visit([&](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, std::wstring>) {
					std::wstring fullData = std::format(arg, context.pboExplorerCLSID, context.modulePath);

					lResult = RegSetValueExW(hKey,
						fullValueName.empty() ? nullptr : fullValueName.c_str(),
						0,
						REG_SZ,
						(LPBYTE)fullData.c_str(),
						(DWORD)fullData.length() * 2 + 2);
				}
				else if constexpr (std::is_same_v<T, DWORD>) {
					lResult = RegSetValueExW(hKey,
						fullValueName.empty() ? nullptr : fullValueName.c_str(),
						0,
						REG_DWORD,
						(BYTE*)&arg,
						(DWORD)sizeof(DWORD));
				}
				else
					static_assert(always_false_v<T>, "non-exhaustive visitor!");
				}, data);

			RegCloseKey(hKey);
		}
		else
			return lResult;

		return S_OK;
	}


	HRESULT DoDelete(const RegistryContext& context) {
		if (ignoreDelete) return S_OK;

		// if we own this directory, delete its whole tree
		if (isDirectory) {
			std::wstring fullPath = std::format(subKeyFormat, context.pboExplorerCLSID, context.modulePath);
			return RegDeleteTreeW(rootKey, fullPath.c_str());
		} else {
			// delete single key
			std::wstring fullPath = std::format(subKeyFormat, context.pboExplorerCLSID, context.modulePath);
			return RegDeleteKeyW(rootKey, fullPath.c_str());
		}
	}

	bool operator==(const RegistryEntry& other) const {
		return
			isDirectory == other.isDirectory &&
			ignoreDelete == other.ignoreDelete &&
			subKeyFormat == other.subKeyFormat &&
			valueNameFormat == other.valueNameFormat &&
			rootKey == other.rootKey;
	}
};

[[noreturn]] void RestartAsAdmin() {


	MessageBoxA(GetDesktopWindow(), "PboExplorer needs Admin rights to register itself into the Windows Explorer, I will now prompt you for that.", "PboExplorer", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);


	// get current process path
	wchar_t szModule[MAX_PATH];
	GetModuleFileNameW(nullptr, szModule, ARRAYSIZE(szModule));

	auto commandLine = GetCommandLineW();
	int nArgs = 0;
	auto args = CommandLineToArgvW(commandLine, &nArgs);

	if (nArgs >= 2)
		commandLine = commandLine + std::wstring_view(commandLine).find(args[1]);

	LocalFree(args);

	ShellExecuteW(
		GetDesktopWindow(), 
		L"runas", 
		szModule,
		commandLine,
		NULL,
		SW_SHOW);

	exit(0);
}



/*---------------------------------------------------------------*/
// DllGetRegisterServer()
/*---------------------------------------------------------------*/
STDAPI DllRegisterServer(VOID)
{
	//Util::WaitForDebuggerPrompt();

	std::vector<RegistryEntry> registryEntries{
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}",																						false},
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}",											L"",					L"PboExplorer",		true},
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}",											L"InfoTip",				L"PboExplorer.",	true},
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\InprocServer32",							L"",					L"{1}",				true},
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\InprocServer32",							L"ThreadingModel",		L"Apartment",		true},
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\DefaultIcon",								L"",					L"{1},0",			true},
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\ShellFolder",								L"Attributes",			DWORD(SFGAO_FOLDER | SFGAO_FILESYSANCESTOR),			true},
		// https://stackoverflow.com/q/21239003 Update 2 - CATID_BrowsableShellExt
		{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\Implemented Categories\\{{00021490-0000-0000-C000-000000000046}}",						false},

		{HKEY_CLASSES_ROOT, L"PboExplorer",																						false},
		{HKEY_CLASSES_ROOT, L"PboExplorer",											L"",					L"PboExplorer",		true},
		{HKEY_CLASSES_ROOT, L"PboExplorer\\CLSID",									L"",					L"{0}",				true},
		{HKEY_CLASSES_ROOT, L"PboExplorer\\DefaultIcon",							L"",					L"{1},0",			true},
		//{HKEY_CLASSES_ROOT, L"PboExplorer\\shell\\open\\command",					L"DelegateExecute",     L"{0}"},
		{HKEY_CLASSES_ROOT, L"PboExplorer\\shell",									L"",					L"open",			true},
		{HKEY_CLASSES_ROOT, L"PboExplorer\\shellex\\PropertySheetHandlers\\{0}",    L"",                    L"",				true},

		// Context Menu on folders (create PBO from folder)
		{HKEY_CLASSES_ROOT, L"Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer",												true},
		{HKEY_CLASSES_ROOT, L"Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer",	L"",					L"{0}",				true},

		// .pbo file format general, association, its handled by PboExplorer key
		{HKEY_CLASSES_ROOT, L".pbox",																							true},
		{HKEY_CLASSES_ROOT, L".pbox",												L"",					L"PboExplorer",		true}, // reference to HKEY_CLASSES_ROOT/PboExplorer key

		// Same thing again? just for CLSID? Dunno.
		{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbox",																	true},
		{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbox\\CLSID",				L"",					L"{0}",				true}
	};

	// register the extension as approved by NT
	//OSVERSIONINFO  osvi;
	//osvi.dwOSVersionInfoSize = sizeof(osvi);
	//GetVersionEx(&osvi);
	//
	//if (VER_PLATFORM_WIN32_NT == osvi.dwPlatformId)
	//{
	//	registryEntries.emplace_back(
	//		HKEY_LOCAL_MACHINE, 
	//		L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
	//		L"{0}",
	//		L"PboExplorer",
	//		true		
	//	);
	//}

	RegistryEntry::RegistryContext regContext;

	{
		// get the CLSID in string form
		LPWSTR pwsz;
		StringFromIID(CLSID_PboExplorer, &pwsz);
		std::wstring szCLSID(pwsz);


		// get this DLL's path and file name
		TCHAR szModule[MAX_PATH];
		GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule));

		regContext = {
			szCLSID,
			std::wstring(szModule)
		};


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
	}

	for (auto& it : registryEntries) {
		auto result = it.DoCreate(regContext);
		if (result != S_OK) {
			Util::TryDebugBreak();

			if (HRESULT_CODE(result) == ERROR_ACCESS_DENIED) {
				// We aren't admin, ask the user to run us as Admin and try again

				RestartAsAdmin();
				return result;
			}

			// undo the things we've already done
			for (auto& undoIt : registryEntries) {
				if (undoIt == it) break;
				undoIt.DoDelete(regContext);
			}
			
			return result;
		}
	}




	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);


	return S_OK;
}

/*---------------------------------------------------------------*/
// DllUnregisterServer()
/*---------------------------------------------------------------*/
STDAPI DllUnregisterServer(VOID)
{
	//Util::WaitForDebuggerPrompt();


	std::vector<RegistryEntry> registryEntries{
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}",																						false},
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}",											L"",					L"PboExplorer",		true},
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}",											L"InfoTip",				L"PboExplorer.",	true},
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\InprocServer32",							L"",					L"{1}",				true},
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\InprocServer32",							L"ThreadingModel",		L"Apartment",		true},
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\DefaultIcon",								L"",					L"{1},0",			true},
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\ShellFolder",								L"Attributes",			DWORD(SFGAO_FOLDER | SFGAO_FILESYSANCESTOR),			true},
	// https://stackoverflow.com/q/21239003 Update 2 - CATID_BrowsableShellExt
	{HKEY_CLASSES_ROOT, L"CLSID\\{0}\\Implemented Categories\\{{00021490-0000-0000-C000-000000000046}}",					true},

	{HKEY_CLASSES_ROOT, L"PboExplorer",																						false},
	{HKEY_CLASSES_ROOT, L"PboExplorer",											L"",					L"PboExplorer",		true},
	{HKEY_CLASSES_ROOT, L"PboExplorer\\CLSID",									L"",					L"{0}",				true},
	{HKEY_CLASSES_ROOT, L"PboExplorer\\DefaultIcon",							L"",					L"{1},0",			true},
	//{HKEY_CLASSES_ROOT, L"PboExplorer\\shell\\open\\command",					L"DelegateExecute",     L"{0}"},
	{HKEY_CLASSES_ROOT, L"PboExplorer\\shell",									L"",					L"open",			true},
	{HKEY_CLASSES_ROOT, L"PboExplorer\\shellex\\PropertySheetHandlers\\{0}",    L"",                    L"",				true},

	// Context Menu on folders (create PBO from folder)
	{HKEY_CLASSES_ROOT, L"Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer",												true},
	{HKEY_CLASSES_ROOT, L"Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer",	L"",					L"{0}",				true},

	// .pbo file format general, association, its handled by PboExplorer key
	{HKEY_CLASSES_ROOT, L".pbox",																							true},
	{HKEY_CLASSES_ROOT, L".pbox",												L"",					L"PboExplorer",		true}, // reference to HKEY_CLASSES_ROOT/PboExplorer key

	// Same thing again? just for CLSID? Dunno.
	{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbox",																	true},
	{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbox\\CLSID",				L"",					L"{0}",				true}
	};

	// register the extension as approved by NT
	//OSVERSIONINFO  osvi;
	//osvi.dwOSVersionInfoSize = sizeof(osvi);
	//GetVersionEx(&osvi);
	//
	//if (VER_PLATFORM_WIN32_NT == osvi.dwPlatformId)
	//{
	//	registryEntries.emplace_back(
	//		HKEY_LOCAL_MACHINE, 
	//		L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
	//		L"{0}",
	//		L"PboExplorer",
	//		true		
	//	);
	//}


	RegistryEntry::RegistryContext regContext;

	{
		// get the CLSID in string form
		LPWSTR pwsz;
		StringFromIID(CLSID_PboExplorer, &pwsz);
		std::wstring szCLSID(pwsz);


		// get this DLL's path and file name
		TCHAR szModule[MAX_PATH];
		GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule));

		regContext = {
			szCLSID,
			std::wstring(szModule)
		};


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
	}


	for (auto& it : registryEntries) {
		auto result = it.DoDelete(regContext);
		if (result != S_OK && result != ERROR_FILE_NOT_FOUND) {
			Util::TryDebugBreak();

			if (HRESULT_CODE(result) == ERROR_ACCESS_DENIED) {
				// We aren't admin, ask the user to run us as Admin and try again

				RestartAsAdmin();
			}
			return result;
		}
	}

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);


	return S_OK;
}