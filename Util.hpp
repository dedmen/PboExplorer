#pragma once
#define NOMINMAX
#include <algorithm>
#include <string>
#include <stdexcept>
#include <unordered_map>

#include <Windows.h> //#TODO move utf en/decode into util.cpp and remove the windows.h

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

namespace Util
{

    // Convert a wide Unicode string to an UTF8 string
    inline std::string utf8_encode(std::wstring_view wstr)
    {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
        return strTo;
    }

    // Convert an UTF8 string to a wide Unicode String
    inline std::wstring utf8_decode(std::string_view str)
    {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    inline std::string random_string(size_t length)
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

    inline void TryDebugBreak() {
        if (IsDebuggerPresent())
            __debugbreak();
    }

    extern void WaitForDebuggerSilent();
    extern void WaitForDebuggerPrompt();


    inline auto splitArgs (std::wstring_view cmdLine) {
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

    template<typename T>
    using unordered_map_stringkey = std::unordered_map<std::string, T, string_hash, MyEqual >;
};


