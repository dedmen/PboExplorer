export module Hashing;

import std;
import std.compat;

export class FNV1A_Hash {
    static constexpr uint64_t FNV_PRIME = 0x100000001b3;
    static constexpr uint64_t OFFSET_BASIS = 0xcbf29ce484222325;
public:

    uint64_t currentValue = OFFSET_BASIS;

    static constexpr uint64_t Add(uint64_t& hashValue, const uint8_t* data, size_t dataLength) noexcept {
        const auto dataEnd = data + dataLength;

        for (; data < dataEnd; data++)
            hashValue = (hashValue ^ *data) * FNV_PRIME;

        return hashValue;
    }

    template<std::integral T>
    constexpr void Add(T d) noexcept
    {
        auto asArray = std::bit_cast<std::array<uint8_t, sizeof(T)>>(d);

        for (const auto& value : asArray)
        {
            currentValue = currentValue ^ value;
            currentValue *= FNV_PRIME;
        }
    }

    constexpr void Add(const uint8_t* data, size_t dataLength) noexcept {
        Add(currentValue, data, dataLength);
    }

    constexpr void AddString(std::string_view str) noexcept {
        Add(currentValue, reinterpret_cast<const uint8_t*>(str.data()), str.length());
    }
};
