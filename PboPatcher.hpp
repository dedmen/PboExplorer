#pragma once
#include <shared_mutex>
#include <utility>

#include "lib/ArmaPboLib/src/pbo.hpp"

class PboPatcher;
class IPatchOperation;


enum class PatchType {
    Add,
    Update,
    Delete
};

class IPatchOperation {
    PatchType type;
public:
    IPatchOperation(PatchType type) : type(type) {}
    virtual ~IPatchOperation() = default;

    [[nodiscard]] PatchType GetType() const { return type; }

    virtual void Process(PboPatcher& patcher) = 0;
};

class PatchAddFileFromDisk final : public IPatchOperation {
    std::filesystem::path inputFile;
    std::filesystem::path pboFilePath;
public:
    PatchAddFileFromDisk(std::filesystem::path inputFile, std::filesystem::path pboFilePath) : IPatchOperation(PatchType::Add), inputFile(std::move(inputFile)), pboFilePath(std::move(pboFilePath)) {}
    void Process(PboPatcher& patcher) override;
};

class PatchUpdateFileFromDisk final : public IPatchOperation {
    std::filesystem::path inputFile;
    std::filesystem::path pboFilePath;
public:
    PatchUpdateFileFromDisk(std::filesystem::path inputFile, std::filesystem::path pboFilePath) : IPatchOperation(PatchType::Update), inputFile(std::move(inputFile)), pboFilePath(std::move(pboFilePath)) {}
    void Process(PboPatcher& patcher) override;
};

class PatchRenameFile final : public IPatchOperation {
    std::filesystem::path pboFilePathOld;
    std::filesystem::path pboFilePathNew;
public:
    PatchRenameFile(std::filesystem::path pboFilePathOld, std::filesystem::path pboFilePathNew) : IPatchOperation(PatchType::Update), pboFilePathOld(std::move(pboFilePathOld)), pboFilePathNew(std::move(pboFilePathNew)) {}
    void Process(PboPatcher& patcher) override;
};

class PatchDeleteFile final : public IPatchOperation {
    std::filesystem::path pboFilePath;
public:
    PatchDeleteFile(std::filesystem::path pboFilePath) : IPatchOperation(PatchType::Delete), pboFilePath(std::move(pboFilePath)) {}
    void Process(PboPatcher& patcher) override;
};


class PatchAddProperty final : public IPatchOperation {
    std::pair<std::string, std::string> property;
public:
    PatchAddProperty(std::pair<std::string, std::string> property) : IPatchOperation(PatchType::Add), property(std::move(property)) {}
    void Process(PboPatcher& patcher) override;
};

// Add will overwrite if key already exists, currently
using PatchUpdateProperty = PatchAddProperty;

class PatchDeleteProperty final : public IPatchOperation {
    std::string propertyName;
public:
    PatchDeleteProperty(std::string propertyName) : IPatchOperation(PatchType::Delete), propertyName(std::move(propertyName)) {}
    void Process(PboPatcher& patcher) override;
};

template<typename T>
concept PatchOperation = std::derived_from<T, IPatchOperation>;


class PboPatcher
{
    friend class IPatchOperation;
    friend class PatchAddFileFromDisk;
    friend class PatchUpdateFileFromDisk;
    friend class PatchRenameFile;
    friend class PatchDeleteFile;
    friend class PatchAddProperty;
    friend class PatchDeleteProperty;

    // active data read from input file, will be modified by patch operations before used to target output file
    std::vector<PboEntry> files;
    std::vector<PboProperty> properties;
    std::vector<std::shared_ptr<PboFileToWrite>> filesToWrite;
    // mutex protecting against filesToWrite iterator invalidation
    std::shared_mutex ftwMutex;
    std::mutex propertyMutex;
    // start offset if you were to insert a new file at the end
    uint32_t endStartOffset = 0;

    PatchType currentPatchStep = PatchType::Add;

    std::vector<std::unique_ptr<IPatchOperation>> patches;

    // is only valid at ReadInputFile and ProcessPatches();
    const PboReader* readerRef;

    // move a file inside pbo from wherever to its end and create dummy space in old slot
    void MoveFileToEnd(std::filesystem::path file, uint32_t indexHint = std::numeric_limits<uint32_t>::max());

    // move a file inside pbo from wherever to a dummy space further towards the end of the file
    void MoveFileToEndOrDummy(std::filesystem::path file, uint32_t indexHint = std::numeric_limits<uint32_t>::max());

    // Deletes a file from filesToWrite by replacing it with a dummy
    void ReplaceWithDummyFile(std::filesystem::path file, uint32_t indexHint = std::numeric_limits<uint32_t>::max());

    // you need to hold a shared ftwMutex lock when calling this
    [[nodiscard]] decltype(filesToWrite)::iterator GetFTWIteratorToFile(const std::filesystem::path& file, uint32_t indexHint = std::numeric_limits<uint32_t>::max());

    [[nodiscard]] bool CheckFTWIteratorValid(decltype(PboPatcher::filesToWrite)::iterator);

    [[nodiscard]] decltype(filesToWrite)::iterator FindFreeSpace(uint32_t size);

    [[nodiscard]] decltype(filesToWrite)::iterator FindFreeSpaceAfter(uint32_t size, uint32_t startOffset);

    [[nodiscard]] bool InsertFileIntoDummySpace(std::shared_ptr<PboFileToWrite> newFile);

    static std::filesystem::path GetNewDummyFileName(uint32_t startOffset);

    // convert a NoTouch file to a in-memory string buffer file
    std::shared_ptr<PboFileToWrite> ConvertNoTouchFile(std::shared_ptr<PboFileToWrite> input);


public:
    // apply file writelock to current process
    // open and parseHeaders PboReader
    // ReadInputFile
    // ProcessPatches
    // close PboReader
    // WriteOutputFile


    PboPatcher() {}



    template<PatchOperation T, typename ... Args>
    void AddPatch(Args&& ...args) {
        patches.emplace_back(new T(std::forward<Args>(args)...));
    }

    void ReadInputFile(const PboReader* inputFile);

    // process patch operations, PboReader needs to be valid still
    void ProcessPatches();

    // write to target
    void WriteOutputFile(std::iostream& outputStream);
};

