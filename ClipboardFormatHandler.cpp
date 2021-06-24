#include "ClipboardFormatHandler.h"

#include <Shlwapi.h>
#include <format>
#include "DebugLogger.hpp"
#include "Util.hpp"

static constexpr Util::FlagSeperator<tagTYMED, 7> TymedSeperator({
      { TYMED_HGLOBAL, "TYMED_HGLOBAL"sv },
      { TYMED_FILE, "TYMED_FILE"sv },
      { TYMED_ISTREAM, "TYMED_ISTREAM"sv },
      { TYMED_ISTORAGE, "TYMED_ISTORAGE"sv },
      { TYMED_GDI, "TYMED_GDI"sv },
      { TYMED_MFPICT, "TYMED_MFPICT"sv },
      { TYMED_ENHMF, "TYMED_ENHMF"sv }
    });


void ClipboardFormatHandler::ReadFromFast(IDataObject* dataObject)
{

    //#TODO get real max size
    supportedFormats.resize((int)ClipboardFormatType::FileGroupDescriptor + 1);

    IEnumFORMATETC* result = nullptr;
    if (dataObject->EnumFormatEtc(DATADIR_GET, &result) == S_OK) {

        FORMATETC format{};

        ULONG fetched = 0;
        while (result->Next(1, &format, &fetched) == S_OK && fetched) {
            wchar_t buffer[128];
            GetClipboardFormatNameW(format.cfFormat, buffer, 127);


            DebugLogger::TraceLog(std::format("formatName {}, format {}, tymed {}", Util::utf8_encode(buffer), format.cfFormat, TymedSeperator.SeperateToString((tagTYMED)format.tymed)), std::source_location::current(), __FUNCTION__);


            //#TODO make lookup table from CFSTR_SHELLIDLIST, we need to call RegisterClipboardFormat to get the correct ID's, do that once at dll load and cache it

            ClipboardFormatType type;

            switch (format.cfFormat) {
                case 49405: type = ClipboardFormatType::ShellIDList; break;
                case 49158: type = ClipboardFormatType::FileName; break;
                case 49329: type = ClipboardFormatType::FileContents; break;
                case 49159: type = ClipboardFormatType::FileName; break; //FileNameW
                case 49410: type = ClipboardFormatType::FileGroupDescriptor; break;
                case CF_HDROP: type = ClipboardFormatType::HDROP; break;
                default: {
                    DebugLogger::TraceLog(std::format("Unkown format formatName {}, format {}, tymed {}", Util::utf8_encode(buffer), format.cfFormat, TymedSeperator.SeperateToString((tagTYMED)format.tymed)), std::source_location::current(), __FUNCTION__);
                    continue; // unknown type we don't care about?
                }
            }

            if (format.tymed == TYMED_HGLOBAL) {


                switch (type) {
                case ClipboardFormatType::ShellIDList: { supportedFormats[(int)ClipboardFormatType::ShellIDList] = true; } break;
                case ClipboardFormatType::FileName: { supportedFormats[(int)ClipboardFormatType::FileName] = true; } break;
                case ClipboardFormatType::FileContents: { supportedFormats[(int)ClipboardFormatType::FileContents] = true; } break;
                case ClipboardFormatType::FileGroupDescriptor: { supportedFormats[(int)ClipboardFormatType::FileGroupDescriptor] = true; } break;
                case ClipboardFormatType::HDROP: { supportedFormats[(int)ClipboardFormatType::HDROP] = true; } break;
                }
            }

            // https://docs.microsoft.com/en-us/windows/win32/shell/clipboard#cfstr_shellidlist


            // CF_HDROP 
            // CFSTR_SHELLIDLIST
            // CFSTR_FILEDESCRIPTORA
            // CFSTR_FILECONTENTS
            // CFSTR_FILENAMEA
            // CFSTR_SHELLURL
            // CFSTR_DRAGCONTEXT
            if (format.cfFormat == CF_TEXT && format.tymed == TYMED_FILE) {

            }
        }

        result->Release();
    }

}

std::vector<ClipboardFormatHandler::FilePathResult> ClipboardFormatHandler::GetFilePathsToRead(IDataObject* dataObject) const
{
    std::vector<FilePathResult> result;
    
    if (supportedFormats[(int)ClipboardFormatType::HDROP]) {


        FORMATETC format{};
        bool formatFound = false;
        //#TODO use comptr
        {
            IEnumFORMATETC* result = nullptr;
            if (dataObject->EnumFormatEtc(DATADIR_GET, &result) == S_OK) {
                ULONG fetched = 0;
                while (result->Next(1, &format, &fetched) == S_OK && fetched) {
                    if (format.cfFormat == CF_HDROP) {
                        formatFound = true;
                        break;
                    }
                }
                result->Release();
            }
        }

        if (!formatFound) {
            Util::TryDebugBreak();
            return result;
        }
            
        STGMEDIUM medium;
        if (dataObject->GetData(&format, &medium) != S_OK) {
            Util::TryDebugBreak();
            return result;
        }

        auto pdw = (DROPFILES*)GlobalLock(medium.hGlobal);

        if (pdw->fWide) {
            // wchar

            wchar_t* filenameStart = (wchar_t*)((uintptr_t)pdw + pdw->pFiles);
            do {
                std::wstring_view filename(filenameStart);

                result.emplace_back(FilePathResult{ std::filesystem::path(filename), false });

                filenameStart += filename.length() + 1;
            } while (*filenameStart != 0);
        }
        else
        {
            // char
            Util::TryDebugBreak();

            char* filenameStart = (char*)((uintptr_t)pdw + pdw->pFiles);
            do {
                std::string_view filename(filenameStart);

                result.emplace_back(FilePathResult{ std::filesystem::path(filename), false });

                filenameStart += filename.length();
            } while (*filenameStart != 0);
        }

       
        GlobalUnlock(medium.hGlobal);
        GlobalFree(medium.hGlobal);
    }



    return result;
}
