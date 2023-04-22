module;


#ifdef ENABLE_SENTRY

#include <ShlObj.h>
#include <Windows.h>



#define SENTRY_BUILD_STATIC 1
#include "../lib/sentry/include/sentry.h"
// PboExplorer\lib\sentry>cmake -B build -D SENTRY_BACKEND=crashpad -D SENTRY_BUILD_SHARED_LIBS=OF
// PboExplorer\lib\sentry>cmake --build build --config RelWithDebInfo
// PboExplorer\lib\sentry>cmake --build build --config Debug
// manually edit the vcxproj to set all to /MT or /MTd
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
#include "../lib/sentry/external/crashpad/third_party/mini_chromium/mini_chromium/base/files/file_path.h"
#include "../lib/sentry/external/crashpad/util/file/file_reader.h"
#include "../lib/sentry/external/crashpad/util/net/http_body.h"
#include "../lib/sentry/external/crashpad/util/net/http_headers.h"
#include "../lib/sentry/external/crashpad/util/net/http_multipart_builder.h"
#include "../lib/sentry/external/crashpad/util/net/http_transport.h"

//#TODO https://github.com/getsentry/sentry/issues/12670

#endif

export module Sentry;

import <filesystem>;
import <format>;
import <fstream>;
import <source_location>;
import DebugLogger;
import FileUtil;
import Version;

std::string GetClipboardText()
{
	if (!OpenClipboard(nullptr)) return "";

    const HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == nullptr) return "";

	// Lock the handle to get the actual text pointer
    const char* pszText = static_cast<char*>(GlobalLock(hData));

	if (pszText == nullptr) return "";

	std::string text(pszText);

	GlobalUnlock(hData);
	CloseClipboard();

	return text;
}

export namespace Sentry {

#ifdef ENABLE_SENTRY

    void Init(HINSTANCE g_hInst) {
		static bool SentryInit = false;
		if (!SentryInit) {
			sentry_options_t* options = sentry_options_new();
			sentry_options_set_dsn(options, "http://cc1dc4103efd4f1dbbc0488599f33e58@lima.dedmen.de:9001/2");
			sentry_options_set_release(options, std::format("PboExplorer@{}", VERSIONNO).c_str());

#ifdef _DEBUG
			sentry_options_set_environment(options, "debug");

			sentry_options_set_logger(options, [](sentry_level_t level, const char* message, va_list args, void* userdata) {
                const size_t size = 1024;
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
			}
			catch (...) {}
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
                    const base::FilePath path(file.wstring());
					reader.Open(path);

					http_multipart_builder.SetFileAttachment(
						kMinidumpKey,
						file.filename().string(),
						&reader,
						"application/octet-stream");

                    const std::unique_ptr<crashpad::HTTPTransport> http_transport(crashpad::HTTPTransport::Create());
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

                    const std::string url = "http://lima.dedmen.de:9001/api/2/minidump/?sentry_key=cc1dc4103efd4f1dbbc0488599f33e58";
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
    }

    int Close() {
        return sentry_close();
    }

#endif

}
