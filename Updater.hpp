#pragma once


/*
Taking care of updating the shell extension:
- Check for update available
- Download new update
- Verify new update files are correct
- Install update (register to be renamed at PC reboot, or ask user if he wants to do now or later)
- Post update handling

*/ 

#include <filesystem>
#include <optional>
#include <string>

class Updater
{
    struct AvailableUpdateInfo {
        // version number of new download
        uint64_t newVersionNo;

        // download link for the new PboExplorer dll
        std::string downloadLinkDLL;
        // download link for the new Update Helper exe
        std::string downloadLinkUpdateHelper;

        // Hash of new files post download
        uint64_t dllHash;
        uint64_t updateHelperHash;
    };

    Updater();


    std::optional<AvailableUpdateInfo> IsUpdateAvailable();
    bool DownloadFile(std::string URL, std::filesystem::path targetDirectory);
    bool DownloadNewUpdate(const AvailableUpdateInfo&);
    bool VerifyDownloadedUpdate(const AvailableUpdateInfo&);
    bool RegisterUpdateInstallOnReboot();
    bool InstallUpdateNow();
    void PostUpdateInstalled();
    void CleanupUpdateFiles();

    std::filesystem::path UpdateTempDir;
    std::filesystem::path PboExplorerDir;

public:
    // Called on application startup, does all the update checking
    static void OnStartup();





};

