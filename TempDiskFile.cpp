
import TempDiskFile;

//#include "TempDiskFile.hpp"
//#define NOMINMAX
//#include <mutex>
//#include <unordered_map>
//#include <fstream>
//#include <Shlobj.h>
//#include "PboPatcherLocked.hpp"
//
//#include "Util.hpp"
//#include "DebugLogger.hpp"
//#include "FileWatcher.hpp"

#define NOMINMAX
#include <Shlobj.h>

#include "PboFileDirectory.hpp"
#include "PboFileStream.hpp"
#include "PboPatcherLocked.hpp"

import DebugLogger;
import FileWatcher;

import Encoding;
import Hashing;

std::mutex TempDiskFileCreationLock;
struct opt_path_hash {
    std::size_t operator()(const std::filesystem::path& path) const {
        return std::filesystem::hash_value(path);
    }
};

std::unordered_map<std::filesystem::path, std::weak_ptr<TempDiskFile>, opt_path_hash> TempFileDirectory;
std::mutex TempFileDirectoryLock;

uint64_t TempDiskFile::GetCurrentHash() const {
    std::ifstream inp(filePath, std::ifstream::binary | std::ifstream::in);

    return GetHashOf(inp);
}

uint64_t TempDiskFile::GetCurrentSize() const {
    return std::filesystem::file_size(filePath);
}

std::filesystem::file_time_type TempDiskFile::GetCurrentModtime() const {
    return std::filesystem::last_write_time(filePath);
}

uint64_t TempDiskFile::GetHashOf(std::istream& inp) {
    FNV1A_Hash result;
    std::array<char, 4096> buf;
    do {
        inp.read(buf.data(), buf.size());
        result.Add(reinterpret_cast<const uint8_t*>(buf.data()), inp.gcount());
    } while (inp.gcount() > 0);

    // This is cheating... FNV hash?
    return result.currentValue;
}


TempDiskFile::TempDiskFile(const PboFile& pboRef, std::filesystem::path subfile) {
    std::unique_lock lock(TempDiskFileCreationLock); //#TODO this should be a windows named mutex
    filePath = std::filesystem::temp_directory_path() / "PboExplorer" / pboRef.diskPath.filename() / subfile;
    pboPath = pboRef.diskPath;
    pboSubPath = subfile;

    try {
        std::filesystem::create_directories(filePath.parent_path());
    }
    catch (...) {
        //#TODO handle already exists properly
    }



    //#TODO properly handle existing file, this is bad. We should never do that
    {
        std::ifstream pboInputStream(pboRef.diskPath, std::ios::in | std::ios::binary);
        PboReader pboReader(pboInputStream);

        PboEntryBuffer fileBuffer(pboReader, *[&, filePath = subfile]()
            {
                pboReader.readHeaders();

                auto& pboFiles = pboReader.getFiles();
                const auto found = std::ranges::find_if(pboFiles, [&](const PboEntry& entry)
                    {
                        return UTF8::Decode(entry.name) == filePath;
                    });

                if (found == pboFiles.end())
                    __debugbreak();
                return found;
            }());


        bool wantWrite = true;

        std::istream sourceStream(&fileBuffer); // We need this for hash checking so we already open source here

        if (std::filesystem::exists(filePath)) // handle already existing file, don't update if we don't need
        {
            wantWrite = false;
            if (GetCurrentSize() != fileBuffer.GetEntryInfo().original_size) // size check is cheap
                wantWrite = true;
            else if (GetCurrentHash() != GetHashOf(sourceStream)) { // I rather read the file once than writing it needlessly, save SSD lifetime plox
                wantWrite = true;
                sourceStream.seekg(0);
                sourceStream.clear(); // reset fail state
                fileBuffer.seekpos(0, std::ios_base::out); // seek buffer back to start, oof this is more complicated than it should be really
            }
        }

        if (wantWrite)
        {
            std::ofstream outFile(filePath, std::ios::out | std::ios::binary);

            std::array<char, 4096> buf;
            do {
                sourceStream.read(buf.data(), buf.size());
                outFile.write(buf.data(), sourceStream.gcount());
            } while (sourceStream.gcount() > 0);
        }
    }
    //else __debugbreak();

    lock.unlock();

    creationFileSize = GetCurrentSize();
    creationHash = GetCurrentHash();
    creationTime = GetCurrentModtime();
    DebugLogger::TraceLog(std::format(L"Open Temp File {}", filePath.wstring()), std::source_location::current(), __FUNCTION__);
}

TempDiskFile::~TempDiskFile() {
    std::unique_lock lock(TempDiskFileCreationLock);
    std::error_code ec;
    DebugLogger::TraceLog(std::format(L"Delete Temp File {}", filePath.wstring()), std::source_location::current(), __FUNCTION__);


    GFileWatcher.UnwatchFile(filePath);
    std::filesystem::remove(filePath, ec);

    auto tempDir = std::filesystem::temp_directory_path() / "PboExplorer";

    // delete full tempdir if no more files left

    bool filesLeft = std::ranges::any_of(std::filesystem::recursive_directory_iterator(tempDir), [](const std::filesystem::directory_entry& p) {
        return p.is_regular_file();
        });

    if (!filesLeft)
        std::filesystem::remove_all(tempDir);

    lock.unlock();
    std::unique_lock lock2(TempFileDirectoryLock);
    auto found = TempFileDirectory.find(filePath);
    if (found != TempFileDirectory.end() && found->second.lock().get() == this || !found->second.lock())
        TempFileDirectory.erase(found);
}

bool TempDiskFile::WasModified() {
    return
        GetCurrentSize() != creationFileSize ||
        GetCurrentModtime() != creationTime ||
        GetCurrentHash() != creationHash
        ;
}

std::mutex DiskWriterLock;

void TempDiskFile::PatchToOrigin() {
    std::unique_lock lock(DiskWriterLock);
    if (!WasModified())
        return;

    {
        PboPatcherLocked patcher(GPboFileDirectory.GetPboFile(pboPath));
        patcher.AddPatch<PatchUpdateFileFromDisk>(filePath, pboSubPath);
    }


    auto absolutePath = pboPath / pboSubPath;
    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, absolutePath.wstring().c_str(), 0);

    creationFileSize = GetCurrentSize();
    creationHash = GetCurrentHash();
    creationTime = GetCurrentModtime();

}

std::shared_ptr<TempDiskFile> TempDiskFile::GetFile(const PboFile& pboRef, std::filesystem::path subfile) {
    std::unique_lock lock(TempFileDirectoryLock);
    auto targetPath = std::filesystem::temp_directory_path() / "PboExplorer" / pboRef.diskPath.filename() / subfile;
    auto found = TempFileDirectory.find(targetPath);

    if (found == TempFileDirectory.end()) {
        std::shared_ptr<TempDiskFile> newFile = std::make_shared<TempDiskFile>(pboRef, subfile);
        TempFileDirectory[targetPath] = newFile;
        //#TODO make this cleaner, take parameter for whether we want to keep-alive, don't do this trick to get non-const. Just set keepalive flag, and let PboFile destructor tell us to remove tempfiles
        pboRef.GetRootFile()->tempDiskFileKeepAlive.push_back(newFile);
        return newFile;
    }

    auto retFile = found->second.lock();
    if (!retFile) {
        retFile = std::make_shared<TempDiskFile>(pboRef, subfile);
        TempFileDirectory[targetPath] = retFile;
        return retFile;
    }

    return retFile;
}
