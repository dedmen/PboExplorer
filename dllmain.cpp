#include <Windows.h>
#include <ShlObj.h>
#include <OleCtl.h>

import <fstream>;
import <string>;
import <variant>;
#include <future>


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



#include "Updater.hpp"

import Registry;
import Sentry;
import Tracy;

// data
HINSTANCE   g_hInst;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
	case DLL_PROCESS_ATTACH: {
		g_hInst = hModule;
	// https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls
		wchar_t szModule[MAX_PATH];
		GetModuleFileNameW(nullptr, szModule, ARRAYSIZE(szModule));
		// don't load FileWatcher in regsvr
		if (std::wstring_view(szModule).find(L"regsvr") == std::string::npos)
			GFileWatcher.Startup();
	} break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
		break;
    case DLL_PROCESS_DETACH:
#if _DEBUG
		Util::WaitForDebuggerPrompt("DLL Detach", true);
#endif
#ifdef ENABLE_SENTRY
		Sentry::Close();
#endif
        break;
    }
    return TRUE;
}

/*---------------------------------------------------------------*/
// DllCanUnloadNow()
/*---------------------------------------------------------------*/
STDAPI DllCanUnloadNow(VOID)
{
	return (GlobalRefCounted::GetCurrentRefCount() ? S_FALSE : S_OK);
}

/*---------------------------------------------------------------*/
// DllGetClassObject()
/*---------------------------------------------------------------*/
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppReturn)
{
	ProfilingScope pScope;
	//Util::WaitForDebuggerPrompt();
#if ENABLE_SENTRY
	Sentry::Init(g_hInst);
#endif

	static bool FirstStartup = true;
	if (FirstStartup) {
		FirstStartup = false;
	
		std::thread([]() {
			Updater::OnStartup();
		}).detach();
	}

	

	*ppReturn = nullptr;

	// do we support the CLSID?
	if (!IsEqualCLSID(rclsid, CLSID_PboExplorer))
		return CLASS_E_CLASSNOTAVAILABLE;


	if (IsEqualIID(riid, IID_IShellFolder2) || IsEqualIID(riid, IID_IShellFolder))
	{
		Util::TryDebugBreak(); // should be done via ClassFactory
		//*ppReturn = static_cast<IShellFolder2*>(new PboFolder());
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


[[noreturn]] void RestartAsAdmin() {
	MessageBoxA(GetDesktopWindow(), "PboExplorer needs Admin rights to register itself into the Windows Explorer, I will now prompt you for that.", "PboExplorer", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);


	// get current process path
	wchar_t szModule[MAX_PATH];
	GetModuleFileNameW(nullptr, szModule, ARRAYSIZE(szModule));

	auto commandLine = GetCommandLineW();
	int nArgs = 0;
	auto args = CommandLineToArgvW(commandLine, &nArgs);

	if (nArgs >= 2) {
		commandLine = commandLine + std::wstring_view(commandLine).find(args[1]);
		// readd the quote if there was one
		if (*(commandLine - 1) == '"')
			--commandLine;
	}
		

	


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
	
	//#TODO .wav to .wss converter 
	/*
	Time I return to these sacred halls! 😄

There actually is an, in my humble opinion, way more effective way for quickly converting .wav to .wss files.

1. Press WIN + R, a small window should pop up. Enter shell:sendto and it should bring you to the "send to" folder.
Alternatively, you can navigate to it via: C:\Users\USERNAME\AppData\Roaming\Microsoft\Windows\SendTo

2. Put the .bat file attached to this post in the "send to" folder, right click on it and click on edit. There are two lines in which you have to enter/change the path to your WAVtoWSS.exe

3. Restart your PC. When selecting one or multiple .wav files anywhere, you should now be able to right-click and see "send to". Select this and choose "WAVtoWSS.bat". 
It will convert the selected files and create new .wss files in the same folder.

I did not write the bat file and unfortunately can't remember who did it back then. Literally more then 8 years ago... 
for %%i in (%*) do (
    echo %%i
    if /i "%%~xi" == ".wav" "C:\Program Files (x86)\Steam\steamapps\common\Arma 3 Tools\Audio\WAVToWSS.exe" %%i

REM  %var%

REM  && del %%i
    if /i "%%~xi" == ".wss" "C:\Program Files (x86)\Steam\steamapps\common\Arma 3 Tools\Audio\WAVToWSS.exe" %%i
Expand
WAVtoWSS.bat
1 KB
	*/
	

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

		// Context Menu on PBO's (unpack PBO)
		{HKEY_CLASSES_ROOT, L"PboExplorer\\ShellEx\\ContextMenuHandlers\\PboExplorer",											true},
		{HKEY_CLASSES_ROOT, L"PboExplorer\\ShellEx\\ContextMenuHandlers\\PboExplorer",L"",					L"{0}",				true},

		// Context Menu on PBO's (non pbox)
		//{HKEY_CLASSES_ROOT, L".pbo\\ShellEx\\ContextMenuHandlers\\PboExplorer",											true},
		//{HKEY_CLASSES_ROOT, L".pbo\\ShellEx\\ContextMenuHandlers\\PboExplorer",L"",					L"{0}",				true},
		//{HKEY_CLASSES_ROOT, L".pbo_auto_file\\ShellEx\\ContextMenuHandlers\\PboExplorer",											true},
		//{HKEY_CLASSES_ROOT, L".pbo_auto_file\\ShellEx\\ContextMenuHandlers\\PboExplorer",L"",					L"{0}",				true},

		// Context Menu on folders (create PBO from folder)
		{HKEY_CLASSES_ROOT, L"Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer",												true},
		{HKEY_CLASSES_ROOT, L"Folder\\ShellEx\\ContextMenuHandlers\\PboExplorer",	L"",					L"{0}",				true},

		// .pbo file format general, association, its handled by PboExplorer key
		{HKEY_CLASSES_ROOT, L".pbo",																							true},
		{HKEY_CLASSES_ROOT, L".pbo",												L"",					L"PboExplorer",		true}, // reference to HKEY_CLASSES_ROOT/PboExplorer key

		// Same thing again? just for CLSID? Dunno.
		{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbo",																	true},
		{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbo\\CLSID",				L"",					L"{0}",				true}
	};

	//#TODO Computer\HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pbo\OpenWithProgids
	// in there, it may have a .pbo\UserChoice folder that overrides the default open action and can also block PboExplorer context menu on pbo files. That needs to be deleted


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
	{HKEY_CLASSES_ROOT, L".pbo",																							true},
	{HKEY_CLASSES_ROOT, L".pbo",												L"",					L"PboExplorer",		true}, // reference to HKEY_CLASSES_ROOT/PboExplorer key

	// Same thing again? just for CLSID? Dunno.
	{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbo",																	true},
	{HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.pbo\\CLSID",				L"",					L"{0}",				true}
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