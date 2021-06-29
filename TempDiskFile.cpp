#include "TempDiskFile.hpp"

#include <mutex>
#include <unordered_map>
#include <fstream>
#include "PboPatcherLocked.hpp"

#include "Util.hpp"


extern "C" {
#include "lib/ArmaPboLib/lib/sha1.h"
}

std::mutex TempDiskFileCreationLock;
struct opt_path_hash {
    std::size_t operator()(const std::filesystem::path& path) const {
        return std::filesystem::hash_value(path);
    }
};

std::unordered_map<std::filesystem::path, std::weak_ptr<TempDiskFile>, opt_path_hash> TempFileDirectory;
std::mutex TempFileDirectoryLock;

uint64_t TempDiskFile::GetCurrentHash() {
    SHA1Context context;
    SHA1Reset(&context);

    std::ifstream inp(filePath, std::ifstream::binary | std::ifstream::in);

    std::array<char, 4096> buf;
    do {
        inp.read(buf.data(), buf.size());
        SHA1Input(&context, reinterpret_cast<const unsigned char*>(buf.data()), inp.gcount());
    } while (inp.gcount() > 0);

    // This is cheating... FNV hash?
    return *reinterpret_cast<uint64_t*>(&context.Message_Digest);
}

uint64_t TempDiskFile::GetCurrentSize() const {
    return std::filesystem::file_size(filePath);
}

std::filesystem::file_time_type TempDiskFile::GetCurrentModtime() const {
    return std::filesystem::last_write_time(filePath);
}


TempDiskFile::TempDiskFile(const PboFile& pboRef, std::filesystem::path subfile) {
    std::unique_lock lock(TempDiskFileCreationLock);
    filePath = std::filesystem::temp_directory_path() / "PboExplorer" / pboRef.diskPath.filename() / subfile;
    pboPath = pboRef.diskPath;
    pboSubPath = subfile;

    std::filesystem::create_directories(filePath.parent_path());

    //#TODO properly handle existing file, this is bad. We should never do that
    if (!std::filesystem::exists(filePath))
    {
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);


        std::ifstream pboInputStream(pboRef.diskPath, std::ios::in | std::ios::binary);
        PboReader pboReader(pboInputStream);


        PboEntryBuffer fileBuffer(pboReader, *[&, filePath = subfile]()
        {
            pboReader.readHeaders();

            auto& pboFiles = pboReader.getFiles();
            const auto found = std::ranges::find_if(pboFiles, [&](const PboEntry& entry)
                {
                    return Util::utf8_decode(entry.name) == filePath;
                });

            if (found == pboFiles.end())
                __debugbreak();
            return found;
        }());
        std::istream sourceStream(&fileBuffer);


        std::array<char, 4096> buf;
        do {
            sourceStream.read(buf.data(), buf.size());
            outFile.write(buf.data(), sourceStream.gcount());
        } while (sourceStream.gcount() > 0);
    }
    //else __debugbreak();

    lock.unlock();

    creationFileSize = GetCurrentSize();
    creationHash = GetCurrentHash();
    creationTime = GetCurrentModtime();
}

TempDiskFile::~TempDiskFile() {
    std::unique_lock lock(TempDiskFileCreationLock);
    std::error_code ec;
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
    //#TODO this crashes, because the Pidl's store filepath as std::filesystem::path, which we can't do (they need to be strings fully contained inside the pidl) we first need to refactor pidl before we can do this
    //SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, absolutePath.wstring().c_str(), 0);

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
