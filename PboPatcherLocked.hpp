#pragma once

#include "PboPatcher.hpp"
#include "PboFileDirectory.hpp"

class PboPatcherLocked {
	std::shared_ptr<PboFile> pboFile;
	PboPatcher patcher;
    std::ifstream readStream;
    std::optional<PboReader> reader;
public:

	PboPatcherLocked(std::shared_ptr<PboFile> file) : pboFile(file) {
        GPboFileDirectory.AcquireGlobalPatchLock();
        readStream = std::ifstream(pboFile->diskPath, std::ifstream::in | std::ifstream::binary);
        reader.emplace(readStream);

        reader->readHeaders();
        patcher.ReadInputFile(&reader.value());
	}

    ~PboPatcherLocked() {
        patcher.ProcessPatches();

        reader = std::nullopt;
        readStream.close();

        {
            std::fstream outputStream(pboFile->diskPath, std::fstream::binary | std::fstream::in | std::fstream::out);
            patcher.WriteOutputFile(outputStream);
        }
        GPboFileDirectory.ReleaseGlobalPatchLock();

        //#TODO this is unsafe, can be called from different threads while something else is working on the pbo. I guess we want to lock the filelists?
        pboFile->ReloadFrom(pboFile->diskPath);
    }

    template<PatchOperation T, typename ... Args>
    void AddPatch(Args&& ...args) {
        patcher.AddPatch<T, Args...>(std::forward<Args>(args)...);
    }
};