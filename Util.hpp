#pragma once
#define NOMINMAX
import <algorithm>;
import <array>;
import <filesystem>;
import <string>;
import <stdexcept>;
import <unordered_map>;

import Version;

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


    /// <summary>
    /// Uses Restart Manager to retrieve which processes are holding a write lock on file
    /// </summary>
    /// <param name="file"></param>
    /// <returns>List of process names (human readable and exe name)</returns>
    std::vector<std::wstring> GetProcessesThatLockFile(std::filesystem::path file);

};

#define TRY_ASSERT(x) {if (!(x)) Util::TryDebugBreak();}
