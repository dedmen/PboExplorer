#include "PboFileDirectory.hpp"
#include "Util.hpp"

import <fstream>;
import <functional>;
import <numeric>;
import <unordered_set>;

#include <windows.h>

import Encoding;
import Tracy;
import PboPatcher;
import PboLib;

const std::filesystem::path& PboSubFolder::GetPboDiskPath() const
{
    return GetRootFile()->diskPath;
}

std::shared_ptr<PboSubFolder> PboSubFolder::GetFolder() const
{
    return  std::const_pointer_cast<PboSubFolder>(shared_from_this());
}

std::shared_ptr<PboFile> PboSubFolder::GetRootFile() const
{
    auto lockedRoot = rootFile.lock();

    if (!lockedRoot) { // should never happen
        Util::WaitForDebuggerPrompt("PboSubFolder, locked root is null");
        Util::TryDebugBreak();
    }

    return lockedRoot;
}

std::optional<std::reference_wrapper<const PboSubFile>> PboSubFolder::GetFileByPath(std::filesystem::path inputPath) const
{
    ProfilingScope pScope;
    auto relPath = inputPath;  //#TODO cleanup

    auto elementCount = std::distance(relPath.begin(), relPath.end());

    if (elementCount == 1) {
        // only a filename


        const auto subfileFound = std::ranges::find_if(subfiles, [relPath](const PboSubFile& subf)
        {
            return subf.filename == relPath;
        });

        if (subfileFound == subfiles.end())
            return {};

        return *subfileFound;
    }
    else {
        auto folderName = *relPath.begin();

        auto subfolderFound = std::find_if(subfolders.begin(), subfolders.end(), [folderName](const std::shared_ptr<PboSubFolder>& subf)
            {
                return subf->filename == folderName;
            });

        if (subfolderFound != subfolders.end())
        {
            return (*subfolderFound)->GetFileByPath(inputPath);
        }
    }


    return {};
}

std::shared_ptr<PboSubFolder> PboSubFolder::GetFolderByPath(std::filesystem::path inputPath) const
{
    ProfilingScope pScope;
    std::shared_ptr<PboSubFolder> curFolder = std::const_pointer_cast<PboSubFolder>(shared_from_this());

    // get proper root directory in case we are a subfolder inside a pbo
    auto relPath = inputPath; //#TODO cleanup

    for (auto& it : relPath)
    {
        auto subfolderFound = std::ranges::find_if(curFolder->subfolders, [it](const std::shared_ptr<PboSubFolder>& subf)
        {
            return subf->filename == it;
        });

        if (subfolderFound != curFolder->subfolders.end())
        {
            curFolder = *subfolderFound;
        }
    }
    return curFolder;
}

std::unique_ptr<PboPidl> PboSubFolder::GetPidlListFromPath(std::filesystem::path inputPath) const //#TODO take path by const ref, we are copying here
{
    ProfilingScope pScope;
    struct TempPidl {
        PboPidlFileType type;
        std::filesystem::path fileName;
    };

    std::vector<TempPidl> resultPidl;

    std::shared_ptr<PboSubFolder> curFolder = std::const_pointer_cast<PboSubFolder>(shared_from_this());
    auto relPath = inputPath;

    for (auto& it : relPath)
    {
        auto subfolderFound = std::ranges::find_if(curFolder->subfolders, [it](const std::shared_ptr<PboSubFolder>& subf)
        {
            return subf->filename == it;
        });

        if (subfolderFound != curFolder->subfolders.end())
        {
            resultPidl.emplace_back(TempPidl{ PboPidlFileType::Folder, (*subfolderFound)->filename });
            curFolder = *subfolderFound;
            continue;
        }

        auto subfileFound = std::ranges::find_if(curFolder->subfiles, [it](const PboSubFile& subf)
        {
            return subf.filename == it;
        });


        if (subfileFound != curFolder->subfiles.end()) {
            resultPidl.emplace_back(TempPidl{ PboPidlFileType::File, (*subfileFound).filename });
            break; // file is always end of path
        } else {
            Util::TryDebugBreak(); // not found
            //
            // not found, pidl is still valid
            return nullptr;
        }
    }

    auto size = std::accumulate(resultPidl.begin(), resultPidl.end(), 0u, [](uint32_t size, const TempPidl& pidl) {
        return size + PboPidl::GetPidlSizeForPath(pidl.fileName);
    });

    size += 2; // 2 byte 0 cb as terminator
    //#TODO use real alloc here instead of this mess, destructor will call delete instead of delete[] anyway and this is messy, need to propagate the dealloc way into the unique_ptr?
    auto data = new PboPidl[(size / sizeof(PboPidl)) + 1];
    auto dataWrite = data;

    for (auto& it : resultPidl) {
        dataWrite = PboPidl::CreatePidlAt(data, it.fileName, it.type);
    }

    // null terminator
    dataWrite->cb = 0;

    return std::unique_ptr<PboPidl>(data);
}

PboFile::PboFile()
{
    rootFolder = std::make_shared<PboSubFolder>();
}

void PboFile::ReadFrom(std::filesystem::path inputPath)
{
    ProfilingScope pScope;
    diskPath = inputPath;
    std::ifstream readStream(inputPath, std::ios::in | std::ios::binary);

    PboReader reader(readStream);
    reader.readHeaders();
    std::vector<std::filesystem::path> segments;
    for (auto& it : reader.getFiles())
    {
        if (it.name.starts_with("$DU")) // ignore dummy spacer files
            continue;
        std::filesystem::path filePath(UTF8::Decode(it.name));
        segments.clear();

        for (auto& it : filePath)
        {
            segments.emplace_back(it);
        }

        auto fileName = segments.back();
        segments.pop_back();

        std::shared_ptr<PboSubFolder> curFolder = rootFolder;
        rootFolder->rootFile = weak_from_this();
        for (auto& it : segments)
        {
            auto subfolderFound = std::ranges::find_if(curFolder->subfolders, [it](const std::shared_ptr<PboSubFolder>& subf)
            {
                return subf->filename == it;
            });

            if (subfolderFound != curFolder->subfolders.end())
            {
                curFolder = *subfolderFound;
                continue;
            }

            auto newSub = std::make_shared<PboSubFolder>();
            newSub->filename = it;
            newSub->fullPath = curFolder->fullPath / it;
            newSub->rootFile = weak_from_this();
            curFolder->subfolders.emplace_back(std::move(newSub));
            curFolder = curFolder->subfolders.back();
        }

        PboSubFile newFile;
        newFile.filename = fileName;
        newFile.filesize = it.original_size;
        newFile.dataSize = it.data_size;
        newFile.startOffset = it.startOffset;
        newFile.fullPath = filePath;
        curFolder->subfiles.emplace_back(std::move(newFile));
    }

    properties.clear();
    for (auto& it : reader.getProperties())
        properties.emplace_back(it.key, it.value);
}

void PboFile::ReloadFrom(std::filesystem::path inputPath)
{
    ProfilingScope pScope;
    if (inputPath != diskPath)
        Util::TryDebugBreak();

    std::ifstream readStream(inputPath, std::ios::in | std::ios::binary);

    PboReader reader(readStream);
    reader.readHeaders();


    // add new files, update existing files
    std::vector<std::filesystem::path> segments;

    std::unordered_set<std::wstring> existingFiles;

    for (auto& it : reader.getFiles())
    {
        if (it.name.starts_with("$DU")) // ignore dummy spacer files
            continue;
        existingFiles.emplace(UTF8::Decode(it.name));

        std::filesystem::path filePath(UTF8::Decode(it.name));
        segments.clear();

        for (auto& it : filePath)
        {
            segments.emplace_back(it);
        }

        auto fileName = segments.back();
        segments.pop_back();

        std::shared_ptr<PboSubFolder> curFolder = rootFolder;
        for (auto& it : segments)
        {
            auto subfolderFound = std::ranges::find_if(curFolder->subfolders, [it](const std::shared_ptr<PboSubFolder>& subf)
            {
                return subf->filename == it;
            });

            if (subfolderFound != curFolder->subfolders.end())
            {
                curFolder = *subfolderFound;
                continue;
            }
            auto newSub = std::make_shared<PboSubFolder>();
            newSub->filename = it;
            newSub->fullPath = filePath.parent_path();
            newSub->rootFile = weak_from_this();
            curFolder->subfolders.emplace_back(std::move(newSub));
            curFolder = curFolder->subfolders.back();
        }

        auto subfileFound = std::ranges::find_if(curFolder->subfiles, [&fileName](const PboSubFile& subf)
        {
            return subf.filename == fileName;
        });

        if (subfileFound == curFolder->subfiles.end()) {
            // add new file
            PboSubFile newFile; //#TODO constructor from entryInfo?
            newFile.filename = fileName;
            newFile.filesize = it.original_size;
            newFile.dataSize = it.data_size;
            newFile.startOffset = it.startOffset;
            newFile.fullPath = filePath;
            curFolder->subfiles.emplace_back(std::move(newFile));
        }
        else
        {
            // update old file with new info
            subfileFound->dataSize = it.data_size;
            subfileFound->filesize = it.original_size;
            subfileFound->startOffset = it.startOffset;
        }
    }

    properties.clear();
    for (auto& it : reader.getProperties())
        properties.emplace_back(it.key, it.value);


    // remove deleted files
    std::function<void(std::shared_ptr<PboSubFolder>&)> cleanupFolder = [&](std::shared_ptr<PboSubFolder>& folder) {

        for (auto& it : folder->subfolders)
            cleanupFolder(it);

        folder->subfiles.erase(
            std::ranges::remove_if(folder->subfiles, [&](const PboSubFile& file) {
                //#TODO case insensitive
                return existingFiles.find(file.fullPath.wstring()) == existingFiles.end();
            }).begin(),
            folder->subfiles.end()
        );

        folder->subfolders.erase(
            std::ranges::remove_if(folder->subfolders, [&](const std::shared_ptr<PboSubFolder>& folder) {
                return folder->subfiles.empty() && folder->subfolders.empty();
                }).begin(),
            folder->subfolders.end()
        );

    };

    cleanupFolder(rootFolder);

}

const std::filesystem::path& PboFile::GetPboDiskPath() const
{
    return diskPath;
}

std::shared_ptr<PboSubFolder> PboFile::GetFolder() const
{
    return rootFolder;
}


std::shared_ptr<PboFile> PboFile::GetRootFile() const
{
    return std::const_pointer_cast<PboFile>(shared_from_this());
}

//#TODO We need to make PboFile's re-scan if their origin pbo was repacked by FileWatcher
std::optional<std::reference_wrapper<const PboSubFile>> PboFile::GetFileByPath(std::filesystem::path inputPath) const
{
    ProfilingScope pScope;
    std::shared_ptr<PboSubFolder> curFolder = rootFolder;

    // get proper root directory in case we are a subfolder inside a pbo
    bool check = rootFolder->fullPath.empty();
    if (!check)
        Util::TryDebugBreak(); // lexically_relative moooost likely needs to be removed here, need check
    auto relPath = inputPath.lexically_relative(rootFolder->fullPath);

    for (auto& it : relPath)
    {
        auto subfolderFound = std::ranges::find_if(curFolder->subfolders, [it](const std::shared_ptr<PboSubFolder>& subf)
        {
            return subf->filename == it;
        });

        if (subfolderFound != curFolder->subfolders.end())
        {
            curFolder = *subfolderFound;
            continue;
        }

        const auto subfileFound = std::ranges::find_if(curFolder->subfiles, [it](const PboSubFile& subf)
        {
            return subf.filename == it;
        });

        if (subfileFound == curFolder->subfiles.end())
            return {};

        return *subfileFound;
    }
    return {};
}

std::shared_ptr<PboSubFolder> PboFile::GetFolderByPath(std::filesystem::path inputPath) const{
    return rootFolder->GetFolderByPath(inputPath);
}

std::unique_ptr<PboPidl> PboFile::GetPidlListFromPath(std::filesystem::path inputPath) const {
    return rootFolder->GetPidlListFromPath(inputPath);
}

PboSubFolderActiveRef::PboSubFolderActiveRef(std::shared_ptr<PboSubFolder> subFolder) : folder(subFolder), rootFile(subFolder->GetRootFile()) {
    if (!rootFile) { // should never happen
        Util::WaitForDebuggerPrompt("PboSubFolderActiveRef without rootfile");
        Util::TryDebugBreak();
    }
}

std::shared_ptr<PboFile> PboFileDirectory::GetPboFile(std::filesystem::path path)
{
    ProfilingScope pScope;
    std::unique_lock lck(openLock);
    auto found = openFiles.find(path.lexically_normal().wstring());

    std::shared_ptr<PboFile> result;

    if (found != openFiles.end() && !(result = found->second.lock())) {
        // entry existed, but has expired, remove the old entry
        openFiles.erase(found);
        found = openFiles.end();
    }

    if (found == openFiles.end()) {
        // pbo file doesn't exist yet, make a new one

        result = std::make_shared<PboFile>();
        result->ReadFrom(path);

        openFiles.insert({ path.lexically_normal().wstring(), result });
    }

    return result;
}

static HANDLE GMutex = nullptr;

void PboFileDirectory::AcquireGlobalPatchLock()
{
    if (!GMutex) {

        GMutex = CreateMutexA(
            NULL,    // default security attribute 
            FALSE,
            "Global\\PboExplorer_Patch"
        );
    }

    if (GMutex) WaitForSingleObject(GMutex, 0);
}

void PboFileDirectory::ReleaseGlobalPatchLock()
{

    if (GMutex) ReleaseMutex(GMutex);
}
