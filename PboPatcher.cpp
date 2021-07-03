#include "PboPatcher.hpp"
#include <execution>
#include "Util.hpp"

/*

    There are 3 types of patch operations for both pbo properties and files:
    - Add
    - Modify
    - Delete

    To make full file rewrites (which are expensive and slow) rare, there will be empty free spaces within the pbo, somewhat like how a memory allocator works.


    Deletes are processed first, to open up free spaces.
    Next Modifies are processed, if a file increases in size the file after it might need to be moved away (into a existing free space that fits, or at the end of the file)
    Last Additions are processed, trying to fit the files into the dummy spaces or at the end.

    Also need to pay attention that the pbo header has sufficient space, the first file after the header might need to be moved away to make room (which requires a dummy file to be added to start of header)

    At the end a pbo file will have quite some wasted space due to dummy files, but thats a acceptable compromise for the ability to edit the pbo without a full repack.
    Could also add a defragmentation step where files (largest first, smallest last) are moved around to fill free spaces and truncate some space at the end of the pbo,
    though you only want to do that the end when a user isn't making any more changes, which we cannot really know when that will be.


    During pbo writing there will be a new file reader type for both Null-Space (free space filled with nulls or whatever data) and a IgnoreFile writer that doesn't actually write to output, but reads the output into the SHA1 hasher while it skips over unpatched files.
    That way we only iterate through the file once at final write.

 
 */

void PboPatcher::MoveFileToEnd(std::filesystem::path file, uint32_t indexHint) {
    // get iterator to our file
    std::shared_lock ftwLockS(ftwMutex);
    auto foundFile = GetFTWIteratorToFile(file, indexHint);
    ftwLockS.unlock();

    std::unique_lock ftwLock(ftwMutex); // we will append, that can invalidate iterators

    // check if iterator is still valid since we released shared lock, grab a new one if it was invalidated
    if (!CheckFTWIteratorValid(foundFile) || (*foundFile)->getEntryInformation().name != file)
        foundFile = GetFTWIteratorToFile(file);

    //swap the existing entry with a free space, and put old entry at end

    auto newDummySpace = std::make_shared<PboFTW_DummySpace>(GetNewDummyFileName((*foundFile)->getEntryInformation().startOffset).string(), (*foundFile)->getEntryInformation());
    auto oldFile = ConvertNoTouchFile(*foundFile);

    *foundFile = newDummySpace;
    filesToWrite.emplace_back(oldFile);
    endStartOffset += oldFile->getEntryInformation().data_size;
}

void PboPatcher::MoveFileToEndOrDummy(std::filesystem::path file, uint32_t indexHint) {
    // get iterator to our file
    std::shared_lock ftwLockS(ftwMutex);
    auto foundFile = GetFTWIteratorToFile(file, indexHint);

    // is there a dummy space
    auto foundFreeSpace = FindFreeSpaceAfter((*foundFile)->getEntryInformation().data_size, (*foundFile)->getEntryInformation().startOffset);
    bool haveFreeSpace = foundFreeSpace != filesToWrite.end();
    // used to check if we still have the exact same dummy space later
    auto freeSpaceName = haveFreeSpace ? (*foundFreeSpace)->getEntryInformation().name : std::string();
    auto freeSpaceSize = haveFreeSpace ? (*foundFreeSpace)->getEntryInformation().data_size : 0;

    ftwLockS.unlock();
    std::unique_lock ftwLock(ftwMutex); // we will append, that can invalidate iterators

    // check if iterator is still valid since we released shared lock, grab a new one if it was invalidated
    if (!CheckFTWIteratorValid(foundFile) || (*foundFile)->getEntryInformation().name != file) {
        foundFile = GetFTWIteratorToFile(file);
    }

    if (haveFreeSpace && (!CheckFTWIteratorValid(foundFreeSpace) || (*foundFreeSpace)->getEntryInformation().name != freeSpaceName || (*foundFreeSpace)->getEntryInformation().data_size != freeSpaceSize)) {
        foundFreeSpace = FindFreeSpaceAfter((*foundFile)->getEntryInformation().data_size, (*foundFile)->getEntryInformation().startOffset);
        haveFreeSpace = foundFreeSpace != filesToWrite.end();
        freeSpaceSize = haveFreeSpace ? (*foundFreeSpace)->getEntryInformation().data_size : 0;
    }


    // put new dummy space into place of old file
    auto newDummySpace = std::make_shared<PboFTW_DummySpace>(GetNewDummyFileName((*foundFile)->getEntryInformation().startOffset).string(), (*foundFile)->getEntryInformation());
    auto oldFile = ConvertNoTouchFile(*foundFile);

    *foundFile = newDummySpace;


    if (!haveFreeSpace) {
        // no dummy space to insert, just append old file at end
        oldFile->getEntryInformation().startOffset = endStartOffset;
        filesToWrite.emplace_back(oldFile);
        endStartOffset += oldFile->getEntryInformation().data_size;
        return;
    }

    // insert into free space
    auto sizeLeftover = freeSpaceSize - oldFile->getEntryInformation().data_size;

    if (sizeLeftover) {
        // we have more space than we need. We insert our new file before the dummy space, and reduce the dummy spaces size

        (*foundFreeSpace)->getEntryInformation().data_size = sizeLeftover;
        (*foundFreeSpace)->getEntryInformation().original_size = sizeLeftover; // mikero weirdness
        oldFile->getEntryInformation().startOffset = (*foundFreeSpace)->getEntryInformation().startOffset;
        (*foundFreeSpace)->getEntryInformation().startOffset += oldFile->getEntryInformation().data_size;
        filesToWrite.insert(foundFreeSpace, oldFile);
    }
    else {
        // fits exactly, we can just replace our dummy space
        *foundFreeSpace = oldFile;
    }
}

void PboPatcher::ReplaceWithDummyFile(std::filesystem::path file, uint32_t indexHint) {

    // deletes are safe, noone will step on eachother and we won't invalidate iterators, we can skip locks
    if (currentPatchStep == PatchType::Delete) {
        auto foundFile = GetFTWIteratorToFile(file, indexHint);
        if (foundFile == filesToWrite.end()) {
            Util::TryDebugBreak();
            return;
        }

        auto newDummySpace = std::make_shared<PboFTW_DummySpace>(GetNewDummyFileName((*foundFile)->getEntryInformation().startOffset).string(), (*foundFile)->getEntryInformation());
        *foundFile = newDummySpace;
        return;
    }

    // get iterator to our file
    std::shared_lock ftwLockS(ftwMutex);
    auto foundFile = GetFTWIteratorToFile(file, indexHint);
    ftwLockS.unlock();

    std::unique_lock ftwLock(ftwMutex); // we will append, that can invalidate iterators

    // check if iterator is still valid since we released shared lock, grab a new one if it was invalidated
    if (!CheckFTWIteratorValid(foundFile) || (*foundFile)->getEntryInformation().name != file)
        foundFile = GetFTWIteratorToFile(file);

    //swap the existing entry with a free space

    auto newDummySpace = std::make_shared<PboFTW_DummySpace>(GetNewDummyFileName((*foundFile)->getEntryInformation().startOffset).string(), (*foundFile)->getEntryInformation());

    *foundFile = newDummySpace;
}

decltype(PboPatcher::filesToWrite)::iterator PboPatcher::GetFTWIteratorToFile(const std::filesystem::path& file, uint32_t indexHint) {
    auto foundFile = filesToWrite.end();

    {
        if (indexHint != std::numeric_limits<uint32_t>::max() && indexHint < filesToWrite.size()) {
            auto found = filesToWrite.begin() + indexHint;
            if ((*found)->getEntryInformation().name == file)
                foundFile = found;
        }

        if (foundFile == filesToWrite.end())
            do {
                std::shared_lock ftwLock(ftwMutex);

                foundFile = std::find_if(std::execution::par_unseq, filesToWrite.begin(), filesToWrite.end(), [&file](const std::shared_ptr<PboFileToWrite>& ftw) {
                    return ftw->getEntryInformation().name == file;
                });
            } while (foundFile != filesToWrite.end() && (*foundFile)->getEntryInformation().name != file);
    }

    return foundFile;
}

bool PboPatcher::CheckFTWIteratorValid(decltype(PboPatcher::filesToWrite)::iterator iterator) {
    return iterator >= filesToWrite.begin() && iterator < filesToWrite.end();
}

decltype(PboPatcher::filesToWrite)::iterator PboPatcher::FindFreeSpace(uint32_t size) {
    std::shared_lock ftwLockS(ftwMutex);
    //#TODO keep a freelist

    // prefer to find the smallest region that will fit our file

    auto found = std::min_element(std::execution::par_unseq, filesToWrite.begin(), filesToWrite.end(), [size](const std::shared_ptr<PboFileToWrite>& l, const std::shared_ptr<PboFileToWrite>& r) {

        if (!dynamic_cast<PboFTW_DummySpace*>(l.get()) || l->getEntryInformation().data_size < size)
            return false; // either not a dummy space, or too small. Rule it out by moving it to the right

        return l->getEntryInformation().data_size >= size < r->getEntryInformation().data_size;
    });


    //auto found = std::ranges::find_if(filesToWrite, [size](const std::shared_ptr<PboFileToWrite>& file) {
    //    return dynamic_cast<PboFTW_DummySpace*>(file.get()) && file->getEntryInformation().data_size >= size;
    //});


    if (
        found == filesToWrite.end() || // none found, shouldn't happen
        (*found)->getEntryInformation().data_size < size || // none found thats bigger than requested size
        !dynamic_cast<PboFTW_DummySpace*>(found->get()) // none found thats bigger than requested size
        )
        return filesToWrite.end();

    return found;
}

decltype(PboPatcher::filesToWrite)::iterator PboPatcher::FindFreeSpaceAfter(uint32_t size, uint32_t startOffset) {
    std::shared_lock ftwLockS(ftwMutex);
    //#TODO keep a freelist

    // prefer to find the smallest region that will fit our file

    auto found = std::min_element(std::execution::par_unseq, filesToWrite.begin(), filesToWrite.end(), [size, startOffset](const std::shared_ptr<PboFileToWrite>& l, const std::shared_ptr<PboFileToWrite>& r) {

        if (!dynamic_cast<PboFTW_DummySpace*>(l.get()) || l->getEntryInformation().data_size < size || l->getEntryInformation().startOffset < startOffset)
            return false; // either not a dummy space, or too small or too early in the file. Rule it out by moving it to the right

        return l->getEntryInformation().data_size >= size < r->getEntryInformation().data_size;
        });


    //auto found = std::ranges::find_if(filesToWrite, [size](const std::shared_ptr<PboFileToWrite>& file) {
    //    return dynamic_cast<PboFTW_DummySpace*>(file.get()) && file->getEntryInformation().data_size >= size;
    //});


    if (
        found == filesToWrite.end() || // none found, shouldn't happen
        (*found)->getEntryInformation().data_size < size || // none found thats bigger than requested size
        !dynamic_cast<PboFTW_DummySpace*>(found->get()) // none found thats bigger than requested size
        )
        return filesToWrite.end();

    return found;
}

bool PboPatcher::InsertFileIntoDummySpace(std::shared_ptr<PboFileToWrite> newFile) {
    std::shared_lock ftwLockS(ftwMutex);
    auto freeSpace = FindFreeSpace(newFile->getEntryInformation().data_size);
    if (freeSpace == filesToWrite.end()) // no space found
        return false;
    uint32_t freeSpaceFound = (*freeSpace)->getEntryInformation().data_size;

    ftwLockS.unlock();
    std::unique_lock ftwLock(ftwMutex);

    // check if someone snatched it away while we switched locks
    if (!CheckFTWIteratorValid(freeSpace) || (*freeSpace)->getEntryInformation().data_size != freeSpaceFound || !dynamic_cast<PboFTW_DummySpace*>(freeSpace->get())) {
        freeSpace = FindFreeSpace(newFile->getEntryInformation().data_size);
        if (freeSpace == filesToWrite.end()) // no space found
            return false;
        freeSpaceFound = (*freeSpace)->getEntryInformation().data_size;
    }

    // if we don't use all of the space we need to insert a new dummy space
    auto sizeLeftover = freeSpaceFound - newFile->getEntryInformation().data_size;

    if (sizeLeftover) {
        // we have more space than we need. We insert our new file before the dummy space, and reduce the dummy spaces size

        (*freeSpace)->getEntryInformation().data_size = sizeLeftover;
        (*freeSpace)->getEntryInformation().original_size = sizeLeftover; // mikero weirdness
        newFile->getEntryInformation().startOffset = (*freeSpace)->getEntryInformation().startOffset;
        (*freeSpace)->getEntryInformation().startOffset += newFile->getEntryInformation().data_size;
        filesToWrite.insert(freeSpace, ConvertNoTouchFile(newFile));
    } else {
        // fits exactly, we can just replace our dummy space
        auto startOffset = (*freeSpace)->getEntryInformation().startOffset;
        *freeSpace = ConvertNoTouchFile(newFile);
        (*freeSpace)->getEntryInformation().startOffset = startOffset;
    }

    return true;
}

std::filesystem::path PboPatcher::GetNewDummyFileName(uint32_t startOffset) {
    return "$DU" + std::to_string(startOffset);
}

std::shared_ptr<PboFileToWrite> PboPatcher::ConvertNoTouchFile(std::shared_ptr<PboFileToWrite> input) {

    if (!dynamic_cast<PboFTW_NoTouch*>(input.get()))
        return input;

    std::string fileContent;

    auto buffer = readerRef->getFileBuffer(input->getEntryInformation());
    fileContent.resize(input->getEntryInformation().data_size);
    buffer.xsgetn(fileContent.data(), fileContent.length());


    auto newFile = std::make_shared<PboFTW_FromString>(input->getEntryInformation(), std::move(fileContent));
    newFile->getEntryInformation().method = input->getEntryInformation().method; // method stays the same here
    return newFile;
}

void PatchAddFileFromDisk::Process(PboPatcher& patcher) {
    // first try if we can fit into dummy space

    auto newFile = std::make_shared<PboFTW_CopyFromFile>(pboFilePath.string(), inputFile);

    if (patcher.InsertFileIntoDummySpace(newFile))
        return; // successfully placed into dummyspace

    // didn't fit into any dummy space. Append to end.
    std::unique_lock ftwLock(patcher.ftwMutex);

    patcher.filesToWrite.emplace_back(newFile);
    newFile->getEntryInformation().startOffset = patcher.endStartOffset;
    patcher.endStartOffset += newFile->getEntryInformation().data_size;
}

void PatchUpdateFileFromDisk::Process(PboPatcher& patcher) {
    auto newFile = std::make_shared<PboFTW_CopyFromFile>(pboFilePath.string(), inputFile);


    // get iterator to our file
    std::shared_lock ftwLockS(patcher.ftwMutex);
    auto foundFile = patcher.GetFTWIteratorToFile(pboFilePath);
    if (foundFile == patcher.filesToWrite.end()) {
        Util::TryDebugBreak();
        return;
    }
    const auto oldFileSize = (*foundFile)->getEntryInformation().data_size;

    ftwLockS.unlock();

    const auto newFileSize = newFile->getEntryInformation().data_size;

    auto validateOldFileIterator = [&]() {
        if (!patcher.CheckFTWIteratorValid(foundFile) || (*foundFile)->getEntryInformation().name != pboFilePath) {
            // file invalidated, find it again
            foundFile = patcher.GetFTWIteratorToFile(pboFilePath);
        }
    };

    if (oldFileSize == newFileSize) {
        // size is exactly the same, we can just exchange them and we're done
        std::unique_lock ftwLock(patcher.ftwMutex);

        validateOldFileIterator();

        newFile->getEntryInformation().startOffset = (*foundFile)->getEntryInformation().startOffset;
        *foundFile = newFile;
        return;
    }

    if (newFileSize < oldFileSize) {
        // new file is smaller, just write it and place a dummy space after it with the leftover
        std::unique_lock ftwLock(patcher.ftwMutex);

        validateOldFileIterator();

        newFile->getEntryInformation().startOffset = (*foundFile)->getEntryInformation().startOffset;
        *foundFile = newFile;

        auto newDummySpace = std::make_shared<PboFTW_DummySpace>(patcher.GetNewDummyFileName((*foundFile)->getEntryInformation().startOffset).string(), (*foundFile)->getEntryInformation());
        newDummySpace->getEntryInformation().startOffset += newFileSize;
        newDummySpace->getEntryInformation().data_size = newDummySpace->getEntryInformation().original_size = oldFileSize - newFileSize;

        patcher.filesToWrite.insert(foundFile + 1, newDummySpace);
        return;
    }

    // new file is larger, turn old file into dummy space, and insert new file wherever it fits

    patcher.ReplaceWithDummyFile(pboFilePath, std::distance(patcher.filesToWrite.begin(),foundFile));


    // first try if we can fit into dummy space
    if (patcher.InsertFileIntoDummySpace(newFile))
        return; // successfully placed into dummyspace
    //
    // didn't fit into any dummy space. Append to end.
    std::unique_lock ftwLock(patcher.ftwMutex);

    newFile->getEntryInformation().startOffset = patcher.endStartOffset;
    patcher.filesToWrite.emplace_back(newFile);
    patcher.endStartOffset += newFile->getEntryInformation().data_size;
}

void PatchRenameFile::Process(PboPatcher& patcher)
{
    // get iterator to our file
    std::shared_lock ftwLockS(patcher.ftwMutex);
    auto foundFile = patcher.GetFTWIteratorToFile(pboFilePathOld);
    if (foundFile == patcher.filesToWrite.end()) {
        Util::TryDebugBreak();
        return;
    }
    (*foundFile)->getEntryInformation().name = pboFilePathNew.string();

    ftwLockS.unlock();

}

void PatchDeleteFile::Process(PboPatcher& patcher) {
    // simply remove the file from FTW
    patcher.ReplaceWithDummyFile(pboFilePath);
}

void PatchAddProperty::Process(PboPatcher& patcher) {
    std::unique_lock lock(patcher.propertyMutex);
    // replace if property with same key already exists
    const auto& [key, value] = property;

    auto found = std::ranges::find_if(patcher.properties, [&key](const PboProperty& prop) { return prop.key == key; });
    if (found == patcher.properties.end())
        patcher.properties.emplace_back(PboProperty(property.first, property.second));
    else
        found->value = value;
}

void PatchDeleteProperty::Process(PboPatcher& patcher) {
    std::unique_lock lock(patcher.propertyMutex);
    auto found = std::ranges::find_if(patcher.properties, [this](const PboProperty& prop) { return prop.key == propertyName; });
    if (found != patcher.properties.end())
        patcher.properties.erase(found);
}

void PboPatcher::ReadInputFile(const PboReader* inputFile) {
    files = inputFile->getFiles();
    properties = inputFile->getProperties();
    readerRef = inputFile;
}


void PboPatcher::ProcessPatches() {

    //prefill filesToWrite, by default everything is notouch
    for (auto& it : files) {

        if (it.name.starts_with("$DU")) {
            filesToWrite.emplace_back(std::make_shared<PboFTW_DummySpace>(it));
        }
        else
            filesToWrite.emplace_back(std::make_shared<PboFTW_NoTouch>(it));
    }
    endStartOffset = filesToWrite.back()->getEntryInformation().startOffset + filesToWrite.back()->getEntryInformation().data_size;


    // deletes first to free up dummy spaces
    currentPatchStep = PatchType::Delete;
    auto deletePatches = std::ranges::partition(patches, [](const auto& patch) {return patch->GetType() == PatchType::Delete; });

    // deletes can run in parallel
    std::for_each(std::execution::par_unseq, patches.begin(), deletePatches.begin(), [this](const auto& patch) {patch->Process(*this); });

    // update patches, shuffle files around, fill empty spaces
    currentPatchStep = PatchType::Update;
    auto updatePatches = std::ranges::partition(deletePatches, [](const auto& patch) {return patch->GetType() == PatchType::Update; });

    for (auto& it : std::ranges::subrange(deletePatches.begin(), updatePatches.begin()))
        it->Process(*this);


    // Add new files
    currentPatchStep = PatchType::Add;
    auto addPatches = std::ranges::subrange(updatePatches.begin(), patches.end());

    for (auto& it : addPatches)
        it->Process(*this);


    // Sort for signature V3, some file types need to be sorted by name

    // We don't actually need to sort, the order just needs to match with order on server.
    // "sqf", "inc", "bikb", "ext", "fsm", "sqm", "hpp", "cfg", "sqs", "h", "sqfc"

    //std::ranges::is_sorted
    //if (!std::is_sorted(filesToWrite.begin(), filesToWrite.end(), [](const std::shared_ptr<PboFileToWrite>& l, const std::shared_ptr<PboFileToWrite>& r) {
    //        return l->getEntryInformation().name < r->getEntryInformation().name;
    //    })) {
    //
    //}





    // potential defragment step here



    // make sure we have the appropriate free space for our headers

    auto headerSize = PboWriter::calculateHeaderSize(properties, filesToWrite);

    // start offset of first file needs to == headerSize
    while (filesToWrite.front()->getEntryInformation().startOffset != headerSize) {
        // we need to adjust
        uint32_t firstFileStartOffset = filesToWrite.front()->getEntryInformation().startOffset;
        int32_t sizeDifference = firstFileStartOffset - headerSize;

        if (sizeDifference > 0) {
            // our first file starts too late, we need to add dummy spacer (but spacer makes our header larger!! maybe too large, this will basically always require a second iteration) or extend existing dummy spacer

            if (auto firstDummyFile = dynamic_cast<PboFTW_DummySpace*>(filesToWrite.front().get())) {
                // first file is already dummy, we can just make it larger to fit
                firstDummyFile->getEntryInformation().data_size += sizeDifference;
                firstDummyFile->getEntryInformation().original_size += sizeDifference; // mikero weirdness
                firstDummyFile->getEntryInformation().startOffset -= sizeDifference;

            }
            else {
                // we need to insert new dummy file at start

                // how many bytes this dummy file will add to the header at minimum
                const auto minDummyFileHeaderSize = GetNewDummyFileName(firstFileStartOffset - sizeDifference).string().length() + 21;
                const auto newHeadersize = headerSize + minDummyFileHeaderSize;

                //if (minDummyFileHeaderSize > sizeDifference) {
                //    // our dummy file would be too big. We cannot make this fit. We will need to convert the first file to a dummy so we can then adjust it next iteration
                //    MoveFileToEndOrDummy(filesToWrite.front()->getEntryInformation().name, 0);
                //} else 
                {
                    auto dummyFile = std::make_shared<PboFTW_DummySpace>(GetNewDummyFileName(firstFileStartOffset - sizeDifference).string(), sizeDifference);
                    dummyFile->getEntryInformation().startOffset = firstFileStartOffset - sizeDifference;
                    filesToWrite.insert(filesToWrite.begin(), dummyFile);
                }
            }
        }
        else {
            // our first file starts too early, we need to move it away by turning it into a dummy space (if it isn't already). And move the dummy space's startOffset back till its good

            if (auto firstDummyFile = dynamic_cast<PboFTW_DummySpace*>(filesToWrite.front().get())) {
                // first file is dummy space, can we move it far enough back?

                if (firstDummyFile->getEntryInformation().data_size >= std::abs(sizeDifference)) {
                    // dummy file is big enough to fix our difference, lets move it

                    firstDummyFile->getEntryInformation().data_size -= std::abs(sizeDifference);
                    firstDummyFile->getEntryInformation().original_size -= std::abs(sizeDifference); // mikero weirdness
                    firstDummyFile->getEntryInformation().startOffset += std::abs(sizeDifference);

                    // we cannot delete 0 size dummy file. That would make the header smaller, which means we need to add new dummy spacer, which makes the header larger again, which means we need to remove...
                }
                else {
                    // dummy file is too small for our needs. Just remove it
                    filesToWrite.erase(filesToWrite.begin());

                    // we know we'll need the space of the next file, if its dummy we can adjust it next cycle, if its not dummy we need to make it so
                    MoveFileToEndOrDummy(filesToWrite.front()->getEntryInformation().name, 0);
                }
            }
            else {
                // first file is a non-dummy. We need to move it away
                MoveFileToEndOrDummy(filesToWrite.front()->getEntryInformation().name, 0);
                // now run another iteration, where we make the dummy file smaller till it fits
            }
        }

        // recalculate new headersize after changes
        headerSize = PboWriter::calculateHeaderSize(properties, filesToWrite);
    }


    readerRef = nullptr;
}


void PboPatcher::WriteOutputFile(std::iostream& outputStream) {
    PboWriter outputFile;

    for (auto& it : properties)
        outputFile.addProperty(it);

    for (auto& it : filesToWrite)
        outputFile.addFile(it);

    outputFile.writePboEx(outputStream);
}
