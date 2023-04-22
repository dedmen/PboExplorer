#include "Util.hpp"
import ComRef;
#include <Windows.h>

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

#include <ShlObj.h>
#include <RestartManager.h>
#pragma comment(lib ,"Rstrtmgr.lib")

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

