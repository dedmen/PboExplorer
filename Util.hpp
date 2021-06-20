#pragma once
#include <string>

#include "ClassFactory.hpp"

using namespace std::string_view_literals;

template<char... Cs>
struct CharSeq
{
    static constexpr const char s[] = { Cs..., 0 }; // The unique address
};

// That template uses the extension
template<char... Cs>
constexpr CharSeq<Cs...> operator"" _cs() {
    return {};
}

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


    template <typename T, size_t N>
    class FlagSeperator {
        std::array<std::pair<T, char[32]>, N> dataHolder;
    public:
        consteval FlagSeperator(std::initializer_list<std::pair<T, std::string_view>> data) noexcept {

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
        
            if (idx != N)
                throw std::logic_error("Array size doesn't match provided arguments");
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



};

