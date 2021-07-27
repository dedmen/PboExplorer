#include "PboFileDirectory.hpp"
#include "Util.hpp"
#include "PboPatcher.hpp"

#include <functional>
#include <unordered_set>

#include <windows.h>

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
        Util::WaitForDebuggerPrompt();
        Util::TryDebugBreak();
    }

    return lockedRoot;
}

std::optional<std::reference_wrapper<const PboSubFile>> PboSubFolder::GetFileByPath(std::filesystem::path inputPath) const
{
    auto relPath = inputPath.lexically_relative(fullPath);

    auto elementCount = std::distance(relPath.begin(), relPath.end());

    if (elementCount == 1) {
        // only a filename


        auto subfileFound = std::find_if(subfiles.begin(), subfiles.end(), [relPath](const PboSubFile& subf)
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
    std::shared_ptr<PboSubFolder> curFolder = std::const_pointer_cast<PboSubFolder>(shared_from_this());

    // get proper root directory in case we are a subfolder inside a pbo
    auto relPath = inputPath.lexically_relative(fullPath);

    for (auto& it : inputPath)
    {
        auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
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

std::unique_ptr<PboPidl> PboSubFolder::GetPidlListFromPath(std::filesystem::path inputPath) const
{
    std::vector<PboPidl> resultPidl;

    std::shared_ptr<PboSubFolder> curFolder = std::const_pointer_cast<PboSubFolder>(shared_from_this());

    // get proper root directory in case we are a subfolder inside a pbo
    auto relPath = inputPath.lexically_relative(fullPath);

    for (auto& it : relPath)
    {
        auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
            {
                return subf->filename == it;
            });

        if (subfolderFound != curFolder->subfolders.end())
        {
            // every pidl has fullpath, can't we just shorten this to 1 pidl, lets try...
            //resultPidl.emplace_back(PboPidl{ sizeof(PboPidl), PboPidlFileType::Folder, (*subfolderFound)->fullPath, -1 });
            curFolder = *subfolderFound;
            continue;
        }

        auto subfileFound = std::find_if(curFolder->subfiles.begin(), curFolder->subfiles.end(), [it](const PboSubFile& subf)
            {
                return subf.filename == it;
            });


        if (subfileFound != curFolder->subfiles.end()) {

            const auto& path = (*subfileFound).fullPath;

            auto data = new PboPidl[(PboPidl::GetPidlSizeForPath(path) / sizeof(PboPidl)) + 1];
            PboPidl::CreatePidlAt(data, path, PboPidlFileType::File);

            return std::unique_ptr<PboPidl>(data);
        }
    }


    if (curFolder->filename == relPath.filename()) {
        Util::TryDebugBreak();
        // return this folder as pidl, instead of returning a file pidl

    }

    Util::TryDebugBreak();
    // not found, pidl is still valid, just not for full path
    return nullptr;
}

PboFile::PboFile()
{
    rootFolder = std::make_shared<PboSubFolder>();
}

void PboFile::ReadFrom(std::filesystem::path inputPath)
{
    diskPath = inputPath;
    std::ifstream readStream(inputPath, std::ios::in | std::ios::binary);

    PboReader reader(readStream);
    reader.readHeaders();
    std::vector<std::filesystem::path> segments;
    for (auto& it : reader.getFiles())
    {
        if (it.name.starts_with("$DU")) // ignore dummy spacer files
            continue;
        std::filesystem::path filePath(Util::utf8_decode(it.name));
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
            auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
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
        existingFiles.emplace(Util::utf8_decode(it.name));

        std::filesystem::path filePath(Util::utf8_decode(it.name));
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
            auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
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
            curFolder->subfolders.emplace_back(std::move(newSub));
            curFolder = curFolder->subfolders.back();
        }

        auto subfileFound = std::find_if(curFolder->subfiles.begin(), curFolder->subfiles.end(), [&fileName](const PboSubFile& subf)
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
            std::remove_if(folder->subfiles.begin(), folder->subfiles.end(), [&](const PboSubFile& file) {
                //#TODO case insensitive
                return existingFiles.find(file.fullPath.wstring()) == existingFiles.end();
                }),
            folder->subfiles.end()
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
    std::shared_ptr<PboSubFolder> curFolder = rootFolder;

    // get proper root directory in case we are a subfolder inside a pbo
    auto relPath = inputPath.lexically_relative(rootFolder->fullPath);

    for (auto& it : relPath)
    {
        auto subfolderFound = std::find_if(curFolder->subfolders.begin(), curFolder->subfolders.end(), [it](const std::shared_ptr<PboSubFolder>& subf)
            {
                return subf->filename == it;
            });

        if (subfolderFound != curFolder->subfolders.end())
        {
            curFolder = *subfolderFound;
            continue;
        }

        auto subfileFound = std::find_if(curFolder->subfiles.begin(), curFolder->subfiles.end(), [it](const PboSubFile& subf)
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

PboSubFolderActiveRef::PboSubFolderActiveRef(std::shared_ptr<PboSubFolder> subFolder) : folder(folder), rootFile(folder->GetRootFile()) {
    if (!rootFile) { // should never happen
        Util::WaitForDebuggerPrompt();
        Util::TryDebugBreak();
    }
}

std::shared_ptr<PboFile> PboFileDirectory::GetPboFile(std::filesystem::path path)
{
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


