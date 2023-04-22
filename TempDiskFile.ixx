module;


export module TempDiskFile;

import <array>;
import <filesystem>;
import <mutex>;
import <unordered_map>;
import <fstream>;
import <source_location>;




extern "C++" class PboFile;

export class TempDiskFile
{
	// When we created out tempfile, if modifDate is newer than this, it was modified
	std::filesystem::file_time_type creationTime;
	uint64_t creationFileSize;
	uint64_t creationHash;

	std::filesystem::path filePath;
	std::filesystem::path pboSubPath;
	std::filesystem::path pboPath;

	uint64_t GetCurrentHash() const;
	uint64_t GetCurrentSize() const;
	std::filesystem::file_time_type GetCurrentModtime() const;
	static uint64_t GetHashOf(std::istream& stream);

public:
	TempDiskFile(const PboFile& pboRef, std::filesystem::path subfile);
	~TempDiskFile();

	bool WasModified();
	auto GetPath() const { return filePath; }
	auto GetPboSubPath() const { return pboSubPath; }

	void PatchToOrigin();


	static std::shared_ptr<TempDiskFile> GetFile(const PboFile& pboRef, std::filesystem::path subfile);

};


