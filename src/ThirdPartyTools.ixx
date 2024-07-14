module;

#include <Windows.h>

export module ThirdPartyTools; 



import std;
import std.compat;
import Registry;

//ReadRegistryFilePathKey


export enum class ToolCategory : uint16_t
{
	PboUnpack = 1 << 0,
	PboPack = 1 << 1,
	PboSign = 1 << 2,

	SupportsMultiple = 1 << 8
};

export constexpr ToolCategory operator&(ToolCategory x, ToolCategory y)
{
	return static_cast<ToolCategory>(static_cast<std::underlying_type_t<ToolCategory>>(x) & static_cast<std::underlying_type_t<ToolCategory>>(y));
}

export constexpr ToolCategory operator|(ToolCategory x, ToolCategory y)
{
	return static_cast<ToolCategory>(static_cast<std::underlying_type_t<ToolCategory>>(x) | static_cast<std::underlying_type_t<ToolCategory>>(y));
}

constexpr std::underlying_type_t<ToolCategory> operator+(ToolCategory x)
{
	return static_cast<std::underlying_type_t<ToolCategory>>(x);
}


export struct ToolExecutionContext {

	bool ExtractToSubfolders = false; //#TODO turn into flags and other stuff

};

struct IToolBase {
	virtual bool Initialize() = 0;
	virtual HRESULT Execute(std::span<const std::filesystem::path> files, ToolExecutionContext context) = 0;
	virtual std::string_view GetDisplayName() = 0;

	bool IsInitialized = false;
	ToolCategory toolCategory;

	IToolBase(ToolCategory toolCategory) : toolCategory(toolCategory) {}
};

export std::filesystem::path GetA3ToolsPath() {
	auto path = ReadRegistryFilePathKey(HKEY_CURRENT_USER, L"SOFTWARE\\Bohemia Interactive\\Arma 3 Tools", L"path");

	if (!path.empty())
		return path;

	// Check some random paths of known Arma tools to see if we can find it that way
	for (auto& it : {
			L"SOFTWARE\\Bohemia Interactive\\CfgConvert",
			L"SOFTWARE\\Bohemia Interactive\\Binarize",
			L"SOFTWARE\\Bohemia Interactive\\BankRev",
			L"SOFTWARE\\Bohemia Interactive\\DSUtils",
			L"SOFTWARE\\Bohemia Interactive\\ImageToPAA",
			L"SOFTWARE\\Bohemia Interactive\\Publisher",
			L"SOFTWARE\\Bohemia Interactive\\TerrainBuilder"
		}) {
		path = ReadRegistryFilePathKey(HKEY_CURRENT_USER, it, L"path");

		if (!path.empty())
			return path.parent_path();
	}

	// fail
	return {};
}

export std::filesystem::path GetMikeroToolsPath() {
	auto path = ReadRegistryFilePathKey(HKEY_CURRENT_USER, L"SOFTWARE\\Mikero\\DePbo", L"path");

	if (!path.empty())
		return path;

	// Check some other paths to see if we can find it that way
	for (auto& it : {
			L"SOFTWARE\\Mikero\\DeOgg",
			L"SOFTWARE\\Mikero\\DeRap",
			L"SOFTWARE\\Mikero\\ExtractPbo",
			L"SOFTWARE\\Mikero\\makePbo",
			L"SOFTWARE\\Mikero\\Rapify"
		}) {
		path = ReadRegistryFilePathKey(HKEY_CURRENT_USER, it, L"path");

		if (!path.empty())
			return path.parent_path();
	}

	// fail
	return {};
}

struct A3Tools_FileBankPack {


};

struct A3Tools_BankRevUnpack : public IToolBase {

	A3Tools_BankRevUnpack() : IToolBase(
		ToolCategory::PboUnpack | ToolCategory::SupportsMultiple
	) {}

	std::filesystem::path bankRevPath;

	bool Initialize() override {
		auto a3ToolsPath = GetA3ToolsPath();
		if (a3ToolsPath.empty())
			return false;

		bankRevPath = a3ToolsPath / "BankRev" / "BankRev.exe";

		if (!std::filesystem::exists(bankRevPath))
			return false;

		return true;
	}

	HRESULT Execute(std::span<const std::filesystem::path> files, ToolExecutionContext context) {

		/*
		Bankrev [-f|-folder destiantion] [-t|-time] PBO_file [PBO_file_2 ...]
				or -d|-diff PBO_file1 PBO_file2 (compare two PBOs)
				or -l|-log PBO_file (write to stdout list of files in archive)
				or -lf|-logFull PBO_file (write to stdout list of files in archive, path with prefix)
				or -p|-properties PBO_file (write out PBO properties)
				or -hash PBO_file (write out hash of PBO)
				or -statistics PBO_file (write the statistics of files lengths)
				or -prefix (the output folder will be set according to the PBO prefix)
				or -t|-time (keep file time from archive)
		*/

		SHELLEXECUTEINFO sei;
		ZeroMemory(&sei, sizeof(sei));
		sei.cbSize = (DWORD)sizeof(sei);
		sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
		auto path = bankRevPath.native();
		sei.lpFile = path.c_str();
		//#TODO can specify multiple files here and reuse exact same code for multipbo
		auto params = std::format(L"-t \"{}\"", files.front().native());
		sei.lpParameters = params.c_str();
		auto workDir = files.front().parent_path().native();
		sei.lpDirectory = workDir.c_str();
		//sei.hwnd = hwnd; //#TODO get HWND
		sei.nShow = SW_SHOWNORMAL;

		bool allGood = true;

		for (auto& it : files) {

			auto params = 
				(context.ExtractToSubfolders)
				? 
					std::format(L"-t \"{}\"", it.native())
				:
					std::format(L"-t \"{}\" -f .", it.native()) // -f . == current directory (workdir is parent)
				;
			sei.lpParameters = params.c_str();
			auto workDir = it.parent_path().native();
			sei.lpDirectory = workDir.c_str();

			allGood &= ShellExecuteEx(&sei) == TRUE;
		}

		return allGood ? S_OK : E_UNEXPECTED;
	}

	std::string_view GetDisplayName() override { return "BankRev (newT)"; }; //#TODO remove newT, just for me to show that this came from new ThirdPartyTools system
};

struct A3Tools_DSSignFile : public IToolBase {

	A3Tools_DSSignFile() : IToolBase(
		ToolCategory::PboSign | ToolCategory::SupportsMultiple
	) {}

	std::filesystem::path signFilePath;

	bool Initialize() override {
		auto a3ToolsPath = GetA3ToolsPath();
		if (a3ToolsPath.empty())
			return false;

		signFilePath = a3ToolsPath / "DSSignFile" / "DSSignFile.exe";

		if (!std::filesystem::exists(signFilePath))
			return false;

		return true;
	}

	HRESULT Execute(std::span<const std::filesystem::path> files, ToolExecutionContext context) {

		/*
		Usage: DSSignFile private_key file_to_sign [-v2]
			>> private_key - path to the bikeyprivate file used to sign the file
			>> file_to_sign - path to the file we want to sign
			>> -v2 - OPTIONAL - when used, the file is signed with a v2 signature (v3 by default)
		*/

		OPENFILENAMEW info{};

		info.lStructSize = sizeof(OPENFILENAMEW);
		//info.hwndOwner =  //#TODO
		info.lpstrFilter = L"BI Private Key\0*.biprivatekey\0\0";
		info.nFilterIndex = 1;
		info.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
		info.lpstrTitle = L"PBOExplorer: Please select the .biprivatekey to sign the PBO's with";

		auto workDir = files.front().parent_path().native();
		info.lpstrInitialDir = workDir.c_str();
		wchar_t pathBuffer[MAX_PATH]{};

		info.lpstrFile = pathBuffer;
		info.nMaxFile = MAX_PATH;

		if (!GetOpenFileNameW(&info))
			return E_FAIL;


		SHELLEXECUTEINFO sei;
		ZeroMemory(&sei, sizeof(sei));
		sei.cbSize = (DWORD)sizeof(sei);
		sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
		auto path = signFilePath.native();
		sei.lpFile = path.c_str();
		//sei.hwnd = hwnd; //#TODO get HWND
		sei.nShow = SW_SHOWNORMAL;

		bool allGood = true;

		for (auto& it : files) {
			auto params = std::format(L"\"{}\" \"{}\"", pathBuffer, it.native());
			sei.lpParameters = params.c_str();
			auto workDir = it.parent_path().native();
			sei.lpDirectory = workDir.c_str();

			allGood &= ShellExecuteEx(&sei) == TRUE;
		}

		return allGood ? S_OK : E_UNEXPECTED;
	}

	std::string_view GetDisplayName() override { return "DSSignFile (NewT)"; }; //#TODO remove newT, just for me to show that this came from new ThirdPartyTools system
};

struct Mikero_ExtractPboDos : public IToolBase {

	Mikero_ExtractPboDos() : IToolBase(
		ToolCategory::PboUnpack | ToolCategory::SupportsMultiple
	) {}

	std::filesystem::path extractPboPath;

	bool Initialize() override {
		auto mikeroToolsPath = GetMikeroToolsPath();
		if (mikeroToolsPath.empty())
			return false;

		extractPboPath = mikeroToolsPath / "bin" / "ExtractPboDos.exe";

		if (!std::filesystem::exists(extractPboPath))
			return false;

		return true;
	}

	HRESULT Execute(std::span<const std::filesystem::path> files, ToolExecutionContext context) {

		/*
		ExtractPbo Version 2.20, Dll 7.46 "no_file"

		Syntax: ExtractPbo [-options...] PboName[.pbo|.xbo|.ifa]|FolderName|Extraction.lst|.txt  [destination]

		destination *must* contain a drive specifier

		Options (case insensitive)

			-F=File(s) ToExtract[,...]
			-L list contents only (do not extract)
			-LB brief listing (dir style)
			-N Noisy
			-P Don't pause
			-W warnings are errors
		*/

		SHELLEXECUTEINFO sei;
		ZeroMemory(&sei, sizeof(sei));
		sei.cbSize = (DWORD)sizeof(sei);
		sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
		auto path = extractPboPath.native();
		sei.lpFile = path.c_str();
		//sei.hwnd = hwnd; //#TODO get HWND
		sei.nShow = SW_SHOWNORMAL;

		bool allGood = true;

		for (auto& it : files) {

			auto params =
				(context.ExtractToSubfolders)
				?
				std::format(L"{}\"{}\"", files.size() > 1 ? L"-P " : L"", it.native()) // silent mode if unpacking several
				:
				std::format(L"-P \"{}\" \"{}\"", it.native(), it.parent_path().native())
				;

			sei.lpParameters = params.c_str();
			auto workDir = it.parent_path().native();
			sei.lpDirectory = workDir.c_str();
			allGood &= ShellExecuteEx(&sei) == TRUE;
		}

		return allGood ? S_OK : E_UNEXPECTED;
	}

	std::string_view GetDisplayName() override { return "ExtractPbo (newT)"; }; //#TODO remove newT, just for me to show that this came from new ThirdPartyTools system
};


std::tuple ToolsList{
	A3Tools_BankRevUnpack{},
	//A3Tools_FileBankPack{},
	Mikero_ExtractPboDos{},
	A3Tools_DSSignFile{}
};


// https://stackoverflow.com/a/47762456
template <
	size_t Index = 0, // start iteration at 0 index
	typename TTuple,  // the tuple type
	size_t Size =
	std::tuple_size_v<
	std::remove_reference_t<TTuple>>, // tuple size
	typename TCallable, // the callable to be invoked for each tuple item
	typename... TArgs   // other arguments to be passed to the callable 
	>
	void tupleForEach(TTuple&& tuple, TCallable&& callable, TArgs&&... args)
{
	if constexpr (Index < Size)
	{
		std::invoke(callable, args..., std::get<Index>(tuple));

		if constexpr (Index + 1 < Size)
			tupleForEach<Index + 1>(
				std::forward<TTuple>(tuple),
				std::forward<TCallable>(callable),
				std::forward<TArgs>(args)...);
	}
}



void InitializeThirdPartyTools() {
	tupleForEach(ToolsList, [](auto& item) {
		item.IsInitialized = item.Initialize();
	});
}

std::once_flag InitializationCompleteFlag;

export std::vector<IToolBase*> GetToolsByCategory(ToolCategory cat) {

	// Make sure we're initialized
	std::call_once(InitializationCompleteFlag, InitializeThirdPartyTools);

	std::vector<IToolBase*> result;

	tupleForEach(ToolsList, [cat, &result](auto& item) {
		if ((item.toolCategory & cat) == cat)
			result.emplace_back(&item);
	});

	return result;
}


