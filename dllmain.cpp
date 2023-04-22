#include <Windows.h>
#include <ShlObj.h>
#include <OleCtl.h>
#include <fstream>
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

#ifdef ENABLE_SENTRY
#define SENTRY_BUILD_STATIC 1
#include "lib/sentry/include/sentry.h"
// PboExplorer\lib\sentry>cmake -B build -D SENTRY_BACKEND=crashpad SENTRY_BUILD_SHARED_LIBS=OFF --config RelWithDebInfo -S .
// manually edit the vcproj to be a .lib
// https://docs.sentry.io/platforms/native/
#pragma comment(lib, "sentry")
#pragma comment(lib, "crashpad_client")
#pragma comment(lib, "crashpad_util")
#pragma comment(lib, "mini_chromium")
#pragma comment(lib, "dbghelp")
#pragma comment(lib, "Version")
#pragma comment(lib, "Winhttp")
#include <Lmcons.h>

#define OS_WIN 1
#include "lib/sentry/external/crashpad/third_party/mini_chromium/mini_chromium/base/files/file_path.h"
#include "lib/sentry/external/crashpad/util/file/file_reader.h"
#include "lib/sentry/external/crashpad/util/net/http_body.h"
#include "lib/sentry/external/crashpad/util/net/http_multipart_builder.h"
#include "lib/sentry/external/crashpad/util/net/http_transport.h"
#include "lib/sentry/external/crashpad/util/net/http_headers.h"

//#TODO https://github.com/getsentry/sentry/issues/12670

#endif
#include <future>
#include "Updater.hpp"

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
		Util::WaitForDebuggerPrompt();
#endif
#ifdef ENABLE_SENTRY
		sentry_close();
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
	return (g_DllRefCount ? S_FALSE : S_OK);
}


std::string GetClipboardText()
{
	if (!OpenClipboard(nullptr)) return "";

	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == nullptr) return "";

	// Lock the handle to get the actual text pointer
	char* pszText = static_cast<char*>(GlobalLock(hData));

	if (pszText == nullptr) return "";

	std::string text(pszText);

	GlobalUnlock(hData);
	CloseClipboard();

	return text;
}

extern std::string ReadWholeFile(std::filesystem::path file); // Updater.cpp


/*---------------------------------------------------------------*/
// DllGetClassObject()
/*---------------------------------------------------------------*/
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppReturn)
{
	//Util::WaitForDebuggerPrompt();
#ifdef ENABLE_SENTRY
	static bool SentryInit = false;
	if (!SentryInit) {
		sentry_options_t* options = sentry_options_new();
		sentry_options_set_dsn(options, "http://cc1dc4103efd4f1dbbc0488599f33e58@lima.dedmen.de:9001/2");
		sentry_options_set_release(options, std::format("PboExplorer@{}", VERSIONNO).c_str());

#ifdef _DEBUG
		sentry_options_set_environment(options, "debug");

		sentry_options_set_logger(options, [](sentry_level_t level, const char* message, va_list args, void* userdata) {
			size_t size = 1024;
			char buffer[2048];
			if (vsnprintf(buffer, size, message, args) >= 0)
			{
				DebugLogger::TraceLog(std::format("{} - {}", (int)level, &buffer[0]), std::source_location::current(), "sentry");
			}
			}, nullptr);
		sentry_options_set_debug(options, 1);
#else
		sentry_options_set_environment(options, "production");
#endif

		// get this DLL's path and file name
		wchar_t szModule[MAX_PATH];
		GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule));

		sentry_options_set_handler_pathw(options, (std::filesystem::path(szModule).parent_path() / "crashpad_handler.exe").c_str());
		sentry_options_set_database_pathw(options, (std::filesystem::path(szModule).parent_path() / "PboExplorerCrashDB").c_str());
		try {
			std::filesystem::create_directories(std::filesystem::path(szModule).parent_path() / "PboExplorerCrashDB");
		} catch (...) {}
		//sentry_options_set_database_path(options, "O:\\sentry");

		sentry_options_set_require_user_consent(options, 1);

		sentry_set_level(SENTRY_LEVEL_DEBUG);


		sentry_init(options);


		if (sentry_user_consent_get() == SENTRY_USER_CONSENT_UNKNOWN) {
			if (MessageBoxA(GetDesktopWindow(),
				"Hello and welcome to PBOExplorer!\n"
				"I would like to request your permission to collect some data that i need to keep track of Errors and Crashes.\n\n"
				"Transmitted data contains:\n"
				"- explorer.exe crashdumps (they will be uploaded to a server and analyzed automatically)\n"
				"- PboExplorer internal logmessages (when an error or unexpected thing happens somewhere)\n"
				"- Your Username as identification so I can attribute crashes/errors for further equestions\n"
				"You can change your Username in the next message box after you click yes.",
				"PboExplorer", MB_YESNO | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST) == IDNO) {
				sentry_user_consent_revoke();
			}
			else 
			{
				if (MessageBoxA(GetDesktopWindow(),
					"Thank you! If you want to have a custom username, please copy the text into your Clipboard and click Yes.\n\n"
					"If you press No I will use your windows username",
					"PboExplorer", MB_YESNO | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST) == IDYES) {

					// custom username selection loop

					int res = 0;
					do {
						auto text = GetClipboardText();
						if (text.length() > 2) {
							auto msg = std::format("Thank you, I got \"{}\" is that correct? If no I'll read the clipboard again.", text);

							res = MessageBoxA(GetDesktopWindow(), msg.c_str(), "PboExplorer", MB_YESNO | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);

							if (res == IDYES) {
								std::ofstream ofs(std::filesystem::path(szModule).parent_path() / "PboExplorerCrashDB" / "username.txt");
								ofs.write(text.data(), text.size());
								sentry_user_consent_give();
							}
						}
						else 
						{
							res = MessageBoxA(GetDesktopWindow(), "Uh, the stuff I got is empty, do you want to quit and let me use your windows username?\nThe name is stored in \"username.txt\" and can be changed later.\n\nIf you press No I'll read clipboard again.", "PboExplorer", MB_YESNO | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);
						}
										
					} while (res != IDYES);
				}
			}
		}
		


		char usernameBuf[UNLEN + 1];
		DWORD username_len = UNLEN + 1;
		GetUserNameA(usernameBuf, &username_len);
		std::string username(usernameBuf, username_len);

		if (std::filesystem::exists(std::filesystem::path(szModule).parent_path() / "PboExplorerCrashDB" / "username.txt")) {
			username = ReadWholeFile(std::filesystem::path(szModule).parent_path() / "PboExplorerCrashDB" / "username.txt");
		}

		sentry_value_t user = sentry_value_new_object();
		sentry_value_set_by_key(user, "ip_address", sentry_value_new_string("{{auto}}"));
		
		sentry_value_set_by_key(user, "username", sentry_value_new_string(username.c_str()));
		sentry_set_user(user);

		auto event = sentry_value_new_message_event(
			/*   level */ SENTRY_LEVEL_INFO,
			/*  logger */ "custom",
			/* message */ "It works!"
		);

		auto thread = sentry_value_new_thread(0, "name");
		sentry_value_set_by_key(thread, "stacktrace", sentry_value_new_stacktrace(nullptr, 16));
		sentry_event_add_thread(event, thread);

		sentry_capture_event(event);

		SentryInit = true;

		if (sentry_user_consent_get() == SENTRY_USER_CONSENT_GIVEN) {
			auto uploadDump = [](std::filesystem::path file) {
				crashpad::HTTPMultipartBuilder http_multipart_builder;
				http_multipart_builder.SetGzipEnabled(false);

				static constexpr char kMinidumpKey[] = "upload_file_minidump";

				crashpad::FileReader reader;
				base::FilePath path(file.wstring());
				reader.Open(path);

				http_multipart_builder.SetFileAttachment(
					kMinidumpKey,
					file.filename().string(),
					&reader,
					"application/octet-stream");

				std::unique_ptr<crashpad::HTTPTransport> http_transport(crashpad::HTTPTransport::Create());
				if (!http_transport) {
					return; // UploadResult::kPermanentFailure;
				}

				crashpad::HTTPHeaders content_headers;
				http_multipart_builder.PopulateContentHeaders(&content_headers);
				for (const auto& content_header : content_headers) {
					http_transport->SetHeader(content_header.first, content_header.second);
				}
				http_transport->SetBodyStream(http_multipart_builder.GetBodyStream());
				// TODO(mark): The timeout should be configurable by the client.
				http_transport->SetTimeout(60.0);  // 1 minute.

				std::string url = "http://lima.dedmen.de:9001/api/2/minidump/?sentry_key=cc1dc4103efd4f1dbbc0488599f33e58";
				http_transport->SetURL(url);

				if (!http_transport->ExecuteSynchronously(nullptr)) {
					return; // UploadResult::kRetry;
				}
				reader.Close();
			};

			wchar_t appPath[MAX_PATH];
			SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appPath);
			//#TODO get proper path
			//	https ://www.ibm.com/support/pages/large-dmp-files-example-cusersappdatalocalcrashdumpsccrexe12345dmp-filling-client-device-typically-citrix-server-hard-drive-triggered-microsoft-windows-wer-user-mode-dumps
			//maybe enable it in register/unregister
			std::error_code ec;
			auto crashDir = std::filesystem::path(appPath) / "CrashDumps";
			for (auto& p : std::filesystem::directory_iterator(crashDir, ec)) {
				if (!p.is_regular_file(ec))
					continue;

				if (!p.path().filename().string().starts_with("explorer"))
					continue;

				if (!p.path().filename().string().ends_with("dmp"))
					continue;

				std::thread([path = p.path(), uploadDump]() {
					uploadDump(path);
					std::error_code ec;
					std::filesystem::remove(path, ec);
				}).detach();
			}
		}
	}
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
		std::wstring fullPath = std::vformat(subKeyFormat, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));

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


			std::wstring fullValueName = std::vformat(valueNameFormat, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));

			std::visit([&](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, std::wstring>) {
					std::wstring fullData = std::vformat(arg, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));

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
					static_assert(sizeof(T) < 0, "non-exhaustive visitor!");
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
			std::wstring fullPath = std::vformat(subKeyFormat, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));
			return RegDeleteTreeW(rootKey, fullPath.c_str());
		} else {
			// delete single key
			std::wstring fullPath = std::vformat(subKeyFormat, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));
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