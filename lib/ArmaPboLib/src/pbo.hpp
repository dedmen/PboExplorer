#pragma once
#define MAXFILES 4096

#include <fstream>
#include <utility>
#include <vector>
#include <algorithm>
#include <sstream>
#include <array>


#ifdef ARMA_PBO_LIB_WRITE
#include <filesystem>

#endif

struct header {
    char name[512];
    uint32_t packing_method;
    uint32_t original_size;
    uint32_t data_size;
};

class PboProperty {
public:
    PboProperty() {}
    PboProperty(std::string k, std::string v): key(std::move(k)), value(std::move(v)) {}

    std::string key;
    std::string value;

    bool read(std::istream& in);
    void write(std::ostream& out) const;
};

enum class PboEntryPackingMethod {
    none,
    version,
    compressed,
    encrypted
};

class PboEntry {
public:
    std::string name;

    uint32_t original_size = 0;
    uint32_t data_size = 0;
    uint32_t startOffset = 0;
    PboEntryPackingMethod method = PboEntryPackingMethod::none;


    void read(std::istream& in);
    void write(std::ostream& out, bool noDate = false) const;
};

class PboReader;
class PboEntryBuffer : public std::streambuf {
    std::vector<char> buffer;
    const PboEntry& file;
    const PboReader& reader;
    //What position the character after the last character that's currently in our buffer, corresponds to in the pbofile
    //Meaning on next read, the first character read is that pos
    size_t bufferEndFilePos{0};
    static void lzss_decomp(std::istream& input, std::vector<char>& output, size_t expectedSize);
public:
    PboEntryBuffer(const PboReader& rd, const PboEntry& ent, uint32_t bufferSize = 4096u);

    void setBufferSize(size_t newSize);
    int underflow() override;
    int64_t xsgetn(char* _Ptr, int64_t _Count) override;
    pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override;
    pos_type seekpos(pos_type, std::ios_base::openmode) override;
    std::streamsize showmanyc() override;

    void setBuffersize(size_t newSize) {
        buffer.resize(newSize);
    }
};

class PboReader {
    friend class PboEntryBuffer;

    std::vector<PboEntry> files;
    std::vector<PboProperty> properties;
    uint32_t propertiesEnd{0};
    uint32_t headerEnd{0};
    std::istream& input;
    std::array<unsigned char, 20> hash;
    bool badHeader{false};
public:
    PboReader(std::istream &input) : input(input) {}
    //Reads whole header and filelist
    void readHeaders();
    //Reads non-file headers like properties and seeks bakc to file start
    void readHeadersSparse();
    const auto& getFiles() const noexcept { return files; }
    PboEntryBuffer getFileBuffer(const PboEntry& ent) const {
        return PboEntryBuffer(*this, ent);
    }
    const auto& getProperties() const { return properties; }
    const auto& getHash() const { return hash; }
    //Broken 3DEN exported pbo with no header
    bool isBadHeader() const{ return badHeader; }
};

#ifdef ARMA_PBO_LIB_WRITE

class PboFileToWrite : std::enable_shared_from_this<PboFileToWrite> { //#TODO need a better name
protected:
    PboEntry entryInfo;
public:
    virtual ~PboFileToWrite() = default;
    //return info like name and filesize (packing method not supported yet, startOffset/originalSize are ignored)
    //Might be called by multiple threads!
    virtual PboEntry& getEntryInformation() {
        return entryInfo;
    }
    virtual void writeDataTo(std::ostream &output) = 0;

};

class PboFTW_CopyFromFile : public PboFileToWrite {
    std::filesystem::path file;
public:
    PboFTW_CopyFromFile(std::string filename, std::filesystem::path inputFile) : file(std::move(inputFile)) {
        entryInfo.name = std::move(filename);
        //only backslash in pbo
        std::replace(entryInfo.name.begin(), entryInfo.name.end(), '/', '\\');
        entryInfo.data_size = entryInfo.original_size = std::filesystem::file_size(file); //I expect file size not to change.
    }
    //PboEntry& getEntryInformation() override {
    //    entryInfo.data_size = entryInfo.original_size = std::filesystem::file_size(file);
    //    return entryInfo;
    //}
    void writeDataTo(std::ostream& output) override;
};

class PboFTW_FromString : public PboFileToWrite {
    std::string data;
public:
    PboFTW_FromString(std::string filename, std::string input) : data(std::move(input)) {
        entryInfo.name = std::move(filename);
        //only backslash in pbo
        std::replace(entryInfo.name.begin(), entryInfo.name.end(), '/', '\\');
        entryInfo.data_size = entryInfo.original_size = data.length();
    }

    PboFTW_FromString(const PboEntry& entry, std::string input) : data(std::move(input)) {
        entryInfo.name = entry.name;
        //only backslash in pbo
        std::replace(entryInfo.name.begin(), entryInfo.name.end(), '/', '\\');
        entryInfo.startOffset = entry.startOffset;
        entryInfo.data_size = entryInfo.original_size = data.length();
    }

    void writeDataTo(std::ostream& output) override;
};

class hashing_ostreambuf;

// This file writer just skips over the files space, not touching any data but reading the data into the SHA1 hasher
// Only used in pboWriter::writePboEx when overwriting/patching a existing pbo
class PboFTW_NoTouch : public PboFileToWrite {
public:
    PboFTW_NoTouch(const PboEntry& entry) {
        entryInfo.name = entry.name;
        //only backslash in pbo
        std::replace(entryInfo.name.begin(), entryInfo.name.end(), '/', '\\');
        entryInfo.startOffset = entry.startOffset;
        entryInfo.data_size = entry.data_size;
        entryInfo.original_size = entry.original_size;
    }
    void writeDataTo(std::ostream& output) override {};
    void processData(std::iostream& output
#ifdef ARMA_PBO_LIB_SHA1
        , hashing_ostreambuf& hashOutbuf
#endif        
        );
};


// This file writer just skips over the files space, not touching any data but reading the data into the SHA1 hasher
// Only used in pboWriter::writePboEx when overwriting/patching a existing pbo
class PboFTW_DummySpace : public PboFileToWrite {
public:
    PboFTW_DummySpace(std::string filename, uint32_t dataSize) {
        entryInfo.name = std::move(filename);
        //only backslash in pbo
        std::replace(entryInfo.name.begin(), entryInfo.name.end(), '/', '\\');
        entryInfo.data_size = entryInfo.original_size = dataSize;
    }
    PboFTW_DummySpace(const PboEntry& entry) {
        entryInfo.name = entry.name;
        //only backslash in pbo
        std::replace(entryInfo.name.begin(), entryInfo.name.end(), '/', '\\');
        entryInfo.startOffset = entry.startOffset;
        entryInfo.data_size = entry.data_size;
        entryInfo.original_size = entry.original_size;
    }

    PboFTW_DummySpace(std::string filename, const PboEntry& entry) {
        entryInfo.name = std::move(filename);
        //only backslash in pbo
        std::replace(entryInfo.name.begin(), entryInfo.name.end(), '/', '\\');
        entryInfo.startOffset = entry.startOffset;
        entryInfo.data_size = entry.data_size;
        entryInfo.original_size = entry.original_size;
    }

    void writeDataTo(std::ostream& output) override;
};



class PboWriter {
    std::vector<PboProperty> properties;
    std::vector<std::shared_ptr<PboFileToWrite>> filesToWrite;
public:


    void addProperty(PboProperty prop) { properties.emplace_back(std::move(prop)); }
    void addFile(std::shared_ptr<PboFileToWrite> file) { filesToWrite.emplace_back(std::move(file)); }
    void writePbo(std::ostream &output);
    // overwrite/patch a existing pbo, supports PboFTW_NoTouch and PboFTW_DummySpace
    void writePboEx(std::iostream& output);

    // precalculate size of header/offset to start of first file
    static uint32_t calculateHeaderSize(const std::vector<PboProperty>& properties, const std::vector<std::shared_ptr<PboFileToWrite>>& filesToWrite);
};

#endif