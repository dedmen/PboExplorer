module;
#include <guiddef.h>

export module GUIDLookup;

import <algorithm>;
import <compare>;
import <string_view>;
import <vector>;

export std::strong_ordering operator <=> (const GUID& a, const GUID& b) noexcept {
    if (const auto c = a.Data1 <=> b.Data1; c != 0) return c;
    if (const auto c = a.Data2 <=> b.Data2; c != 0) return c;
    if (const auto c = a.Data3 <=> b.Data3; c != 0) return c;
    if (const auto c = a.Data4[0] <=> b.Data4[0]; c != 0) return c;
    if (const auto c = a.Data4[1] <=> b.Data4[1]; c != 0) return c;
    if (const auto c = a.Data4[2] <=> b.Data4[2]; c != 0) return c;
    if (const auto c = a.Data4[3] <=> b.Data4[3]; c != 0) return c;
    if (const auto c = a.Data4[4] <=> b.Data4[4]; c != 0) return c;
    if (const auto c = a.Data4[5] <=> b.Data4[5]; c != 0) return c;
    if (const auto c = a.Data4[6] <=> b.Data4[6]; c != 0) return c;
    return a.Data4[7] <=> b.Data4[7];
}




export template<typename GUIDT, typename ValueT>
class BaseGUIDLookup {
    using EntryT = std::pair<GUIDT, ValueT>;
    std::vector<EntryT> lookupList;
public:
    constexpr BaseGUIDLookup(std::initializer_list<EntryT> init) : lookupList(init) {
        std::sort(lookupList.begin(), lookupList.end(), [](const EntryT& a, const EntryT& b) {
            return a.first < b.first;
        });
    }
    //#TODO binary search, more modular, lookup template type by GUID. We can reuse for debug logger it uses unordered map for lookup
    // Could also use it in GetDetailsEx to look up a functor which will be called to process the request
    std::string_view GetName(const GUIDT& guid) {
        auto found = std::lower_bound(lookupList.begin(), lookupList.end(), guid, [](const EntryT& a, const GUIDT& b) {
            return a.first < b;
        });

        if (found == lookupList.end() || found->first != guid)
            return {};
        return found->second;
    }

    auto find(const GUIDT& guid) {
        auto found = std::lower_bound(lookupList.begin(), lookupList.end(), guid, [](const EntryT& a, const GUIDT& b) {
            return a.first < b;
        });

        if (found == lookupList.end() || found->first != guid)
            return lookupList.end();
        return found;
    }

    auto insert(EntryT newEntry) {
        // find position
        auto found = std::lower_bound(lookupList.begin(), lookupList.end(), newEntry.first, [](const EntryT& a, const GUIDT& b) {
            return a.first < b;
        });

        return lookupList.insert(found, std::move(newEntry));
    }

    auto begin() const {
        return lookupList.begin();
    }

    auto end() const {
        return lookupList.end();
    }


};

export template<typename ValueT>
using GUIDLookup = BaseGUIDLookup<GUID, ValueT>;