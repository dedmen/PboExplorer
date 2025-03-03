#include <string_view>
#include <shlobj_core.h>

import Util;

import std;
import FileUtil;
import Encoding;

using namespace std::string_view_literals;
#ifdef IS_RESTART_HELPER

int main(int argc, char* argv[]) {


    //#TODO  https://docs.microsoft.com/en-us/windows/win32/api/restartmanager/nf-restartmanager-rmrestart

    if (std::string_view(argv[1]) == "RegisterFileMove"sv) {
        //Util::WaitForDebuggerPrompt();

        std::filesystem::path PboExplorerDir(UTF8::Decode(Base64::Decode(std::string_view(argv[2]))));

        wchar_t appPath[MAX_PATH];
        SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appPath);

        auto UpdateTempDir = std::filesystem::path(appPath) / "Arma 3" / "PboExplorer" / "UpdateTemp";

        if (!std::filesystem::exists(UpdateTempDir)) { // Backwards compat
            UpdateTempDir = std::filesystem::path(appPath) / "Arma 3" / "PboExplorer";
        }

        std::filesystem::create_directories(UpdateTempDir);

        bool hasDLL = std::filesystem::exists(UpdateTempDir / "PboExplorer.dll");
        bool hasUpdateHelper = std::filesystem::exists(UpdateTempDir / "PboExplorerUpdateHelper.exe");

        // The problem with the DLL is that our target is generally already in use, it's currently loaded in explorer
        // But maybe, we are lucky and windows lets us rename it while its in use. This generally works
        try {
            std::filesystem::rename(PboExplorerDir / "PboExplorer.dll", PboExplorerDir / "PboExplorerOld.dll");
            // If we didn't throw an exception till here, we succeeded! We can just move in the new one and mark the old one for deletion
            std::filesystem::rename(UpdateTempDir / "PboExplorer.dll", PboExplorerDir / "PboExplorer.dll");

            auto sourceFile = (PboExplorerDir / "PboExplorerOld.dll").lexically_normal().wstring();
            MoveFileExW(sourceFile.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

            hasDLL = false; // We already handled it
        } catch (...) {

        }

        // The DLL is present, and it's currently loaded in explorer so we cannot touch it, mark it for windows to be swapped out at next reboot
        if (hasDLL) {
            // Windows cannot move between different drives. We are running in Admin, lets just place it into the correct directory right away
            std::filesystem::rename(UpdateTempDir / "PboExplorer.dll", PboExplorerDir / "PboExplorerNew.dll");

            auto sourceFile = (PboExplorerDir / "PboExplorerNew.dll").lexically_normal().wstring();
            auto targetFile = (PboExplorerDir / "PboExplorer.dll").lexically_normal().wstring();
            // delete old one
            MoveFileExW(targetFile.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            // move new one
            MoveFileExW(sourceFile.c_str(), targetFile.c_str(), MOVEFILE_DELAY_UNTIL_REBOOT);
        }

        if (hasUpdateHelper) {
            // Windows cannot move between different drives. We are running in Admin, lets just place it into the correct directory right away
            std::filesystem::rename(UpdateTempDir / "PboExplorerUpdateHelper.exe", PboExplorerDir / "PboExplorerUpdateHelperNew.exe");

            auto sourceFile = (PboExplorerDir / "PboExplorerUpdateHelperNew.exe").lexically_normal().wstring();
            auto targetFile = (PboExplorerDir / "PboExplorerUpdateHelper.exe").lexically_normal().wstring();
            // delete old one
            MoveFileExW(targetFile.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            // move new one
            MoveFileExW(sourceFile.c_str(), targetFile.c_str(), MOVEFILE_DELAY_UNTIL_REBOOT);
        }
    } else if (std::string_view(argv[1]) == "GetFileHash"sv) {
        auto hash = GetFileHash(std::string_view(argv[2]));
        std::cout << hash << " hex " << std::hex << hash;
    }

    return 0;
}
#endif