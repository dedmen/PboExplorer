#pragma once
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
    std::filesystem::path GetFilePath() const {
        auto pathLen = cb - sizeof(PboPidl);
        return std::filesystem::path((wchar_t*)filePath);
    }

    bool IsValidPidl() const {
        return cb + packMask == 0xBEEF;
    }

    static bool IsValidPidl(const void* address) {
        return ((PboPidl*)address)->IsValidPidl();
    }


    static uint16_t GetPidlSizeForPath(const std::filesystem::path& path) {
        return 
            sizeof(PboPidl) +
            (path.native().length() - 1) * sizeof(wchar_t);
    }

    static void CreatePidlAt(PboPidl* dst, const std::filesystem::path& path, PboPidlFileType fileType) {
        dst->cb = GetPidlSizeForPath(path);
        dst->packMask = 0xBEEF - dst->cb;
        dst->type = fileType;
        //dst->idx = idx;
        std::memcpy(dst->filePath, path.native().c_str(), path.native().length() * sizeof(wchar_t));
        dst->filePath[path.native().length()] = 0;
    }

};
