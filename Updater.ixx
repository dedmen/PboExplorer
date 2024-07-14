module;
#define NOMINMAX
#include <windows.h>
#include <windns.h>
#include <urlmon.h>
#include <shlobj_core.h>
#pragma comment (lib,"urlmon.lib")
#pragma comment(lib, "Dnsapi.lib")


export module Updater;


import std;

import Util;
import Version;
import Encoding;
import FileUtil;





/*
Taking care of updating the shell extension:
- Check for update available
- Download new update
- Verify new update files are correct
- Install update (register to be renamed at PC reboot, or ask user if he wants to do now or later)
- Post update handling

*/

import std;
import std.compat;

export class Updater
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
















static std::vector<std::string_view> SplitString(std::string_view s, char delim)
{

    std::vector<std::string_view> elems;
    std::string::size_type lastPos = 0;
    const std::string::size_type length = s.length();

    while (lastPos < length + 1) {
        std::string::size_type pos = s.find_first_of(delim, lastPos);
        if (pos == std::string::npos) {
            pos = length;
        }

        //if (pos != lastPos || !trimEmpty)
        elems.emplace_back(s.data() + lastPos, pos - lastPos);

        lastPos = pos + 1;
    }

    return elems;
}

static std::optional<uint64_t> ParseUInt64(std::string_view input)
{
    uint64_t result;
    if (auto [p, ec] = std::from_chars(input.data(), input.data() + input.size(), result); ec == std::errc())
        return result;
    return {};
}



#include <Winerror.h>


Updater::Updater()
{
    PboExplorerDir = Util::GetCurrentDLLPath().parent_path();

    wchar_t appPath[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appPath);

    UpdateTempDir = std::filesystem::path(appPath) / "Arma 3" / "PboExplorer";

    std::filesystem::create_directories(UpdateTempDir);
}

std::optional<Updater::AvailableUpdateInfo> Updater::IsUpdateAvailable()
{
    PDNS_RECORD pDnsRecord;
    auto status = DnsQuery_UTF8("PboExplorerVersion3.arma3.io",
        DNS_TYPE_TEXT,
        DNS_QUERY_BYPASS_CACHE | DNS_QUERY_WIRE_ONLY | DNS_QUERY_TREAT_AS_FQDN, // Bypasses the resolver cache on the lookup.
        NULL,
        &pDnsRecord,
        NULL);

    auto actualRec = reinterpret_cast<_DnsRecordA*>(pDnsRecord);
    if (!pDnsRecord)
        return {};


    AvailableUpdateInfo info{};


    /*
    string format
        versionNo;dllHash;updaterHash;dllURL;updaterURL
    */

    if (actualRec->wType == DNS_TYPE_TEXT) {
        // combine all strings, they are split into 255 chunks

        std::string result;

        for (size_t i = 0; i < actualRec->Data.TXT.dwStringCount; ++i)
            result.append(actualRec->Data.TXT.pStringArray[i]);

        auto splitStuff = SplitString(result, ';');

        if (splitStuff.size() < 5)
            return {}; // fail

        auto versionNo = ParseUInt64(splitStuff[0]);
        auto dllHash = ParseUInt64(splitStuff[1]);
        auto updaterHash = ParseUInt64(splitStuff[2]);

        if (!versionNo || !dllHash || !updaterHash) //#TODO sentry
            return {}; // fail

        info.newVersionNo = *versionNo;
        info.dllHash = *dllHash;
        info.updateHelperHash = *updaterHash;
        info.downloadLinkDLL = splitStuff[3];
        info.downloadLinkUpdateHelper = splitStuff[4];
    }

    DnsFree(pDnsRecord, DnsFreeRecordList);

    if (!info.newVersionNo) // will be null if default initialized and failed to parse the DNS record
        return {};


    if (info.newVersionNo <= VERSIONNO) // we are on latest version
        return {};


    // We want update to version, is update already present and just waiting for reboot?

    if (std::filesystem::exists(UpdateTempDir / "OldVersion.txt")) { // OldVersion file is only created when update is installed
      // old version is present, good sign that update is in progress
    
        if (VerifyDownloadedUpdate(info)) // update is in progress, and downloaded version is still newest, nothing to do here
            return {};
    }



    return info;
}

bool Updater::DownloadFile(std::string URL, std::filesystem::path targetFile)
{
    auto URLW = UTF8::Decode(URL);
    auto filenameW = targetFile.wstring();
    auto hres = URLDownloadToFileW(nullptr, URLW.c_str(), filenameW.c_str(), 0, nullptr);
   
    return SUCCEEDED(hres);
}

bool Updater::DownloadNewUpdate(const Updater::AvailableUpdateInfo& info)
{
    CleanupUpdateFiles();

    bool dllDownloadSuccess = true, updaterHelperDownloadSuccess = true;

    // don't need to download PboExplorer.dll if we already have the same file
    if (GetFileHash(PboExplorerDir / "PboExplorer.dll") != info.dllHash)
         dllDownloadSuccess = DownloadFile(info.downloadLinkDLL, UpdateTempDir / "PboExplorer.dll");


    // don't need to download PboExplorerUpdateHelper.exe if we already have the same file
    if (GetFileHash(PboExplorerDir / "PboExplorerUpdateHelper.exe") != info.dllHash)
        updaterHelperDownloadSuccess = DownloadFile(info.downloadLinkUpdateHelper, UpdateTempDir / "PboExplorerUpdateHelper.exe");

    return dllDownloadSuccess && updaterHelperDownloadSuccess;
}

bool Updater::VerifyDownloadedUpdate(const Updater::AvailableUpdateInfo& info)
{
    bool hasDLL = std::filesystem::exists(UpdateTempDir / "PboExplorer.dll");
    bool hasUpdateHelper = std::filesystem::exists(UpdateTempDir / "PboExplorerUpdateHelper.exe");

    auto dllHash = GetFileHash(UpdateTempDir / "PboExplorer.dll");

    if (hasDLL && dllHash != info.dllHash)
        return false; // hash missmatch

    auto helperHash = GetFileHash(UpdateTempDir / "PboExplorerUpdateHelper.exe");

    if (hasUpdateHelper && helperHash != info.updateHelperHash)
        return false; // hash missmatch

    if (!hasDLL && !hasUpdateHelper && info.newVersionNo != VERSIONNO)
        return false; // our version is different, so we want an update, but we got none

    //#TODO Digital signature verification, take from Intercept

    return true; // hashes match the desired ones
}

bool Updater::RegisterUpdateInstallOnReboot()
{
    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexa

    bool hasDLL = std::filesystem::exists(UpdateTempDir / "PboExplorer.dll");
    bool hasUpdateHelper = std::filesystem::exists(UpdateTempDir / "PboExplorerUpdateHelper.exe");


    if (hasUpdateHelper) { // try installing update helper first, if we can't thats gotta be done at reboot.
        try {
            std::filesystem::rename(UpdateTempDir / "PboExplorerUpdateHelper.exe", PboExplorerDir / "PboExplorerUpdateHelper.exe");
            // if we didn't throw exception, that worked. Delete the file so that UpdateHelper doesn't see it anymore
            hasUpdateHelper = false;
            std::filesystem::remove(UpdateTempDir / "PboExplorerUpdateHelper.exe");
        } catch (...) {}
    }


    if (hasDLL || hasUpdateHelper) {
        // tell our update helper to take care of it.

        {
            std::ofstream oldversion(UpdateTempDir / "OldVersion.txt", std::ofstream::out | std::ofstream::binary);

            auto oldVersionStr = std::format("{}", VERSIONNO);
            oldversion.write(oldVersionStr.data(), oldVersionStr.size());
        }

        MessageBoxA(GetDesktopWindow(), "PboExplorer needs Admin rights to tell Windows to update it on next restart, I will now prompt you for that.", "PboExplorer", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);

        // always run the new update helper
        auto targetFile = hasUpdateHelper ? (UpdateTempDir / "PboExplorerUpdateHelper.exe") : (PboExplorerDir / "PboExplorerUpdateHelper.exe");

        auto fileNameW = targetFile.lexically_normal().wstring();
        auto commandLine = std::format(L"RegisterFileMove {}", UTF8::Decode(Base64::Encode(UTF8::Encode(PboExplorerDir.lexically_normal().wstring()))));

        return ShellExecuteW(
            GetDesktopWindow(),
            L"runas",
            fileNameW.c_str(),
            commandLine.c_str(),
            NULL,
            SW_SHOW) != nullptr;
    }

    return false;
}

bool Updater::InstallUpdateNow()
{

    // https://docs.microsoft.com/en-us/windows/win32/rstmgr/restart-manager-portal?redirectedfrom=MSDN

    return false;
}

void Updater::PostUpdateInstalled()
{
    if (!std::filesystem::exists(UpdateTempDir / "OldVersion.txt"))  // OldVersion file is only created when update is installed
        return; 
     
    // we just restarted after installing a new update. Handle it.

    auto oldVersionString = ReadWholeFile(UpdateTempDir / "OldVersion.txt");
    if (ParseUInt64(oldVersionString) == VERSIONNO) {
        //MessageBoxA(GetDesktopWindow(), "pboExplorer update WIP?", "PboExplorer", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);

        return; // Update is WIP but not actually installed yet
    }
        

    CleanupUpdateFiles();

    auto textA = std::format("PboExplorer installed a new update (Old: {}, New: {})", oldVersionString, VERSIONNO);

    MessageBoxA(GetDesktopWindow(), textA.c_str(), "PboExplorer", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);



    // can do post update handling here
}

void Updater::CleanupUpdateFiles()
{
    try {
        if (std::filesystem::exists(UpdateTempDir / "PboExplorer.dll"))
            std::filesystem::remove(UpdateTempDir / "PboExplorer.dll");

        if (std::filesystem::exists(UpdateTempDir / "PboExplorerUpdateHelper.exe"))
            std::filesystem::remove(UpdateTempDir / "PboExplorerUpdateHelper.exe");

        if (std::filesystem::exists(UpdateTempDir / "OldVersion.txt"))
            std::filesystem::remove(UpdateTempDir / "OldVersion.txt");
    }
    catch (...) {}
}

void Updater::OnStartup()
{
    Updater upd;

    upd.PostUpdateInstalled();

    auto updateInfo = upd.IsUpdateAvailable();
    if (!updateInfo)
        return;

    // Check if we already have files available that match
    if (!upd.VerifyDownloadedUpdate(*updateInfo)) {

        // either no files available or old or corrupt, download the new version
        if (!upd.DownloadNewUpdate(*updateInfo)) {
            // #TODO Sentry info

            return;
        }

        if (!upd.VerifyDownloadedUpdate(*updateInfo)) {// download got corrupted files or smth, clean up the files and ignore the update for now
            upd.CleanupUpdateFiles();
            return; 
        }
    }

    // update now downloaded and ready to install. Ask user if he wants to install now or do restart
    auto textA = std::format("PboExplorer has a new update available (Current: {}, New: {})", VERSIONNO, updateInfo->newVersionNo);

    MessageBoxA(GetDesktopWindow(), textA.c_str(), "PboExplorer", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST);

    //#TODO Yes/No/Don't ask again for this version

    upd.RegisterUpdateInstallOnReboot();
}
