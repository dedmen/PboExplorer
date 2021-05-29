#pragma once
#include "ClassFactory.hpp"

enum class PboPidlFileType
{
	Folder,
	File
};


struct PboPidl
{
    USHORT cb;
    PboPidlFileType type;
    //int idx;
    std::filesystem::path filePath;
    int dbgIdx;

    bool IsFile() const { return type == PboPidlFileType::File; }
    bool IsFolder() const { return type == PboPidlFileType::Folder; }	
};
