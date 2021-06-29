#pragma once
enum class PboPidlFileType
{
	Folder,
	File
};

//#TODO total size needs to be 4B aligned
struct PboPidl
{
    uint16_t cb;
    PboPidlFileType type;
    //int idx;
    std::filesystem::path filePath; //#TODO convert pidl to store filepath as dynamic length string, we cannot store std::filesystem::path pointer to disk
    int dbgIdx;

    bool IsFile() const { return type == PboPidlFileType::File; }
    bool IsFolder() const { return type == PboPidlFileType::Folder; }	
};
