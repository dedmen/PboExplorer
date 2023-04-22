#include "FileWatcher.hpp"
#include <Windows.h>
#include <fstream>
#include <future>

import TempDiskFile;

void FileWatcher::Run() {
    auto tempPath = std::filesystem::temp_directory_path() / "PboExplorer";

    HANDLE hDir = CreateFile(
        tempPath.c_str(), 
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    OVERLAPPED oRead {};
    oRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);



    while (shouldRun) {       
        FILE_NOTIFY_INFORMATION buffer[1024];
        DWORD BytesReturned;
        if (ReadDirectoryChangesW(
            hDir,
            &buffer,
            sizeof(buffer),
            TRUE, 
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_SIZE,
            &BytesReturned,		// bytes returned
            &oRead,			// overlapped buffer
            NULL			// completion routine
        ))
        {

            while (shouldRun) {
                auto res = WaitForSingleObject(oRead.hEvent, 500);

                if (res == WAIT_OBJECT_0) {
                    for (auto& change : buffer)
                        if (change.Action == FILE_ACTION_MODIFIED) {
                            if (!change.FileName[0])
                                continue; // don't know why it gives us empty string but FilenameLength != 0

                            std::wstring_view filename(change.FileName, change.FileNameLength / sizeof(wchar_t));
                            std::unique_lock lck(mainMutex);

                            auto found = watchedFiles.find(std::filesystem::path(filename));
                            if (found == watchedFiles.end())
                                continue;

                            if (auto locked = found->second) {
                                std::async(std::launch::async, [locked]() {Sleep(200); locked->PatchToOrigin(); });
                            }

                        }
                    break;
                }

            }
        }

    }

    CloseHandle(hDir);
    CloseHandle(oRead.hEvent);

}

FileWatcher::FileWatcher() {

    std::filesystem::create_directories(std::filesystem::temp_directory_path() / "PboExplorer");

    // dummy to keep PboExplorer folder alive
    std::ofstream(std::filesystem::temp_directory_path() / "PboExplorer" / std::format(L"h{}", GetCurrentProcessId())).put('A');
}

FileWatcher::~FileWatcher() {
    std::filesystem::remove(std::filesystem::temp_directory_path() / "PboExplorer" / std::format(L"h{}", GetCurrentProcessId()));
    shouldRun = false;
    if (thread.joinable())
        thread.join(); //#TODO triggered _Throw_Cpp_error(_RESOURCE_DEADLOCK_WOULD_OCCUR); on DLL unload after setting shouldRun to false via debugger. Somehow returning from thread triggered dll unload. It was probably kept running while a unload request
}

void FileWatcher::Startup() {
    thread = std::thread([this]() {Run(); });
}

void FileWatcher::WatchFile(std::shared_ptr<TempDiskFile> newFile) {
    std::unique_lock lck(mainMutex);
    watchedFiles[ std::filesystem::relative(newFile->GetPath(), std::filesystem::temp_directory_path() / "PboExplorer")] = newFile;
}

void FileWatcher::UnwatchFile(std::filesystem::path file) {
    std::unique_lock lck(mainMutex);

    watchedFiles.erase(file);
}
