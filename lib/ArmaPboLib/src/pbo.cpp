#include "pbo.hpp"

bool PboProperty::read(std::istream& in) {
    std::getline(in, key, '\0');
    if (key.empty()) return false; //We tried to read the end element of the property list
    std::getline(in, value, '\0');
    return true;
}

void PboProperty::write(std::ostream& out) const {
    out.write(key.c_str(), key.length() + 1);
    out.write(value.c_str(), value.length() + 1);
}

void PboEntry::read(std::istream& in) {
    struct {
        uint32_t method;
        uint32_t originalsize;
        uint32_t reserved;
        uint32_t timestamp;
        uint32_t datasize;
    } header {};

    std::getline(in, name, '\0');
    in.read(reinterpret_cast<char*>(&header), sizeof(header));

    method = PboEntryPackingMethod::none;

    if (header.method == 'Encr') { //encrypted
        method = PboEntryPackingMethod::encrypted;
    }
    if (header.method == 'Cprs') { //compressed
        method = PboEntryPackingMethod::compressed;
    }
    if (header.method == 'Vers') { //Version
        method = PboEntryPackingMethod::version;
    }

    data_size = header.datasize;
    if (method == PboEntryPackingMethod::compressed)
        original_size = header.originalsize;
    else
        original_size = header.datasize;
}

void PboEntry::write(std::ostream& out, bool noDate) const {
    struct {
        uint32_t method;
        uint32_t originalsize;
        uint32_t reserved;
        uint32_t timestamp;
        uint32_t datasize;
    } header {};

    switch (method) {
        case PboEntryPackingMethod::none: header.method = 0; break;
        case PboEntryPackingMethod::version: header.method = 'Vers'; break;
        case PboEntryPackingMethod::compressed: header.method = 'Cprs'; break;
        case PboEntryPackingMethod::encrypted: header.method = 'Encr'; break;
    }
    header.originalsize = original_size;
    header.reserved = 0;// '3ECA'; //#TODO remove dis?
    //time is unused by Arma. We could write more misc stuff into there for a total of 8 bytes
    header.timestamp = 0;// noDate ? 0 : std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    header.datasize = data_size;

    out.write(name.c_str(), name.length()+1);
    out.write(reinterpret_cast<char*>(&header.method), 4);
    out.write(reinterpret_cast<char*>(&header.originalsize), 4);
    out.write(reinterpret_cast<char*>(&header.reserved), 4);
    out.write(reinterpret_cast<char*>(&header.timestamp), 4);
    out.write(reinterpret_cast<char*>(&header.datasize), 4);
}

void PboEntryBuffer::lzss_decomp(std::istream& input, std::vector<char>& output, size_t expectedSize) {
    //Based on https://github.com/Braini01/bis-file-formats/blob/6eba9b590df03eadd7822c164ba397748190f7a5/BIS.Core/Compression/LZSS.cs#L7
    const int N = 4096;
    const int F = 18;
    const int THRESHOLD = 2;
    char text_buf[N+F-1];

    auto bytesLeft = expectedSize;
    int iDst = 0;
    output.resize(expectedSize);

    int i,j,r,c,csum=0;
    int flags;
    for( i=0; i<N-F; i++ ) text_buf[i] = ' ';
    r=N-F; flags=0;
    while( bytesLeft>0 ) {
        if( ((flags>>= 1)&256)==0 ) {
            c=input.get();
            flags=c|0xff00;
        }
        if( (flags&1) != 0) {
            c=input.get();
            csum += (uint8_t)c;

            // save byte
            output[iDst++]=(int8_t)c;
            bytesLeft--;
            // continue decompression
            text_buf[r]=(char)c;
            r++;r&=(N-1);
        } else {
            i=input.get();
            j=input.get();
            i|=(j&0xf0)<<4; j&=0x0f; j+=THRESHOLD;

            int ii = r-i;
            int jj = j+ii;

            if (j+1>bytesLeft) {
                throw new std::runtime_error("LZSS overflow");
            }

            for(; ii<=jj; ii++ ) {
                c=(uint8_t)text_buf[ii&(N-1)];
                csum += (uint8_t)c;

                // save byte
                output[iDst++]=(int8_t)c;
                bytesLeft--;
                // continue decompression
                text_buf[r]=(char)c;
                r++;r&=(N-1);
            }
        }
    }

    uint32_t checksum = input.get()+(input.get()<<8)+(input.get()<<16)+(input.get()<<24);

    if( checksum!=csum ) {
        throw new std::runtime_error("Checksum mismatch");
    }
}

PboEntryBuffer::
PboEntryBuffer(const PboReader& rd, const PboEntry& ent, uint32_t bufferSize): buffer(std::min(ent.original_size, bufferSize)), file(ent),
                                                                               reader(rd) {
    char* start = &buffer.front();
    setg(start, start, start);

    if (ent.method == PboEntryPackingMethod::compressed) {

        rd.input.seekg(file.startOffset);

        buffer.resize(ent.original_size);


        lzss_decomp(rd.input, buffer, ent.original_size);
        bufferEndFilePos = ent.original_size;
        setg(&buffer.front(), &buffer.front(), &buffer.front() + ent.original_size);
    }
}

void PboEntryBuffer::setBufferSize(size_t newSize) {
    if (file.method == PboEntryPackingMethod::compressed) return; //Already max size
    size_t dataLeft = egptr() - gptr();
    auto bufferOffset = gptr() - &buffer.front(); //Where we currently are inside the buffer
    auto bufferStartInFile = bufferEndFilePos - (dataLeft + bufferOffset); //at which offset in the PboEntry file our buffer starts
    bufferEndFilePos = bufferStartInFile; //Back to start.
    setg(&buffer.front(), &buffer.front(), &buffer.front()); //no data available

    buffer.clear(); //So we don't copy on realloc.
    buffer.resize(newSize);
}

int PboEntryBuffer::underflow() {
    if (gptr() < egptr()) // buffer not exhausted
        return traits_type::to_int_type(*gptr());

    reader.input.seekg(file.startOffset + bufferEndFilePos);
    size_t sizeLeft = file.original_size - bufferEndFilePos;
    if (sizeLeft == 0) return std::char_traits<char>::eof();

    auto sizeToRead = std::min(sizeLeft, buffer.size());
    reader.input.read(buffer.data(), sizeToRead);
    bufferEndFilePos += sizeToRead;

    setg(&buffer.front(), &buffer.front(), &buffer.front() + sizeToRead);

    return std::char_traits<char>::to_int_type(*this->gptr());
}

int64_t PboEntryBuffer::xsgetn(char* _Ptr, int64_t _Count) {
    // get _Count characters from stream
    const int64_t _Start_count = _Count;

    while (_Count) {
        size_t dataLeft = egptr() - gptr();
        if (dataLeft == 0) {
            reader.input.seekg(file.startOffset + bufferEndFilePos);
            size_t sizeLeft = file.original_size - bufferEndFilePos;
            if (sizeLeft == 0) break; //EOF
            auto sizeToRead = std::min(sizeLeft, buffer.size());
            reader.input.read(buffer.data(), sizeToRead);
            bufferEndFilePos += sizeToRead;

            setg(&buffer.front(), &buffer.front(), &buffer.front() + sizeToRead);

            dataLeft = std::min(sizeToRead, (size_t)_Count);
        } else
            dataLeft = std::min(dataLeft, (size_t)_Count);

        std::copy(gptr(), gptr() + dataLeft, _Ptr);
        _Ptr += dataLeft;
        _Count -= dataLeft;
        gbump(dataLeft);
    }

    return (_Start_count - _Count);
}

std::basic_streambuf<char>::pos_type PboEntryBuffer::seekoff(off_type offs, std::ios_base::seekdir dir, std::ios_base::openmode mode) {
    auto test = egptr();
    auto test2 = gptr();


    switch (dir) {
        case std::ios_base::beg: {
            //#TODO negative offs is error


            size_t dataLeft = egptr() - gptr();
            auto bufferOffset = gptr() - &buffer.front(); //Where we currently are inside the buffer
            auto bufferStartInFile = bufferEndFilePos - (dataLeft + bufferOffset); //at which offset in the PboEntry file our buffer starts



            //offset is still inside buffer
            if (bufferStartInFile <= offs && bufferEndFilePos > offs) { 
                auto curFilePos = (bufferEndFilePos - dataLeft);

                int64_t offsetToCurPos = offs - static_cast<int64_t>(curFilePos);
                gbump(offsetToCurPos); //Jump inside buffer till we find offs
                return offs;
            }

            //We are outside of buffer. Just reset and exit
            bufferEndFilePos = offs;
            setg(&buffer.front(), &buffer.front(), &buffer.front()); //no data available
            return bufferEndFilePos;

        }

        break;
        case std::ios_base::cur: {
                size_t dataLeft = egptr() - gptr();
                auto curFilePos = (bufferEndFilePos - dataLeft);

                if (offs == 0) return curFilePos;

                if (dataLeft == 0) {
                    bufferEndFilePos += offs;
                    return bufferEndFilePos;
                }


                if (offs > 0 && dataLeft > offs) { // offset is still inside buffer
                    gbump(offs);
                    return curFilePos + offs;
                }
                if (offs > 0) { //offset is outside of buffer
                    bufferEndFilePos = curFilePos + offs;
                    setg(&buffer.front(), &buffer.front(), &buffer.front()); //no data available
                    return bufferEndFilePos;
                }

                if (offs < 0) {

                    auto bufferOffset = gptr() - &buffer.front(); //Where we currently are inside the buffer
                    if (bufferOffset >= -offs) {//offset is still in buffer
                        gbump(offs);
                        return bufferOffset + offs;
                    }

                    bufferEndFilePos = curFilePos + offs;
                    setg(&buffer.front(), &buffer.front(), &buffer.front()); //no data available
                    return bufferEndFilePos;
                }
            }
        break;
        case std::ios_base::end:
            //#TODO positive offs is error
            bufferEndFilePos = file.original_size + offs;
            setg(&buffer.front(), &buffer.front(), &buffer.front()); //no data available
            return bufferEndFilePos;
        break;
    }
    return -1; //#TODO this is error
}

std::basic_streambuf<char>::pos_type PboEntryBuffer::seekpos(pos_type offs, std::ios_base::openmode mode) {
    return seekoff(offs, std::ios_base::beg, mode);
}

std::streamsize PboEntryBuffer::showmanyc() {
    //How many characters are left
    size_t dataLeft = egptr() - gptr();
    
    return (file.original_size - bufferEndFilePos) + dataLeft;
}

void PboReader::readHeaders() {
    PboEntry intro;
    intro.read(input);

    //#TODO check stuff and throw if error
    //filename is empty
    //packing method is vers
    //time is 0
    //datasize is 0

    //header ignores startoffset and uncompressed size

    if (intro.method == PboEntryPackingMethod::none) {//Broken 3den exported pbo
        input.seekg(0, std::istream::beg); //Seek back to start
        badHeader = true;
    } else {   
        PboProperty prop;
        while (prop.read(input)) {
            properties.emplace_back(std::move(prop));
        }
        //When prop's last read "failed" we just finished reading the terminator of the properties
        propertiesEnd = input.tellg();
    }


    PboEntry entry;

    while (entry.read(input), !entry.name.empty()) {
        files.emplace_back(std::move(entry));
    }
    //We just read the last terminating entry header too.
    headerEnd = input.tellg();

    size_t curPos = headerEnd;
    for (auto& it : files) {
        it.startOffset = curPos;
        curPos += it.data_size;
    }
    auto fileEnd = curPos;
    input.seekg(fileEnd+1, std::istream::beg);
    input.read(reinterpret_cast<char*>(hash.data()), 20); //After end there is checksum 20 bytes.
}

void PboReader::readHeadersSparse() {
    PboEntry intro;
    intro.read(input);

    //#TODO check stuff and throw if error
    //filename is empty
    //packing method is vers
    //time is 0
    //datasize is 0

    //header ignores startoffset and uncompressed size

    if (intro.method == PboEntryPackingMethod::none) {//Broken 3den exported pbo
        input.seekg(0, std::istream::beg); //Seek back to start
        badHeader = true;
    } else {
        PboProperty prop;
        while (prop.read(input)) {
            properties.emplace_back(std::move(prop));
        }
        //When prop's last read "failed" we just finished reading the terminator of the properties
        propertiesEnd = input.tellg();
    }

    input.seekg(0, std::istream::beg); //Seek back to start
}

#ifdef ARMA_PBO_LIB_WRITE

#ifdef ARMA_PBO_LIB_SHA1

#include <array>

extern "C" {
#include "../lib/sha1.h"
}


struct hashing_ostreambuf : public std::streambuf
{
    hashing_ostreambuf(std::ostream &finalOut) : finalOut(finalOut) {
        SHA1Reset(&context);
    }

protected:
    std::streamsize xsputn(const char_type* s, std::streamsize n) override {
        SHA1Input(&context, reinterpret_cast<const unsigned char*>(s), n);
        finalOut.write(s, n);

        //Yes return result is fake, but meh.
        return n; // returns the number of characters successfully written.
    }

    int_type overflow(int_type ch) override {
        auto outc = static_cast<char>(ch);
        SHA1Input(&context, reinterpret_cast<const unsigned char*>(&outc), 1);
        finalOut.put(outc);

        return 1;
    }

public:

    std::array<char, 20> getResult() {

        if (!SHA1Result(&context))
            throw std::logic_error("SHA1 hash corrupted");

        for (int i = 0; i < 5; i++) {
            unsigned temp = context.Message_Digest[i];
            context.Message_Digest[i] = ((temp >> 24) & 0xff) |
                ((temp << 8) & 0xff0000) | ((temp >> 8) & 0xff00) | ((temp << 24) & 0xff000000);
        }
        std::array<char, 20> res;

        memcpy(res.data(), context.Message_Digest, 20);

        return res;
    }


private:
    std::ostream &finalOut;
    SHA1Context context;
};

#endif

void PboFTW_CopyFromFile::writeDataTo(std::ostream& output) {
    std::ifstream inp(file, std::ifstream::binary);
    std::array<char, 4096> buf;
    do {
        inp.read(buf.data(), buf.size());
        output.write(buf.data(), inp.gcount());
    } while (inp.gcount() > 0);
}

void PboWriter::writePbo(std::ostream& output) {

#ifdef ARMA_PBO_LIB_SHA1
    hashing_ostreambuf hashOutbuf(output);

    std::ostream out(&hashOutbuf);
#else
    auto& out = output;
#endif

    struct pboEntryHeader {
        uint32_t method;
        uint32_t originalsize;
        uint32_t reserved;
        uint32_t timestamp;
        uint32_t datasize;
        void write(std::ostream& output) const {
            output.write(reinterpret_cast<const char*>(this), sizeof(pboEntryHeader));
        }
    };

    pboEntryHeader versHeader {
        'Vers',
        //'amra','!!ek', 0, 0
        //'UHTC',' UHL', 0, 0
        0,0,0,0
    };

    //Write a "dummy" Entry which is used as header.
    //#TODO this should use pboEntry class to write. But I want a special "reserved" value here
    out.put(0); //name
    versHeader.write(out);

    for (auto& it : properties)
        it.write(out);
    out.put(0); //properties endmarker

    //const size_t curOffs = out.tellp();
    //
    //const size_t headerStringsSize = std::transform_reduce(filesToWrite.begin(),filesToWrite.end(), 0u, std::plus<>(), [](auto& it) {
    //    return it->getEntryInformation().name.length() + 1;
    //});
    //
    //const size_t headerSize = (filesToWrite.size() + 1) * sizeof(pboEntryHeader) + headerStringsSize;
    //
    //auto curFileStartOffset = curOffs + headerSize;
    for (auto& it : filesToWrite) {
        auto& prop = it->getEntryInformation();
        //prop.startOffset = curFileStartOffset;
        //
        //curFileStartOffset += prop.name.length() + 1 + sizeof(pboEntryHeader);
        prop.write(out);
    }

    //End is indicated by empty name
    out.put(0);
    //Rest of the header after that is ignored, so why not have fun?
    pboEntryHeader endHeader {
        //'sihT','t si','b eh',' tse','gnat'
        0,0,0,0,0
    };
    endHeader.write(out);


    for (auto& it : filesToWrite) {
        it->writeDataTo(out);
    }


#ifdef ARMA_PBO_LIB_SHA1
    auto hash = hashOutbuf.getResult();

    //no need to write to hashing buf anymore
    output.put(0); //file hash requires leading zero which doesn't contribute to hash data
    output.write(hash.data(), hash.size());

#else
    output.put(0); //file hash requires leading zero which doesn't contribute to hash data
    output.write("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 20);
#endif


}

#endif // ARMA_PBO_LIB_WRITE
