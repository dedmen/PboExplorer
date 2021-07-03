#include "PboFileStream.hpp"

#include <Shlwapi.h>

#include "PboPatcher.hpp"
#include "Util.hpp"
#include "lib/ArmaPboLib/src/pbo.hpp"
#include "DebugLogger.hpp"


PboFileStream::PboFileStream(std::shared_ptr<PboFile> pboFile, std::filesystem::path filePath) :
    pboFile(pboFile),
    filePath(filePath),
    pboInputStream(pboFile->diskPath, std::ios::in | std::ios::binary),
	pboReader(pboInputStream),
    fileBuffer(pboReader, *[this, filePath]()
    {
	    pboReader.readHeaders();

	    auto& pboFiles = pboReader.getFiles();
	    const auto found = std::find_if(pboFiles.begin(), pboFiles.end(), [&](const PboEntry& entry)
	    {
		    return Util::utf8_decode(entry.name) == filePath;
	    });

        

        if (found == pboFiles.end())
            __debugbreak();
        this->fileSize = found->original_size;
	    return found;
    }()),
	sourceStream(&fileBuffer)
{
    m_pos = 0;
}
PboFileStream::~PboFileStream()
{
    //dirMutex.lock();
    //m_stream->countDown();
    //dirMutex.unlock();
}


// IUnknown
HRESULT PboFileStream::QueryInterface(REFIID riid, void** ppvObject)
{
    DebugLogger_OnQueryInterfaceEntry(riid);
    if (IsEqualIID(riid, IID_IStream))
        *ppvObject = (IStream*)this;
    else if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = (IUnknown*)this;
    else
    {
        DebugLogger_OnQueryInterfaceExitUnhandled(riid);
        *ppvObject = nullptr;
        return(E_NOINTERFACE);
    }

    AddRef();
    return(S_OK);
}


// IStream
HRESULT PboFileStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
    ULONG didRead = 0;

    if (tempFile) {
        if (tempFileStream.seekg(m_pos))
        {
            tempFileStream.read(static_cast<char*>(pv), cb);
            didRead = tempFileStream.gcount();
        }
    } else {
        //dirMutex.lock();
        if (sourceStream.seekg(m_pos))
        {
            sourceStream.read((char*)pv, cb);
            didRead = sourceStream.gcount();
        }

        //dirMutex.unlock();
    }

    if (!didRead && !m_pos) return E_FAIL;

    m_pos += didRead;
    *pcbRead = didRead;

    return cb == didRead ? S_OK : S_FALSE;
}

HRESULT PboFileStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
{
    // https://docs.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-isequentialstream-write
    TransformToWritable();

    ULONG didWrite = 0;

    if (tempFileStream.seekp(m_pos))
    {
        tempFileStream.write(static_cast<const char*>(pv), cb);
        didWrite = cb; //#TODO check properly
    }

    // write into memory buffer


    if (!didWrite && !m_pos) return(E_FAIL);

    m_pos += didWrite;
    if (pcbWritten)
        *pcbWritten = didWrite;

    return(cb == didWrite ? S_OK : S_FALSE);
}
HRESULT PboFileStream::Seek(
    LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
{
    switch (dwOrigin)
    {
    default:
        m_pos = dlibMove.QuadPart;
        break;
    case STREAM_SEEK_CUR:
        m_pos += dlibMove.QuadPart;
        break;
    case STREAM_SEEK_END:
        m_pos = fileSize + dlibMove.QuadPart;
        break;
    }

    if (plibNewPosition)
        plibNewPosition->QuadPart = m_pos;

    return(S_OK);
}
HRESULT PboFileStream::SetSize(ULARGE_INTEGER newSize)
{
    return(E_NOTIMPL);
}

HRESULT PboFileStream::CopyTo(IStream* dstStream, ULARGE_INTEGER bytesToCopy, ULARGE_INTEGER* nBytesRead, ULARGE_INTEGER* nBytesWritten)
{

    size_t sizeLeft = bytesToCopy.QuadPart;

    std::array<char, 4096> buf;
    HRESULT res;
    ULONG bytesRead;
    ULONG bytesWritten;
    do {
        if (FAILED(res = Read(buf.data(), std::min(buf.size(), sizeLeft), &bytesRead)))
            return res;
        if (nBytesRead)
            nBytesRead->QuadPart += bytesRead;

        //if (std::min(buf.size(), sizeLeft) != bytesRead)
        //    __debugbreak();

        if (FAILED(res = dstStream->Write(buf.data(), bytesRead, &bytesWritten)))
            return res;
        if (nBytesWritten)
            nBytesWritten->QuadPart += bytesWritten;

        sizeLeft -= bytesRead;
    } while (bytesRead > 0 && sizeLeft > 0);

    return S_OK;
}

HRESULT PboFileStream::Commit(DWORD)
{
    //https://docs.microsoft.com/en-us/windows/win32/api/wtypes/ne-wtypes-stgc
    // write to pbo file

    if (!tempFile)
        __debugbreak();

    tempFileStream.flush();
    tempFileStream.close();

    (*tempFile)->PatchToOrigin();

    // BLURP

    tempFileStream.open((*tempFile)->GetPath(), std::ifstream::binary | std::ifstream::in | std::ifstream::out);
    tempFileStream.seekg(m_pos);
    tempFileStream.seekp(m_pos);

    return S_OK;
}
HRESULT PboFileStream::Revert(void)
{
    return(E_NOTIMPL);
}
HRESULT PboFileStream::LockRegion(
    ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
{
    return(E_NOTIMPL);
}
HRESULT PboFileStream::UnlockRegion(
    ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
{
    return(E_NOTIMPL);
}
HRESULT PboFileStream::Stat(STATSTG* pstatstg, DWORD grfStatFlag)
{
    ZeroMemory(pstatstg, sizeof(STATSTG));

    pstatstg->cbSize.QuadPart = fileSize;
    pstatstg->type = STGTY_STREAM;
    pstatstg->grfMode = STGM_READ;

    if (!(grfStatFlag & STATFLAG_NONAME))
    {
        const wchar_t* name = filePath.filename().c_str();
        const wchar_t* delim = wcsrchr(name, '\\');
        if (!delim) delim = name;
        else delim++;
        SHStrDup(delim, &pstatstg->pwcsName);
    }

    //int64_t date = m_stream->moddate();
    //getFileTimeFromDate(date, &pstatstg->mtime);

    return(S_OK);
}
HRESULT PboFileStream::Clone(IStream**)
{
    return(E_NOTIMPL);
}

void PboFileStream::TransformToWritable() {
    if (tempFile)
        return;

    tempFile = TempDiskFile::GetFile(*pboFile, filePath);
    tempFileStream = std::fstream((*tempFile)->GetPath(), std::ifstream::binary | std::ifstream::in | std::ifstream::out);

    pboInputStream.close(); //invalidates pboReader, sourceStream!
}
