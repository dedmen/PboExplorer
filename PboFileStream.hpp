#pragma once
#include <memory>
#include <ShlObj.h>
#include <Windows.h>

#include "ComRef.hpp"
#include "PboFolder.hpp"
class PboFile;

class PboFileStream :
    GlobalRefCounted,
	public RefCountedCOM<PboFileStream, IStream>
{
public:
    PboFileStream(std::shared_ptr<PboFile> pboFile, std::filesystem::path filePath);
    ~PboFileStream() override;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    // IStream
    HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead);
    HRESULT STDMETHODCALLTYPE Write(
        void const* pv, ULONG cb, ULONG* pcbWritten);
    HRESULT STDMETHODCALLTYPE Seek(
        LARGE_INTEGER dlibMove, DWORD dwOrigin,
        ULARGE_INTEGER* plibNewPosition);
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize);
    HRESULT STDMETHODCALLTYPE CopyTo(
        IStream* pstm, ULARGE_INTEGER cb,
        ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten);
    HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags);
    HRESULT STDMETHODCALLTYPE Revert(void);
    HRESULT STDMETHODCALLTYPE LockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    HRESULT STDMETHODCALLTYPE UnlockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD grfStatFlag);
    HRESULT STDMETHODCALLTYPE Clone(IStream** ppstm);

private:


    LONGLONG m_pos;

    std::ifstream pboInputStream;
    PboReader pboReader;
    PboEntryBuffer fileBuffer;
    std::istream sourceStream;
    uint32_t fileSize;
    std::filesystem::path filePath;
};

