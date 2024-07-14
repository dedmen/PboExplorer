module;
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



export class PaaThumbnailProvider :
    GlobalRefCounted,
    public RefCountedCOM< PaaThumbnailProvider,
    IInitializeWithStream,
    IThumbnailProvider
    > {

    PboSubFile imageFile;
    std::shared_ptr<IPboFolder> pboFile;
    HWND hwnd;

public:
    PaaThumbnailProvider(PboSubFile imageFile, std::shared_ptr<IPboFolder> mainFolder, HWND hwnd) : imageFile(imageFile), pboFile(std::move(mainFolder)), hwnd(hwnd) {

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
    HRESULT Initialize(IStream* pstream, DWORD grfMode) override {
        Util::TryDebugBreak();
        return E_NOINTERFACE;
    }
    HRESULT GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override
    {


        std::ifstream pboInputStream(pboFile->GetRootFile()->diskPath, std::ios::in | std::ios::binary);
        PboReader pboReader(pboInputStream);
        PboEntry ent{ "", imageFile.filesize, imageFile.dataSize, imageFile.startOffset, PboEntryPackingMethod::none }; // PAA could be compressed, but I've never seen one
        PboEntryBuffer fileBuffer(pboReader, ent);
        std::istream sourceStream(&fileBuffer);

        auto [tex, width, height] = MipMapTool::LoadRGBATexture(sourceStream, cx);

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
