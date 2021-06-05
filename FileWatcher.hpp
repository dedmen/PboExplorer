#pragma once
#include <mutex>
#include <unordered_map>

#include "TempDiskFile.hpp"


class FileWatcher
{

	struct opt_path_hash {
		std::size_t operator()(const std::filesystem::path& path) const {
			return std::filesystem::hash_value(path);
		}
	};

	std::unordered_map<std::filesystem::path, std::shared_ptr<TempDiskFile>, opt_path_hash> watchedFiles;
	std::mutex mainMutex;

	std::thread thread;
	std::atomic_bool shouldRun = true;
	std::condition_variable wakeupEvent;

	//std::vector<void*> checkHandles;
	//std::vector<std::function(void())>

	void Run();

public:
	FileWatcher();
	~FileWatcher();

	void Startup();

	void WatchFile(std::shared_ptr<TempDiskFile> newFile);
	void UnwatchFile(std::filesystem::path file);
};


inline FileWatcher GFileWatcher;
