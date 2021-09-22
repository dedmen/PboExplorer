#include "Util.hpp"

#include "ComRef.hpp"

void Util::WaitForDebuggerSilent() {
    if (IsDebuggerPresent())
        return;

    while (!IsDebuggerPresent())
        Sleep(10);    

    // We waited for attach, so break us there
    __debugbreak();
}

void Util::WaitForDebuggerPrompt() {
    if (IsDebuggerPresent())
        return;

    while (!IsDebuggerPresent())
        MessageBoxA(0, "PboExplorer waiting for debugger!", "PboExplorer waiting for debugger!", 0);

    // We waited for attach, so break us there
    __debugbreak();
}

// https://github.com/dedmen/cpp-base64/blob/0aaaf66785558807da1c331be114f8727f7f5a2b/base64.cpp


/*
   base64.cpp and base64.h
   base64 encoding and decoding with C++.
   Version: 1.01.00
   Copyright (C) 2004-2017 René Nyffenegger
   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.
   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:
   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.
   3. This notice may not be removed or altered from any source distribution.
   René Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";


static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string Util::base64_encode(std::string_view s) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = s.length();
    auto bytes_to_encode = s.begin();
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';

    }

    return ret;

}

std::string Util::base64_decode(std::string_view encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }

    return ret;
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

