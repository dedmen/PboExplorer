#pragma once


#include <vector>
#include <variant>
#include <any>
#include <filesystem>

#include <shtypes.h>
#include "ComRef.hpp"


struct IDataObject;

class ClipboardFormatHandler
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

