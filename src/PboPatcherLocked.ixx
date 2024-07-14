module;

#include "PboFileDirectory.hpp"

#include <Windows.h>
export module PboPatcherLocked;

import std;
import std.compat;
import PboPatcher;
import Util;

export
class PboPatcherLocked {
    std::shared_ptr<PboFile> pboFile;
    PboPatcher patcher;
    std::ifstream readStream;
    std::optional<PboReader> reader;
    // Do nothing in destructor, don't process, don't patch
    bool patchingBlocked = false;
public:

    PboPatcherLocked(std::shared_ptr<PboFile> file) : pboFile(file) {
        GPboFileDirectory.AcquireGlobalPatchLock();

        // try out if output file is writable.


        bool retry = false;
        do {
            retry = false;
            std::fstream outputStream(pboFile->diskPath, std::fstream::binary | std::fstream::in | std::fstream::out);

            if (!outputStream.is_open() || outputStream.fail()) {
                // Output file cannot be opened in write mode, fail
                patchingBlocked = true; // Don't patch on destructor

                auto lockingProcesses = Util::GetProcessesThatLockFile(pboFile->diskPath);
                // if this list is empty, windows tells us no process has the file locked, maybe we just had very bad timing?
                // we could just try to reopen and see if it works, but this is so unlikely to ever happen so I'll just spare myself the time

                size_t textNumberOfChars = std::accumulate(lockingProcesses.begin(), lockingProcesses.end(), 0ull, [](size_t sz, const std::wstring& str) { return sz + str.length() + 1; });

                textNumberOfChars += pboFile->diskPath.native().length();
                textNumberOfChars += std::wstring_view(L"PboExplorer cannot open the target file for patching: \nIt is blocked by the following processes:\n").length();

                std::wstring message;
                message.resize(textNumberOfChars);

                auto fmtResult = std::format_to_n(message.begin(), textNumberOfChars, L"PboExplorer cannot open the target file for patching:\n{}\nIt is blocked by the following process{}:\n\n", pboFile->diskPath.native(), lockingProcesses.size() > 1 ? L"es" : L"");
                textNumberOfChars -= fmtResult.size;

                for (const auto& process : lockingProcesses) {
                    fmtResult = std::format_to_n(fmtResult.out, textNumberOfChars, L"{}\n", process);
                    textNumberOfChars -= fmtResult.size;
                }

                //#TODO text doesn't usually fit.
                // https://docs.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-taskdialogindirect ?
                // https://www.codeproject.com/Articles/1239/XMessageBox-A-reverse-engineered-MessageBox ?
                const auto result = MessageBoxW(GetDesktopWindow(), message.data(), L"PboExplorer", MB_RETRYCANCEL | MB_ICONWARNING | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);

                retry = result == IDRETRY;
            }
            else {
                patchingBlocked = false; // is gud now
            }

            outputStream.close();
        } while (retry);


        if (patchingBlocked)
            return; // Don't read in and prepare patcher

        readStream = std::ifstream(pboFile->diskPath, std::ifstream::in | std::ifstream::binary);
        reader.emplace(readStream);

        reader->readHeaders();
        patcher.ReadInputFile(&reader.value());
    }

    ~PboPatcherLocked() {

        if (patchingBlocked)
            return; // Not doing any patching

        patcher.ProcessPatches();

        reader = std::nullopt;
        readStream.close();

        {
            std::fstream outputStream(pboFile->diskPath, std::fstream::binary | std::fstream::in | std::fstream::out);
            TRY_ASSERT(outputStream.is_open());
            TRY_ASSERT(!outputStream.fail());

            if (!outputStream.is_open() || outputStream.fail()) {

                auto message = std::format(L"PboExplorer couldn't patch the file, it was unable to open the target file for writing even though it checked at the start of the patch process that it was available. Sorry.\n{}\nPatching was not done, if you made a edit please retry.", pboFile->diskPath.native());
                const auto result = MessageBoxW(GetDesktopWindow(), message.data(), L"PboExplorer", MB_RETRYCANCEL | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);
            }

            patcher.WriteOutputFile(outputStream);
        }
        GPboFileDirectory.ReleaseGlobalPatchLock();

        //#TODO this is unsafe, can be called from different threads while something else is working on the pbo. I guess we want to lock the filelists?
        pboFile->ReloadFrom(pboFile->diskPath);
    }

    template<PatchOperation T, typename ... Args>
    void AddPatch(Args&& ...args) {
        patcher.AddPatch<T, Args...>(std::forward<Args>(args)...);
    }

    const auto& GetFilesFromReader() {
        return reader->getFiles();
    }
};