module;

#include <shtypes.h>
#include <Shlwapi.h>
#include <shlobj.h>

export module ClipboardFormatHandler;


import std;
import Util;
#include "DebugLogger.hpp"



import ComRef;


struct IDataObject;

export class ClipboardFormatHandler
{
public:


	enum class ClipboardFormatType {
		ShellIDList,
		FileName,
		FileContents,
		HDROP,
		FileGroupDescriptor,
		_LastSupportedItem = FileGroupDescriptor, // always last supported type in ClipboardFormatHandler
		PreferredDropEffect,
		PerformedDropEffect,
		OleClipboardPersistOnFlush,
		IsShowingText,
		DropDescription,
		PasteSucceeded
	};

	struct FilePathResult {
		std::filesystem::path path;
		// if true, we need to delete the file when we're done with it.
		bool isTemporary;
	};



	void ReadFromFast(IDataObject* dataObject);
	void LogFormats(IDataObject* dataObject);


	bool CanReadAsFile() const {

		return supportedFormats.size() >= (int)ClipboardFormatType::FileContents &&
			(
				supportedFormats[(int)ClipboardFormatType::ShellIDList] ||
				supportedFormats[(int)ClipboardFormatType::FileContents] ||
				supportedFormats[(int)ClipboardFormatType::HDROP]
				);
	}


	// Retrieves disk paths to all files in clipboard, such that they can be read/copied from disk
	std::vector<FilePathResult> GetFilePathsToRead(IDataObject* dataObject) const;
	std::vector<CoTaskMemRefS<ITEMIDLIST>> GetPidlsToRead(IDataObject* dataObject) const;

	static CLIPFORMAT GetCFFromType(ClipboardFormatType type);
	static std::optional<ClipboardFormatType> GetTypeFromCF(CLIPFORMAT cf);



private:

	std::vector<std::any> formats;
	std::vector<bool> supportedFormats;


};




import Encoding;
import GlobalCache;

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
    supportedFormats.resize((int)ClipboardFormatType::_LastSupportedItem + 1);

    ComRef<IEnumFORMATETC> result = nullptr;
    if (dataObject->EnumFormatEtc(DATADIR_GET, result.AsQueryInterfaceTarget<IEnumFORMATETC>()) == S_OK) {

        FORMATETC format{};

        ULONG fetched = 0;
        while (result->Next(1, &format, &fetched) == S_OK && fetched) {
            wchar_t buffer[128];
            GetClipboardFormatNameW(format.cfFormat, buffer, 127);


            DebugLogger::TraceLog(std::format("formatName {}, format {}, tymed {}", UTF8::Encode(buffer), format.cfFormat, TymedSeperator.SeperateToString((tagTYMED)format.tymed)), std::source_location::current(), __FUNCTION__);

            ClipboardFormatType type;

            switch (format.cfFormat) {
            case CF_HDROP: type = ClipboardFormatType::HDROP; break;
            default: {
                auto type = GetTypeFromCF(format.cfFormat);
                if (!type) {
                    DebugLogger::TraceLog(std::format("Unkown format formatName {}, format {}, tymed {}", UTF8::Encode(buffer), format.cfFormat, TymedSeperator.SeperateToString((tagTYMED)format.tymed)), std::source_location::current(), __FUNCTION__);
                    continue; // unknown type we don't care about?
                }
                else
                    type = *type;
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
    }

}

void ClipboardFormatHandler::LogFormats(IDataObject* dataObject)
{
    ComRef<IEnumFORMATETC> result = nullptr;
    if (dataObject->EnumFormatEtc(DATADIR_GET, result.AsQueryInterfaceTarget<IEnumFORMATETC>()) == S_OK) {

        FORMATETC format{};

        ULONG fetched = 0;
        while (result->Next(1, &format, &fetched) == S_OK && fetched) {
            wchar_t buffer[128];
            GetClipboardFormatNameW(format.cfFormat, buffer, 127);

            DebugLogger::TraceLog(std::format("formatName {}, format {}, tymed {}", UTF8::Encode(buffer), format.cfFormat, TymedSeperator.SeperateToString((tagTYMED)format.tymed)), std::source_location::current(), __FUNCTION__);
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

        {
            ComRef<IEnumFORMATETC> result;
            if (dataObject->EnumFormatEtc(DATADIR_GET, result.AsQueryInterfaceTarget<IEnumFORMATETC>()) == S_OK) {
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

std::vector<CoTaskMemRefS<ITEMIDLIST>> ClipboardFormatHandler::GetPidlsToRead(IDataObject* dataObject) const
{
    std::vector<CoTaskMemRefS<ITEMIDLIST>> result;

    if (!supportedFormats[(int)ClipboardFormatType::ShellIDList]) return result;


    CLIPFORMAT ShellIDListFormat = GetCFFromType(ClipboardFormatType::ShellIDList);
    FORMATETC format{};
    bool formatFound = false;

    {
        ComRef<IEnumFORMATETC> result;
        if (dataObject->EnumFormatEtc(DATADIR_GET, result.AsQueryInterfaceTarget<IEnumFORMATETC>()) == S_OK) {
            ULONG fetched = 0;
            while (result->Next(1, &format, &fetched) == S_OK && fetched) {
                if (format.cfFormat == ShellIDListFormat) {
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

    auto pdw = (CIDA*)GlobalLock(medium.hGlobal);

    // first element is parent folder of our actual element

    for (size_t i = 1; i < pdw->cidl + 1; i++)
    {
        BYTE* p = (BYTE*)pdw + pdw->aoffset[i];

        result.emplace_back(ILClone((LPCITEMIDLIST)p));
    }


    GlobalUnlock(medium.hGlobal);
    GlobalFree(medium.hGlobal);


    return result;
}


#ifndef CFSTR_OLECLIPBOARDPERSISTONFLUSH
#define CFSTR_OLECLIPBOARDPERSISTONFLUSH TEXT("OleClipboardPersistOnFlush")
#endif

CLIPFORMAT ClipboardFormatHandler::GetCFFromType(ClipboardFormatType type)
{
    const auto& clipboardTypes = GCache.GetFromCache("CFH_RegCFSTRM"sv, []() {
        std::unordered_map<ClipboardFormatType, CLIPFORMAT> assocMap{
            {ClipboardFormatType::ShellIDList, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_SHELLIDLIST)},
            {ClipboardFormatType::FileContents, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILECONTENTS)},
            {ClipboardFormatType::FileGroupDescriptor, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW)},
            {ClipboardFormatType::FileName, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILENAMEW)},
            {ClipboardFormatType::PreferredDropEffect, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT)},
            {ClipboardFormatType::OleClipboardPersistOnFlush, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_OLECLIPBOARDPERSISTONFLUSH)},
            {ClipboardFormatType::IsShowingText, (CLIPFORMAT)RegisterClipboardFormat(L"IsShowingText")},
            {ClipboardFormatType::DropDescription, (CLIPFORMAT)RegisterClipboardFormat(L"DropDescription")},
            {ClipboardFormatType::PasteSucceeded, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_PASTESUCCEEDED)},
            {ClipboardFormatType::PerformedDropEffect, (CLIPFORMAT)RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT)},
        };
        return assocMap;
        }
    );

    auto found = clipboardTypes.find(type);

    if (found == clipboardTypes.end()) {
        Util::TryDebugBreak();
        return CLIPFORMAT();
    }
    else
        return found->second;
}

std::optional<ClipboardFormatHandler::ClipboardFormatType> ClipboardFormatHandler::GetTypeFromCF(CLIPFORMAT cf)
{
    const auto& clipboardTypes = GCache.GetFromCache("CFH_RegCFSTR"sv, []() {
        std::vector<std::pair<CLIPFORMAT, ClipboardFormatType>> assocVec;

        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_SHELLIDLIST), ClipboardFormatType::ShellIDList);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILECONTENTS), ClipboardFormatType::FileContents);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA), ClipboardFormatType::FileGroupDescriptor);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW), ClipboardFormatType::FileGroupDescriptor);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILENAMEA), ClipboardFormatType::FileName);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILENAMEW), ClipboardFormatType::FileName);

        // not used by ClipboardFormatHandler itself, but by other components
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT), ClipboardFormatType::PreferredDropEffect);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_OLECLIPBOARDPERSISTONFLUSH), ClipboardFormatType::OleClipboardPersistOnFlush);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(L"IsShowingText"), ClipboardFormatType::IsShowingText);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(L"DropDescription"), ClipboardFormatType::DropDescription);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_PASTESUCCEEDED), ClipboardFormatType::PasteSucceeded);
        assocVec.emplace_back((CLIPFORMAT)RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT), ClipboardFormatType::PerformedDropEffect);

        std::ranges::sort(assocVec, {}, [](const auto& el) {return el.first; });

        return assocVec;
        }
    );


    auto found = std::ranges::lower_bound(clipboardTypes, cf, {}, [](const auto& el) {return el.first; });

    if (found == clipboardTypes.end() || found->first != cf)
        return {};
    else
        return found->second;
}