export module FileUtil;

import std;
import std.compat;
import Hashing;

export uint64_t GetFileHash(std::filesystem::path file)
{
    if (!std::filesystem::exists(file))
        return 0;

    FNV1A_Hash result;
    std::ifstream inp(file, std::ifstream::binary | std::ifstream::in);

    std::array<char, 4096> buf;
    do {
        inp.read(buf.data(), buf.size());
        result.Add(reinterpret_cast<const uint8_t*>(buf.data()), inp.gcount());
    } while (inp.gcount() > 0);

    return result.currentValue;
}

export std::string ReadWholeFile(std::filesystem::path file)
{
    if (!std::filesystem::exists(file))
        return {};

    std::string result;
    result.resize(std::filesystem::file_size(file));

    std::ifstream inp(file, std::ifstream::binary | std::ifstream::in);
    inp.read(result.data(), result.size());
    result.resize(inp.gcount());

    return result;
}
