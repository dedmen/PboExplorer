#pragma once
#include <filesystem>

enum class PboPidlFileType
{
	Folder,
	File
};

//#TODO total size needs to be 4B aligned
struct PboPidl
{
#pragma pack(push,1)
    uint16_t cb;
private:
    // used as a sanity check, cb + packMask always == 0xBEEF
    uint16_t packMask;
#pragma pack(pop)
public:
    PboPidlFileType type;
    //int idx;
    //std::filesystem::path filePath; //#TODO convert pidl to store filepath as dynamic length string, we cannot store std::filesystem::path pointer to disk
    //int dbgIdx;
private:
    wchar_t filePath[1];
public:

    bool IsFile() const { return type == PboPidlFileType::File; }
    bool IsFolder() const { return type == PboPidlFileType::Folder; }

    /// <summary>
    /// Returns the full path even if this is a Combined/multi-level pidl
    /// </summary>
    /// <returns></returns>
    std::filesystem::path GetFilePath() const {
        std::filesystem::path retPath;
        auto pidl = this;
        uint32_t pidlCount = 0;
        do {
            retPath /= pidl->GetFileName();

            pidl = reinterpret_cast<PboPidl*>(reinterpret_cast<uintptr_t>(pidl) + pidl->cb);
            ++pidlCount;
        } while (pidl->IsValidPidl());

        return retPath;
    }

    /// <summary>
    /// Returns number of levels how deep this pidl is
    /// </summary>
    uint32_t GetLevelCount() const {
        auto pidl = this;
        uint32_t pidlCount = 0;
        do {
            pidl = reinterpret_cast<PboPidl*>(reinterpret_cast<uintptr_t>(pidl) + pidl->cb);
            ++pidlCount;
        } while (pidl->IsValidPidl());

        return pidlCount;
    }

    /// <summary>
    /// Gets the name of the current pidl, if this is a Combined/multi-level pidl it only returns the name of the current one!
    /// </summary>
    std::filesystem::path GetFileName() const {
        auto pathLen = cb - sizeof(PboPidl);
        return { std::wstring_view((const wchar_t*)filePath) };
    }

    bool IsValidPidl() const {
        return cb + packMask == 0xBEEF && cb != 0;
    }

    static bool IsValidPidl(const void* address) {
        return ((PboPidl*)address)->IsValidPidl();
    }


    static uint16_t GetPidlSizeForPath(const std::filesystem::path& path) {
        return 
            sizeof(PboPidl) +
            (path.native().length() - 1) * sizeof(wchar_t);
    }

    /// <summary>
    /// Fills in a pidl into dst buffer
    /// </summary>
    /// <param name="dst">Buffer where pidl will be created in</param>
    /// <param name="path">Path of the current pidl element</param>
    /// <param name="fileType">Type of the current pidl element</param>
    /// <returns>Pointer to 1 past end in case of creating a multi-level pidl</returns>
    static PboPidl* CreatePidlAt(PboPidl* dst, const std::filesystem::path& path, PboPidlFileType fileType) {
        dst->cb = GetPidlSizeForPath(path);
        dst->packMask = 0xBEEF - dst->cb;
        dst->type = fileType;
        //dst->idx = idx;
        std::memcpy(dst->filePath, path.native().c_str(), path.native().length() * sizeof(wchar_t));
        dst->filePath[path.native().length()] = 0;
        return reinterpret_cast<PboPidl*>(reinterpret_cast<uintptr_t>(dst) + dst->cb);
    }

};
