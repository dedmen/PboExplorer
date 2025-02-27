module;
#define NOMINMAX
#include <thumbcache.h>
#include "DebugLogger.hpp"
#include "PboFileDirectory.hpp"
#include "PboFolder.hpp"
#include <gdiplus.h>

#include "PboFileStream.hpp"
#pragma comment(lib, "Gdiplus.lib")

export module PaaThumbnailProvider;

import ComRef;
import Tracy;
import Util;
import PboLib;
import MipMapTool;

class IStreamBufWrapper : public std::streambuf {
    std::vector<char> buffer;
    IStream* source;
    size_t totalSize = 0;
    // Position in source file, of the character after the last character that's currently in our buffer
    size_t bufferEndFilePos{ 0 };
public:
    IStreamBufWrapper(IStream* source) : buffer(4096), source(source){
        char* start = &buffer.front();
        setg(start, start, start);

        STATSTG stats {};
        source->Stat(&stats, STATFLAG_NONAME);
        totalSize = stats.cbSize.QuadPart;
    }

    int underflow() override {
        if (gptr() < egptr()) // buffer not exhausted
            return traits_type::to_int_type(*gptr());

        LARGE_INTEGER seekOffs;
        seekOffs.QuadPart = bufferEndFilePos;
        source->Seek(seekOffs, STREAM_SEEK_SET, nullptr);

        ULONG bytesRead = 0;
        source->Read(buffer.data(), buffer.size(), &bytesRead);
        bufferEndFilePos += bytesRead;

        setg(&buffer.front(), &buffer.front(), &buffer.front() + bytesRead);

        return std::char_traits<char>::to_int_type(*this->gptr());
    }
    int64_t xsgetn(char* _Ptr, int64_t _Count) override {
        // get _Count characters from stream
        const int64_t _Start_count = _Count;

        while (_Count) {
            size_t dataLeft = egptr() - gptr();
            if (dataLeft == 0) {

                LARGE_INTEGER seekOffs;
                seekOffs.QuadPart = bufferEndFilePos;
                source->Seek(seekOffs, STREAM_SEEK_SET, nullptr);

                ULONG bytesRead = 0;
                source->Read(buffer.data(), buffer.size(), &bytesRead);
                bufferEndFilePos += bytesRead;
                setg(&buffer.front(), &buffer.front(), &buffer.front() + bytesRead);

                dataLeft = std::min((size_t)bytesRead, (size_t)_Count);
            }
            else
                dataLeft = std::min(dataLeft, (size_t)_Count);

            std::copy(gptr(), gptr() + dataLeft, _Ptr);
            _Ptr += dataLeft;
            _Count -= dataLeft;
            gbump(dataLeft);
        }

        return (_Start_count - _Count);
    }
    pos_type seekoff(off_type offs, std::ios_base::seekdir dir, std::ios_base::openmode mode) override {
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
            bufferEndFilePos = totalSize + offs;
            setg(&buffer.front(), &buffer.front(), &buffer.front()); //no data available
            return bufferEndFilePos;
            break;
        }
        return -1; //#TODO this is error
    }
    pos_type seekpos(pos_type offs, std::ios_base::openmode mode) override {
        return seekoff(offs, std::ios_base::beg, mode);
    }
    std::streamsize showmanyc() override {
        //How many characters are left in current buffer
        size_t dataInBuffer = egptr() - gptr();
        ULARGE_INTEGER sourceFilePos;
        source->Seek(LARGE_INTEGER{}, STREAM_SEEK_CUR, &sourceFilePos);
        size_t dataInSource = totalSize - sourceFilePos.QuadPart;

        return dataInBuffer + dataInSource;
    }
};

export class PaaThumbnailProvider :
    GlobalRefCounted,
    public RefCountedCOM< PaaThumbnailProvider,
    IInitializeWithStream,
    IInitializeWithFile,
    IThumbnailProvider
    > {

    struct PboFileContent {
        PboSubFile imageFile;
        std::shared_ptr<IPboFolder> pboFile;
    };

    std::variant<PboFileContent, ComRef<IStream>, std::filesystem::path> contentSource;
public:
    PaaThumbnailProvider(PboSubFile imageFile, std::shared_ptr<IPboFolder> mainFolder, HWND hwnd) : contentSource(PboFileContent{ std::move(imageFile), std::move(mainFolder) }) {

    }
    PaaThumbnailProvider() : contentSource() {

    }

    HRESULT QueryInterface(const IID& riid, void** ppvObject) override {
        ProfilingScope pScope;
        pScope.SetValue(DebugLogger::GetGUIDName(riid).first);
        DebugLogger_OnQueryInterfaceEntry(riid);

        if (COMJoiner::QueryInterfaceJoiner(riid, ppvObject)) {
            AddRef();
            return S_OK;
        }
        else
        {
            DebugLogger_OnQueryInterfaceExitUnhandled(riid);
            *ppvObject = nullptr;
            return(E_NOINTERFACE);
        }
    }

    HRESULT Initialize(LPCWSTR pszFilePath, DWORD grfMode) override {
        contentSource = std::filesystem::path(pszFilePath);
        return S_OK;
    }

    HRESULT Initialize(IStream* pStream, DWORD grfMode) override {
        ComRef<IStream> myStream;
        pStream->QueryInterface(IID_IStream, myStream.AsQueryInterfaceTarget());
        contentSource = std::move(myStream);
        return S_OK;
    }

    auto LoadTextureFromPbo(uint16_t maxSize, const PboFileContent& arg) {

        //#TODO somehow unify these, std::istream sublass that just stores these wrappers internally?
        std::ifstream pboInputStream(arg.pboFile->GetRootFile()->diskPath, std::ios::in | std::ios::binary);
        PboReader pboReader(pboInputStream);
        PboEntry ent{ "", arg.imageFile.filesize, arg.imageFile.dataSize, arg.imageFile.startOffset, PboEntryPackingMethod::none }; // PAA could be compressed, but I've never seen one
        PboEntryBuffer fileBuffer(pboReader, ent);
        std::istream sourceStream(&fileBuffer);

        return MipMapTool::LoadRGBATexture(sourceStream, maxSize);
    }

    auto LoadTextureFromStream(uint16_t maxSize, ComRef<IStream>& arg)
    {
        IStreamBufWrapper wrapper(arg);
        std::istream sourceStream(&wrapper);

        return MipMapTool::LoadRGBATexture(sourceStream, maxSize);
    }

    auto LoadTextureFromFile(uint16_t maxSize, std::filesystem::path arg)
    {
        std::ifstream sourceStream(arg);
        return MipMapTool::LoadRGBATexture(sourceStream, maxSize);
    }


    auto LoadTexture(uint16_t maxSize)
    {
        if (contentSource.index() == 0)
            return LoadTextureFromPbo(maxSize, std::get<PboFileContent>(contentSource));
        if (contentSource.index() == 1)
            return LoadTextureFromStream(maxSize, std::get<ComRef<IStream>>(contentSource));
        if (contentSource.index() == 2)
            return LoadTextureFromFile(maxSize, std::get<std::filesystem::path>(contentSource));
        else
            Util::TryDebugBreak();

        // Garbage dummy, invalid
        return LoadTextureFromStream(maxSize, std::get<ComRef<IStream>>(contentSource));

        // Internal compiler error :)
        //return std::visit([this, maxSize](auto&& arg)
        //    {
        //        using T = std::decay_t<decltype(arg)>;
        //        if constexpr (std::is_same_v<T, PboFileContent>)
        //            return LoadTextureFromPbo(maxSize, arg);
        //        else if constexpr (std::is_same_v<T, ComRef<IStream>>)
        //            return LoadTextureFromStream(maxSize, arg);
        //        else
        //            static_assert(false, "non-exhaustive visitor!");
        //    }, contentSource);
    }


    std::tuple<int, int, std::string_view> GetTextureMetaInfo()
    {
        if (contentSource.index() == 0) {
            auto& arg = std::get<PboFileContent>(contentSource);

            std::ifstream pboInputStream(arg.pboFile->GetRootFile()->diskPath, std::ios::in | std::ios::binary);
            PboReader pboReader(pboInputStream);
            PboEntry ent{ "", arg.imageFile.filesize, arg.imageFile.dataSize, arg.imageFile.startOffset, PboEntryPackingMethod::none }; // PAA could be compressed, but I've never seen one
            PboEntryBuffer fileBuffer(pboReader, ent);
            std::istream sourceStream(&fileBuffer);
            return MipMapTool::LoadTextureMetaInfo(sourceStream);
        }

        if (contentSource.index() == 1) {
            auto& arg = std::get<ComRef<IStream>>(contentSource);
            IStreamBufWrapper wrapper(arg);
            std::istream sourceStream(&wrapper);

            return MipMapTool::LoadTextureMetaInfo(sourceStream);
        }
        if (contentSource.index() == 2) {
            auto& arg = std::get<std::filesystem::path>(contentSource);
            std::ifstream sourceStream(arg);

            return MipMapTool::LoadTextureMetaInfo(sourceStream);
        }
        return { 0,0, std::string_view() };
    }

    HRESULT GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override
    {
        auto [tex, width, height] = LoadTexture(cx);

        // Input data is RGBA, we need ARGB
        std::span<uint32_t> x(reinterpret_cast<uint32_t*>(tex.data()), tex.size() / sizeof(uint32_t));
        uint32_t idx = 0;
        for (auto& it : x) {

            //if (it << 24 == 0) {
            //    // Replace transparency by pattern
            //    auto column = idx % width;
            //    auto row = idx / width;
            //
            //    bool alternate = ((column % 8) == 0) != ((row % 8) == 0);
            //    it = alternate ? 0xFF00FFFF : 0x000000FF; // RGBA, will be flipped below
            //}

            it = it >> 8 | it << 24;

            ++idx;
        }

        //BITMAPFILEHEADER bfh;
        //ZeroMemory(&bfh, sizeof(BITMAPFILEHEADER));
        //bfh.bfType = 0x4d42;
        //bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO) + tex.size();
        //bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO);

        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biPlanes = 1;

        HDC dc = GetDC(0);
        HBITMAP bmp = CreateCompatibleBitmap(dc, width, height);
        auto r1 = SetDIBits(dc, bmp, 0, height, tex.data(), &bmi, DIB_RGB_COLORS);
        *phbmp = bmp;

        // The GDI way, works. But is more expensive and the basic way works fine (after alot of troubleshooting...)
        /*
        uint64_t gdiToken;

        Gdiplus::GdiplusStartupInput gdiInp;
        gdiInp.DebugEventCallback = [](Gdiplus::DebugEventLevel debugEventLevel, char* string) {
            ;
        };
        gdiInp.SuppressBackgroundThread = true;
        gdiInp.GdiplusVersion = 1;
        Gdiplus::GdiplusStartupOutput gdiOutp;
        Gdiplus::GdiplusStartup(&gdiToken, &gdiInp, &gdiOutp);

        //Gdiplus::Bitmap* pBitmap = NULL;
        //IStream* pStream = NULL;
        //
        //HRESULT hResult = ::CreateStreamOnHGlobal(NULL, TRUE, &pStream);
        //if (hResult == S_OK && pStream)
        //{
        //    hResult = pStream->Write(&bfh, sizeof(BITMAPFILEHEADER), NULL);
        //    hResult = pStream->Write(&bmi, sizeof(BITMAPINFO), NULL);
        //    hResult = pStream->Write(tex.data(), ULONG(tex.size()), NULL);
        //    if (hResult == S_OK)
        //        pBitmap = Gdiplus::Bitmap::FromStream(pStream);
        //    Gdiplus::GpBitmap* bitmap = NULL;
        //    auto r1 = Gdiplus::DllExports::GdipCreateBitmapFromStream(pStream, &bitmap);
        //
        //    Gdiplus::DllExports::GdipCreateHBITMAPFromBitmap(bitmap, phbmp, 0x00000000);
        //    auto err = GetLastError();
        //    pStream->Release();
        //}

        auto pBitmap = new Gdiplus::Bitmap(&bmi, tex.data());
        if (!pBitmap) {
            return E_INVALIDARG;
        }
        auto stats = pBitmap->GetHBITMAP(Gdiplus::Color(255, 255, 0, 0), phbmp);
        delete pBitmap;
        Gdiplus::GdiplusShutdown(gdiToken);
        */

        *pdwAlpha = WTSAT_ARGB;
        return S_OK;
    }
};
