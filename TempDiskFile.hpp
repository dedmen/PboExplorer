#pragma once
#include <filesystem>

class PboFile;

class TempDiskFile
{
	// When we created out tempfile, if modifDate is newer than this, it was modified
	std::filesystem::file_time_type creationTime;
	uint64_t creationFileSize;
	uint64_t creationHash;

	std::filesystem::path filePath;
	std::filesystem::path pboSubPath;
	std::filesystem::path pboPath;

	uint64_t GetCurrentHash();
	uint64_t GetCurrentSize() const;
	std::filesystem::file_time_type GetCurrentModtime() const;

public:
	TempDiskFile(const PboFile& pboRef, std::filesystem::path subfile);
	~TempDiskFile();

	bool WasModified();
	auto GetPath() const { return filePath; }

	void PatchToOrigin();


    static std::shared_ptr<TempDiskFile> GetFile(const PboFile& pboRef, std::filesystem::path subfile);

};

