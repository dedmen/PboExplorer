#pragma once
#include <string>

#include "ClassFactory.hpp"

class Util
{
public:
    // Convert a wide Unicode string to an UTF8 string
    static std::string utf8_encode(const std::wstring& wstr)
    {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
        return strTo;
    }

    // Convert an UTF8 string to a wide Unicode String
    static std::wstring utf8_decode(const std::string& str)
    {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    static std::string random_string(size_t length)
    {
        auto randchar = []() -> char
        {
            const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
            const size_t max_index = (sizeof(charset) - 1);
            return charset[rand() % max_index];
        };
        std::string str(length, 0);
        std::generate_n(str.begin(), length, randchar);
        return str;
    }

    static void TryDebugBreak() {
        if (IsDebuggerPresent())
            __debugbreak();
    }

    static void WaitForDebuggerSilent();
    static void WaitForDebuggerPrompt();


    static auto splitArgs (std::wstring_view cmdLine) {
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


};

