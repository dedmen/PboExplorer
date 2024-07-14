#pragma once

import std;
import std.compat;

import PboPidl;

class IPboSub
{
public:
    std::wstring filename;
    /// <summary>
    /// Full path relative to pbo root
    /// </summary>
    /// <example>config.cpp</example>
    /// <example>functions/fnc_init.sqf</example>
    /// <example>UI/textures/icon_co.paa</example>
    std::filesystem::path fullPath;


    // Iterates through all subfiles/folders (if current object is a file, only runs that one file, if its a folder, iterates through all subfolders)
    virtual void ForEachFileOrFolder(std::function<void(const IPboSub*)> func, bool recursive = false) const = 0;
};

class PboSubFile;
class PboSubFolder;
class PboFile;
class TempDiskFile;

/// <summary>
/// Interface for a directory inside a pbo. It can either be the root (the pbo itself is a directory) or a subdirectory.
/// 
/// If subdirectory, then selfRelPath is not empty
/// </summary>
class IPboFolder {
public:
    /// <summary>
    /// Retrieves path of the Pbo file on disk
    /// </summary>
    /// <returns></returns>
    virtual const std::filesystem::path& GetPboDiskPath() const = 0;

    /// <summary>
    /// Retrieve this folders "root" folder. Can either be overall Pbo root, or a subdirectory
    /// </summary>
    /// <returns></returns>
    virtual std::shared_ptr<PboSubFolder> GetFolder() const = 0;


    virtual std::shared_ptr<PboFile> GetRootFile() const = 0;

    virtual std::optional<std::reference_wrapper<const PboSubFile>> GetFileByPath(std::filesystem::path inputPath) const = 0;
    virtual std::shared_ptr<PboSubFolder> GetFolderByPath(std::filesystem::path inputPath) const = 0;
    /// <summary>
    /// Returns A multi-level pidl for inputPath, relative to current folder
    /// </summary>
    virtual std::unique_ptr<PboPidl> GetPidlListFromPath(std::filesystem::path inputPath) const = 0;
};


class PboSubFile final : public IPboSub
{
public:
    uint32_t filesize;
    uint32_t dataSize;
    uint32_t startOffset;

    // Inherited via IPboSub
    void ForEachFileOrFolder(std::function<void(const IPboSub*)> func, bool recursive = false) const override { func(this); }
};

class PboSubFolder final : public IPboSub, public IPboFolder, public std::enable_shared_from_this<PboSubFolder>
{
public:
    std::vector<PboSubFile> subfiles;
    std::vector<std::shared_ptr<PboSubFolder>> subfolders;
    std::weak_ptr<PboFile> rootFile;

    // Inherited via IPboFolder
    const std::filesystem::path& GetPboDiskPath() const override;
    std::shared_ptr<PboSubFolder> GetFolder() const override;
    std::shared_ptr<PboFile> GetRootFile() const override;
    std::optional<std::reference_wrapper<const PboSubFile>> GetFileByPath(std::filesystem::path inputPath) const override;
    std::shared_ptr<PboSubFolder> GetFolderByPath(std::filesystem::path inputPath) const override;
    std::unique_ptr<PboPidl> GetPidlListFromPath(std::filesystem::path inputPath) const override;

    // Inherited via IPboSub
    void ForEachFileOrFolder(std::function<void(const IPboSub*)> func, bool recursive = false) const override { //#TODO make this better, accept flags files,folders,recursive and return a range
        func(this);

        if (recursive)
            for (auto& it : subfolders)
                it->ForEachFileOrFolder(func, recursive);
        else
            for (auto& it : subfolders)
                func(it.get());
        

        for (auto& it : subfiles)
            func(&it);

    }
};

class PboFile final : public IPboFolder, public std::enable_shared_from_this<PboFile>
{
public:
    PboFile();
    std::filesystem::path diskPath;
    std::shared_ptr<PboSubFolder> rootFolder;

    void ReadFrom(std::filesystem::path inputPath);
    void ReloadFrom(std::filesystem::path inputPath);


    // Inherited via IPboFolder
    const std::filesystem::path& GetPboDiskPath() const override;
    std::shared_ptr<PboSubFolder> GetFolder() const override;

    std::shared_ptr<PboFile> GetRootFile() const override;
    std::optional<std::reference_wrapper<const PboSubFile>> GetFileByPath(std::filesystem::path inputPath) const override;
    std::shared_ptr<PboSubFolder> GetFolderByPath(std::filesystem::path inputPath) const override;
    std::unique_ptr<PboPidl> GetPidlListFromPath(std::filesystem::path inputPath) const override;

    //#TODO make this cleaner?
    std::vector<std::shared_ptr<TempDiskFile>> tempDiskFileKeepAlive;


    std::vector<std::pair<std::string, std::string>> properties;
};



/// <summary>
/// Use this when storing a PboSubFolder, for example in PboFolder class for subfolders. It keeps the root PboFile alive, otherwise PboSubFolder only has a weak ref
/// </summary>
class PboSubFolderActiveRef : public IPboFolder {
    std::shared_ptr<PboSubFolder> folder;
    std::shared_ptr<PboFile> rootFile;
public:

    PboSubFolderActiveRef(std::shared_ptr<PboSubFolder> subFolder);

    PboSubFolder* operator->() {
        return folder.get();
    }

    // Inherited via IPboFolder
    const std::filesystem::path& GetPboDiskPath() const override { return rootFile->diskPath; };
    std::shared_ptr<PboSubFolder> GetFolder() const override { return folder; };
    std::shared_ptr<PboFile> GetRootFile() const override { return rootFile; };
    std::optional<std::reference_wrapper<const PboSubFile>> GetFileByPath(std::filesystem::path inputPath) const override { return folder->GetFileByPath(inputPath); };
    std::shared_ptr<PboSubFolder> GetFolderByPath(std::filesystem::path inputPath) const override { return folder->GetFolderByPath(inputPath); };
    std::unique_ptr<PboPidl> GetPidlListFromPath(std::filesystem::path inputPath) const override { return folder->GetPidlListFromPath(inputPath); };
};




class PboFileDirectory
{
    std::unordered_map<std::wstring, std::weak_ptr<PboFile>> openFiles;
    std::mutex openLock;
public:

    std::shared_ptr<PboFile> GetPboFile(std::filesystem::path path);



    void AcquireGlobalPatchLock();
    void ReleaseGlobalPatchLock();

};

inline PboFileDirectory GPboFileDirectory;

