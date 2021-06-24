#pragma once


#include <vector>
#include <variant>
#include <any>
#include <filesystem>


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
	};

	struct FilePathResult {
		std::filesystem::path path;
		// if true, we need to delete the file when we're done with it.
		bool isTemporary;
	};



	void ReadFromFast(IDataObject* dataObject);


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

private:

	std::vector<std::any> formats;
	std::vector<bool> supportedFormats;


};

