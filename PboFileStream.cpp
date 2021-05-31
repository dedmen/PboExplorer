#include "PboFileStream.hpp"


/*
 * Copyright (C) 2012-2015 Hannes Domani.
 * For conditions of distribution and use, see copyright notice in LICENSE.txt
 */

#include "PboFileStream.hpp"
#include <shlwapi.h>

#include "ClassFactory.hpp"
#include "Util.hpp"


PboFileStream::PboFileStream(std::shared_ptr<PboFile> pboFile, std::filesystem::path filePath):
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
    if (IsEqualIID(riid, IID_IStream))
        *ppvObject = (IStream*)this;
    else if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = (IUnknown*)this;
    else
    {
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

    //dirMutex.lock();
    if (sourceStream.seekg(m_pos))
    {
        sourceStream.read((char*)pv, cb);
        didRead = sourceStream.gcount();
    }
    
    //dirMutex.unlock();

    if (!didRead && !m_pos) return(E_FAIL);

    m_pos += didRead;
    *pcbRead = didRead;

    return(cb == didRead ? S_OK : S_FALSE);
}
HRESULT PboFileStream::Write(void const*, ULONG, ULONG*)
{
    // write into memory buffer


    return(E_NOTIMPL);
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
HRESULT PboFileStream::SetSize(ULARGE_INTEGER)
{
    return(E_NOTIMPL);
}
HRESULT PboFileStream::CopyTo(
    IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*)
{
    return(E_NOTIMPL);
}
HRESULT PboFileStream::Commit(DWORD)
{
    // write to pbo file
    return(E_NOTIMPL);
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
