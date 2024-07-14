module;
#include <Windows.h>

#include <ShlObj.h>
#include <RestartManager.h>
#pragma comment(lib ,"Rstrtmgr.lib")
export module Util;

import std;
import std.compat;
import ComRef;
import Version;

export using namespace std::string_view_literals;

template<char... Cs>
struct CharSeq
{
    static constexpr const char s[] = { Cs..., 0 }; // The unique address
};

// That template uses the extension
export template<char... Cs>
constexpr CharSeq<Cs...> operator"" _cs() {
    return {};
}

namespace {
    struct MyEqual : public std::equal_to<>
    {
        using is_transparent = void;
    };

    struct string_hash {
        using is_transparent = void;
        using key_equal = std::equal_to<>;  // Pred to use
        using hash_type = std::hash<std::string_view>;  // just a helper local type
        size_t operator()(std::string_view txt) const { return hash_type{}(txt); }
        size_t operator()(const std::string& txt) const { return hash_type{}(txt); }
        size_t operator()(const char* txt) const { return hash_type{}(txt); }
    };
};

export namespace Util
{
    //inline std::string random_string(size_t length)
    //{
    //    auto randchar = []() -> char
    //    {
    //        const char charset[] =
    //            "0123456789"
    //            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    //            "abcdefghijklmnopqrstuvwxyz";
    //        const size_t max_index = (sizeof(charset) - 1);
    //        return charset[rand() % max_index];
    //    };
    //    std::string str(length, 0);
    //    std::generate_n(str.begin(), length, randchar);
    //    return str;
    //}

    void TryDebugBreak();

    void WaitForDebuggerSilent();
    void WaitForDebuggerPrompt(std::string_view message = {}, bool canIgnore = false);


    inline auto splitArgs(std::wstring_view cmdLine) {
        std::vector<std::wstring_view> ret;

        auto subRange = cmdLine;

        while (!subRange.empty()) {
            if (subRange[0] == L' ') { // skip whitespace
                subRange = subRange.substr(1);
            }
            else if (subRange[0] == L'"') { // quoted string
                auto endStr = subRange.find(L'"', 1);

                if (endStr == std::string::npos) {
                    ret.emplace_back(subRange);
                    break;
                }

                ret.emplace_back(subRange.substr(0, endStr + 1));

                subRange = subRange.substr(endStr + 1);
            }
            else {
                // non quoted, whitespace delimited string
                auto endStr = subRange.find(L' ', 1);

                if (endStr == std::string::npos) {
                    ret.emplace_back(subRange);
                    break;
                }

                ret.emplace_back(subRange.substr(0, endStr + 1));

                subRange = subRange.substr(endStr + 1);
            }
        }

        return ret;
    };


    template <typename T, size_t N>
    class FlagSeperator {
        std::array<std::pair<T, char[32]>, N> dataHolder;
    public:
        consteval FlagSeperator(std::initializer_list<std::pair<T, std::string_view>> data) noexcept {

            if (data.size() != N)
                throw std::logic_error("Array size doesn't match provided arguments");

            int idx = 0;
            for (auto& it : data) {
                dataHolder[idx].first = it.first;

                int i = 0;
                for (auto& ch : it.second) {
                    dataHolder[idx].second[i++] = ch;
                }
                dataHolder[idx].second[i] = 0;

                ++idx;
            }
        }

        std::string SeperateToString(T flags) const {
            std::string result;

            for (auto& it : dataHolder) {
                if (flags & it.first) {
                    result += it.second;
                    result += "|";
                }
            }

            if (!result.empty())
                result.pop_back();
            return result;
        }




    };

    template<typename T>
    using unordered_map_stringkey = std::unordered_map<std::string, T, string_hash, MyEqual >;


    /// <summary>
    /// Uses Restart Manager to retrieve which processes are holding a write lock on file
    /// </summary>
    /// <param name="file"></param>
    /// <returns>List of process names (human readable and exe name)</returns>
    std::vector<std::wstring> GetProcessesThatLockFile(std::filesystem::path file);


    std::filesystem::path GetCurrentDLLPath();

};

export void TRY_ASSERT(bool x) {
    if (!(x)) Util::TryDebugBreak();
}



void Util::TryDebugBreak() {
    if (IsDebuggerPresent())
        __debugbreak();
}

void Util::WaitForDebuggerSilent() {
    if (IsDebuggerPresent())
        return;

    while (!IsDebuggerPresent())
        Sleep(10);

    // We waited for attach, so break us there
    __debugbreak();
}

void Util::WaitForDebuggerPrompt(std::string_view message, bool canIgnore) {
    if (IsDebuggerPresent())
        return;

    auto currentPid = GetCurrentProcessId();

    char fname[512];
    GetModuleFileNameA(nullptr, fname, 511);

    const std::string text = 
        std::format("PboExplorer waiting for debugger!\nName: {}\nPID: {}\nMsg: {}{}", std::string_view(fname), currentPid,
            message,
            canIgnore ? "" : "\n(Abort/Cancel will break to debugger)"
        );

    while (!IsDebuggerPresent()) {
        const auto result = MessageBoxA(0, text.c_str(), "PboExplorer waiting for debugger!", MB_RETRYCANCEL | MB_SYSTEMMODAL);
        if (result == IDCANCEL) {
            if (canIgnore) return; // skip forced break
            break;
        }
    }

    // We waited for attach, so break us there
    __debugbreak();
}


std::vector<std::wstring> Util::GetProcessesThatLockFile(std::filesystem::path file) {
    std::vector<std::wstring> result;

    std::wstring fileName = file.wstring();
    LPCWSTR data = fileName.data();


    // Check IFileInUse first
    // https://web.archive.org/web/20070429124756/http://shellrevealed.com/blogs/shellblog/archive/2007/03/29/Your-File-Is-In-Use_2620_-Demystified.aspx
    // https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ifileisinuse

    ComRef<IMoniker> pmkFile;
    ComRef<IRunningObjectTable> prot;
    ComRef<IEnumMoniker> penumMk;
    if (SUCCEEDED(CreateFileMoniker(fileName.data(), pmkFile.AsQueryInterfaceTarget<IMoniker>())))
    if (SUCCEEDED(GetRunningObjectTable(NULL, prot.AsQueryInterfaceTarget<IRunningObjectTable>())))
    if (SUCCEEDED(prot->EnumRunning(penumMk.AsQueryInterfaceTarget<IEnumMoniker>())))
    {
        ComRef<IMoniker> pmk;
        while (penumMk->Next(1, pmk.AsQueryInterfaceTarget<IMoniker>(), nullptr) == S_OK)
        {
            DWORD dwType;
            if (SUCCEEDED(pmk->IsSystemMoniker(&dwType)) && (dwType == MKSYS_FILEMONIKER))
            {
                // Is this a moniker prefix?
                ComRef<IMoniker> pmkPrefix;
                if (SUCCEEDED(pmkFile->CommonPrefixWith(pmk, pmkPrefix.AsQueryInterfaceTarget<IMoniker>())))
                {
                    if (pmkFile->IsEqual(pmkPrefix) == S_OK)
                    {
                        // Get the IFileIsInUse instance
                        ComRef<IUnknown> punk;
                        if (prot->GetObject(pmk, punk.AsQueryInterfaceTarget<IUnknown>()) == S_OK)
                        {
                            ComRef<IFileIsInUse> pfiu;
                            if (SUCCEEDED(punk->QueryInterface(__uuidof(IFileIsInUse), pfiu.AsQueryInterfaceTarget())) && pfiu) {
                                CoTaskMemRefS<wchar_t> target;
                                pfiu->GetAppName(&target);
                                result.emplace_back(target.GetRef());
                                CoTaskMemFree(target);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!result.empty()) // IFileInUse worked and returned some results, thats fine for us just use it.
        return result;

    // IFileInUse didn't supply info, try Restart Manager instead

    DWORD dwSession;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
    DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError == ERROR_SUCCESS)
    {
        dwError = RmRegisterResources(dwSession, 1, &data, 0, nullptr, 0, nullptr);
        if (dwError == ERROR_SUCCESS)
        {
            UINT nProcInfoNeeded = 0;
            UINT nProcInfo = 0;
            DWORD dwReason;

            // Get required buffercount
            RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, nullptr, &dwReason);

            if (nProcInfoNeeded != 0) // no processes have lock
            {
                nProcInfoNeeded += 2; // overcompensation, in case more processes lock it between our size check and our actual get, shouldn't happen for PBO's
                std::vector<RM_PROCESS_INFO> buffer;
                buffer.resize(nProcInfoNeeded);
                nProcInfo = nProcInfoNeeded;

                if (RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, buffer.data(), &dwReason) == ERROR_SUCCESS) {
                    buffer.resize(nProcInfo); // nProcInfo is the number of actual buffer entries that were filled, maybe processes disappeared between our size check and now
                    for (auto& processInfo : buffer) {
                        // get process imagename
                        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processInfo.Process.dwProcessId);
                        if (hProcess)
                        {
                            FILETIME ftCreate, ftExit, ftKernel, ftUser;
                            if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser) &&
                                CompareFileTime(&processInfo.Process.ProcessStartTime, &ftCreate) == 0)  // if filetime doesn't match, the process exited since our query
                            {
                                WCHAR sz[MAX_PATH];
                                DWORD cch = MAX_PATH;
                                if (QueryFullProcessImageNameW(hProcess, 0, sz, &cch) && cch <= MAX_PATH)
                                    result.emplace_back(std::format(L"{} (PID: {}, Path: {})", processInfo.strAppName, processInfo.Process.dwProcessId, sz));
                            }
                            CloseHandle(hProcess);
                        }
                    }
                }
            }
        }
    }

    RmEndSession(dwSession);
    return result;
}

std::filesystem::path Util::GetCurrentDLLPath() {
    char path[MAX_PATH];
    HMODULE hm = NULL;

    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&GetCurrentDLLPath), &hm) == 0)
    {
        //int ret = GetLastError();
        //fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
        // Return or however you want to handle an error.
    }
    if (GetModuleFileNameA(hm, path, sizeof(path)) == 0)
    {
        //int ret = GetLastError();
        //fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
        // Return or however you want to handle an error.
    }

    return std::filesystem::path(path);
}
