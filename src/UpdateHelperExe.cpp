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