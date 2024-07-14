#pragma once
#define NOMINMAX

#include <ShlObj.h>
#include <Windows.h>

import std;
import std.compat;
import ComRef;
import PboLib;
#include "PboFolder.hpp"

class PboFile;
class TempDiskFile;

class PboFileStream :
    GlobalRefCounted,
	public RefCountedCOM<PboFileStream, IStream>
{
public:
    PboFileStream(std::shared_ptr<PboFile> pboFile, std::filesystem::path filePath);
    PboFileStream(std::shared_ptr<PboFile> pboFile, const PboSubFile& subFile);
    ~PboFileStream() override;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    // IStream
    HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override;
    HRESULT STDMETHODCALLTYPE Write(
        void const* pv, ULONG cb, ULONG* pcbWritten) override;
    HRESULT STDMETHODCALLTYPE Seek(
        LARGE_INTEGER dlibMove, DWORD dwOrigin,
        ULARGE_INTEGER* plibNewPosition) override;
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize) override;
    HRESULT STDMETHODCALLTYPE CopyTo(
        IStream* pstm, ULARGE_INTEGER cb,
        ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override;
    HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) override;
    HRESULT STDMETHODCALLTYPE Revert(void) override;
    HRESULT STDMETHODCALLTYPE LockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
    HRESULT STDMETHODCALLTYPE UnlockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
    HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD grfStatFlag) override;
    HRESULT STDMETHODCALLTYPE Clone(IStream** ppstm) override;

private:

    void TransformToWritable();


    LONGLONG m_pos;

    std::ifstream pboInputStream;
    PboReader pboReader;
    PboEntryBuffer fileBuffer;
    std::istream sourceStream;
    uint32_t fileSize;
    std::filesystem::path filePath;

    std::shared_ptr<PboFile> pboFile;

    std::optional<std::shared_ptr<TempDiskFile>> tempFile;
    std::fstream tempFileStream;
};

