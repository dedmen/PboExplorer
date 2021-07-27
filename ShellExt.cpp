#include "ShellExt.hpp"

#include "resource.h"
#include "DebugLogger.hpp"
#include "GlobalCache.hpp"

#include <fstream> // Addon builder reading config file
#include "PboFileDirectory.hpp"
#include "PboPatcherLocked.hpp"

// CreatePropertySheetPageW
#pragma comment( lib, "Comctl32" )


//#TODO util? https://github.com/vbaderks/msf/blob/4e91fcc409d196f8ddc350a467f484daf40fc0e1/include/msf/cf_shell_id_list.h
// https://www.codeproject.com/Articles/11674/The-Mini-Shell-Extension-Framework-Part-III#ishellfolder_interface
// https://github.com/vbaderks/msf/blob/4e91fcc409d196f8ddc350a467f484daf40fc0e1/include/msf/shell_folder_impl.h#L1146




ShellExt::ShellExt()
{

}

ShellExt::~ShellExt()
{

}


STDMETHODIMP ShellExt::QueryInterface(REFIID riid, LPVOID* ppReturn)
{
	DebugLogger_OnQueryInterfaceEntry(riid);
	*ppReturn = nullptr;

#ifdef _DEBUG
	if (IsEqualIID(riid, IID_IObjectWithSite))
		return E_NOINTERFACE;
	if (IsEqualIID(riid, IID_IInternetSecurityManager))
		return E_NOINTERFACE;
#endif

	if (COMJoiner::QueryInterfaceJoiner(riid, ppReturn)) {
		AddRef();
		return S_OK;
	}
		

	//if (IsEqualIID(riid, IID_IClassFactory))
	//	*ppReturn = static_cast<IClassFactory*>(this);
	//else if (IsEqualIID(riid, IID_IShellFolder2))
	//	*ppReturn = static_cast<IShellFolder2*>(this);
	//else
	//	__debugbreak();
	


	//if (*ppReturn)
	//{
	//	LPUNKNOWN pUnk = (LPUNKNOWN)(*ppReturn);
	//	pUnk->AddRef();
	//	return S_OK;
	//}

	DebugLogger_OnQueryInterfaceExitUnhandled(riid);
	return E_NOINTERFACE;
}

STDMETHODIMP ShellExt::Initialize(
	LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hProgID)
{
	FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stg = { TYMED_HGLOBAL };
	HDROP     hDrop;

	// Look for CF_HDROP data in the data object.
	if (FAILED(pDataObj->GetData(&fmt, &stg)))
	{
		// Nope! Return an "invalid argument" error back to Explorer.
		return E_INVALIDARG;
	}

	// Get a pointer to the actual data.
	hDrop = (HDROP)GlobalLock(stg.hGlobal);

	// Make sure it worked.
	if (nullptr == hDrop)
		return E_INVALIDARG;

	// Sanity check - make sure there is at least one filename.
	UINT uNumFiles = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
	HRESULT hr = S_OK;

	if (0 == uNumFiles)
	{
		GlobalUnlock(stg.hGlobal);
		ReleaseStgMedium(&stg);
		return E_INVALIDARG;
	}

	TCHAR m_szFile[MAX_PATH];
	for (size_t i = 0; i < uNumFiles; i++)
	{
		// Get the name of the first file and store it in our member variable m_szFile.
		if (0 == DragQueryFile(hDrop, i, m_szFile, MAX_PATH))
			hr = E_INVALIDARG;

		selectedFiles.push_back(m_szFile);
	}

	GlobalUnlock(stg.hGlobal);
	ReleaseStgMedium(&stg);

	contextMenu = QueryContextMenuFromCache();

	return hr;
}

STDMETHODIMP ShellExt::QueryContextMenu(
	HMENU hmenu, UINT uMenuIndex, UINT uidFirstCmd,
	UINT uidLastCmd, UINT uFlags)
{
	// If the flags include CMF_DEFAULTONLY then we shouldn't do anything.
	if (uFlags & CMF_DEFAULTONLY)
		return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);

	//InsertMenu(hmenu, uMenuIndex, MF_BYPOSITION, uidFirstCmd, L"PboExplorer Test Item");

	auto startIndex = uMenuIndex;
	cmdFirst = uidFirstCmd;

	for (auto& it : contextMenu)
		it.InsertIntoMenu(hmenu, uMenuIndex, uidFirstCmd);

	return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, ((uidFirstCmd - cmdFirst) + 1));
}

STDMETHODIMP ShellExt::GetCommandString(
	UINT_PTR idCmd, UINT uFlags, UINT* pwReserved, LPSTR pszName, UINT cchMax)
{
	//USES_CONVERSION;

	// Check idCmd, it must be 0 since we have only one menu item.
	//if (0 != idCmd)
	//	return E_INVALIDARG;

	// If Explorer is asking for a help string, copy our string into the
	// supplied buffer.
	if (uFlags == GCS_HELPTEXT)
	{
		LPCTSTR szText = L"This is the simple shell extension's help";

		if (uFlags & GCS_UNICODE)
		{
			lstrcpynW((LPWSTR)pszName, L"This is the simple shell extension's help", cchMax);
		}
		else
		{
			// Use the ANSI string copy API to return the help string.
			lstrcpynA(pszName, "This is the simple shell extension's help", cchMax);
		}

		return S_OK;
	}

	

	if (uFlags == GCS_VERB)
	{
		std::wstring verb;
		for (auto& it : contextMenu) {
			verb = it.GetVerb(idCmd + cmdFirst);
			if (verb.empty())
				continue;
		}
			
		if (verb.empty())
			return E_INVALIDARG;
		

		LPCTSTR szText = L"This is the simple shell extension's help";

		if (uFlags & GCS_UNICODE)
		{
			lstrcpynW((LPWSTR)pszName, verb.data(), cchMax);
		}
		else
		{
			__debugbreak(); // #TODO convert
			// Use the ANSI string copy API to return the help string.
			lstrcpynA(pszName, "This is the simple shell extension's help", cchMax);
		}

		return S_OK;
	}



	return E_INVALIDARG;
}

STDMETHODIMP ShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO pCmdInfo)
{
	LPCMINVOKECOMMANDINFOEX test = (LPCMINVOKECOMMANDINFOEX)pCmdInfo;

	// If lpVerb really points to a string, ignore this function call and bail out.
	if (0 != HIWORD(pCmdInfo->lpVerb)) {

		// its a string


		return E_INVALIDARG;
	}
		
	for (auto& it : contextMenu) {
	
		auto res = it.InvokeByCmd(LOWORD(pCmdInfo->lpVerb) + cmdFirst, selectedFiles);
		if (SUCCEEDED(res))
			return res;
	}

	return E_INVALIDARG; // when we get here, all context menus failed to resolve
}


BOOL OnInitDialog(HWND hwnd, LPARAM lParam);
BOOL OnApply(HWND hwnd, PSHNOTIFY* phdr);


BOOL OnInitDialog(HWND hwnd, LPARAM lParam)
{
	PROPSHEETPAGE* ppsp = (PROPSHEETPAGE*)lParam;
	ShellExt* shellEx = (ShellExt*)ppsp->lParam;


	// Store the filename in this window's user data area, for later use.
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)shellEx);

	auto& files = shellEx->GetSelectedFiles();

	//#TODO support multiple files? would need multiple pages
	auto pboFile = files.front();

	auto file = GPboFileDirectory.GetPboFile(pboFile);
	if (!file) return TRUE;

	std::wstring buffer;
	buffer.reserve(1024);

	for (auto& it : file->properties) {
		buffer.append(Util::utf8_decode(std::format("{}={}\r\n", it.first, it.second)));
	}
	buffer.pop_back();
	buffer.pop_back();

	SetDlgItemText(hwnd, IDC_EDIT1, buffer.data());
	// SendDlgItemMessage(hwnd, IDC_EDIT1, EM_SETSEL, 0, 5); //#TODO figure out why this doesn't work?

	return TRUE;
}

BOOL OnApply(HWND hwnd, PSHNOTIFY* phdr)
{
	std::wstring buffer;
	buffer.resize(2048);

	GetDlgItemText(hwnd, IDC_EDIT1, buffer.data(), 2048);

	// parse out properties

	std::vector<std::pair<std::string, std::string>> newProperties;

	uint32_t curOffs = 0;

	while (buffer[curOffs]) {

		auto found = buffer.find(L"\r\n", curOffs);
		if (found == std::string::npos)
			found = buffer.find(L'\0', curOffs);

		if (found == std::string::npos)
			return false; // failed to parse properly

		std::wstring_view line(buffer.data() + curOffs, buffer.data() + found);
		curOffs = found + 2; // next line

		auto sep = line.find(L'=');
		if (sep == std::string::npos)
			return false; // failed to parse properly

		auto propName = line.substr(0, sep);
		auto propValue = line.substr(sep + 1);

		newProperties.emplace_back(Util::utf8_encode(propName), Util::utf8_encode(propValue));

	}

	// patch	

	//#TODO this is not viable, make a map to associate hwnd to proper Ref to ShellExt, can clean it up in PropPageCallbackProc
	ShellExt* shellEx = (ShellExt*)GetWindowLong(hwnd, GWLP_USERDATA);


	auto file = GPboFileDirectory.GetPboFile(shellEx->GetSelectedFiles().front());
	if (!file) return FALSE;

	if (newProperties == file->properties)
		return FALSE; //nothing to update

	{
		PboPatcherLocked patcher(file);

		// find deletes
		for (auto& it : file->properties) {
			if (std::ranges::none_of(newProperties, [&it](const std::pair<std::string, std::string>& entry) {
				return entry.first == it.first;
				})) {
				// new properties doesn't contain this key	
				patcher.AddPatch<PatchDeleteProperty>(it.first);
			}
		}

		// adds/replaces, currently doesn't matter
		for (auto& it : newProperties) {
			patcher.AddPatch<PatchUpdateProperty>(it);
		}
	}





	return TRUE;
}

INT_PTR CALLBACK PropPageDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL bRet = FALSE;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		bRet = OnInitDialog(hwnd, lParam);
		break;
	case WM_NOTIFY:
	{
		NMHDR* phdr = (NMHDR*)lParam;

		switch (phdr->code)
		{
		case PSN_APPLY:
			bRet = OnApply(hwnd, (PSHNOTIFY*)phdr);
			break;
		case DTN_DATETIMECHANGE:
			// If the user changes any of the DTP controls, enable
			// the Apply button.
			SendMessage(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
			break;
		}  // end switch
	}  // end case WM_NOTIFY
	break;
	}  // end switch

	return bRet;
}

UINT CALLBACK PropPageCallbackProc(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE ppsp)
{
	if (PSPCB_RELEASE == uMsg)
		((ShellExt*)ppsp->lParam)->Release();

	return 1;
}

extern HINSTANCE   g_hInst;

HRESULT ShellExt::AddPages(LPFNADDPROPSHEETPAGE lpfnAddPageProc, LPARAM lParam) {

	PROPSHEETPAGE  psp;
	HPROPSHEETPAGE hPage;
	TCHAR          szPageTitle[MAX_PATH];


	// 'it' points at the next filename. Allocate a new copy of the string
	// that the page will own.
	//LPCTSTR szFile = _tcsdup(it->c_str());
	psp.dwSize = sizeof(PROPSHEETPAGE);
	psp.dwFlags = PSP_USETITLE |
		PSP_USECALLBACK; // PSP_USEICONID | PSP_USEREFPARENT | 
	psp.hInstance = g_hInst;
	psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_LARGE);
	psp.pszIcon = nullptr; // MAKEINTRESOURCE(IDI_TAB_ICON);
	psp.pszTitle = L"PboExplorer";
	psp.pfnDlgProc = PropPageDlgProc;
	psp.lParam = (LPARAM)this;
	AddRef(); //PropPageCallbackProc
	psp.pfnCallback = PropPageCallbackProc;
	psp.pcRefParent = nullptr; // (UINT*)&_Module.m_nLockCnt;
	hPage = CreatePropertySheetPage(&psp);
	if (NULL != hPage)
	{
		// Call the shell's callback function, so it adds the page to
		// the property sheet.
		if (!lpfnAddPageProc(hPage, lParam))
			DestroyPropertySheetPage(hPage);
	}

	return S_OK;
}



std::vector<ContextMenuItem> ShellExt::QueryContextMenuFromCache()
{
	bool isFolders = std::ranges::all_of(selectedFiles, [](const std::filesystem::path path) -> bool {
		return std::filesystem::is_directory(path);
	});

	bool isSingleElement = selectedFiles.size() == 1;


	if (isFolders)
		if (isSingleElement)
			return GCache.GetFromCache("SHEX_FldrS", CreateContextMenu_SingleFolder);
		else
			return GCache.GetFromCache("SHEX_FldrM", CreateContextMenu_MultiFolder);

	bool isPBOs = std::ranges::all_of(selectedFiles, [](const std::filesystem::path path) -> bool {
		return path.extension() == ".pbo";
	});

	if (isPBOs)
		if (isSingleElement)
			return GCache.GetFromCache("SHEX_PboS", CreateContextMenu_SinglePbo);
		else
			return GCache.GetFromCache("SHEX_PboM", CreateContextMenu_MultiPbo);

	//#TODO derapify .bin (if file has correct header, need to open and read)

	return {};
}

std::filesystem::path ReadRegistryFilePathKey(HKEY hkey, std::wstring path, std::wstring key) {

	HKEY hKey;
	auto lRes = RegOpenKeyExW(hkey, path.data(), 0, KEY_READ, &hKey);
	if (!SUCCEEDED(lRes))
		return {};

	WCHAR szBuffer[MAX_PATH];
	DWORD dwBufferSize = sizeof(szBuffer);
	DWORD type = 0;
	lRes = RegQueryValueExW(hKey, key.data(), 0, &type, (LPBYTE)szBuffer, &dwBufferSize);
	if (!SUCCEEDED(lRes))
		return {};

	if (type == REG_SZ && std::filesystem::exists(szBuffer))
		return szBuffer;
	if (type == REG_EXPAND_SZ) {
		WCHAR szBufferEx[512];
		ExpandEnvironmentStringsW(szBuffer, szBufferEx, sizeof(szBufferEx));
		if (std::filesystem::exists(szBufferEx))
			return szBufferEx;
	}

	return {};
}

std::filesystem::path GetA3ToolsPath() {
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

std::filesystem::path GetMikeroToolsPath() {
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

#pragma region ContextMenuTools

HRESULT MikeroExtractPboToSubfolders(const std::vector<std::filesystem::path>& files) {
	auto MikeroToolsPath = GCache.GetFromCache("MikeroToolsPath", GetMikeroToolsPath);
	if (MikeroToolsPath.empty() || !std::filesystem::exists(MikeroToolsPath / "bin" / "ExtractPbo.exe"))
		return E_FAIL;

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
	auto path = (MikeroToolsPath / "bin" / "ExtractPbo.exe").native();
	sei.lpFile = path.c_str();
	//sei.hwnd = hwnd; //#TODO get HWND
	sei.nShow = SW_SHOWNORMAL;

	bool allGood = true;

	for (auto& it : files) {
		auto params = std::format(files.size() > 1 ? L"-P \"{}\"": L"\"{}\"", it.native()); // silent mode if unpacking several
		sei.lpParameters = params.c_str();
		auto workDir = it.parent_path().native();
		sei.lpDirectory = workDir.c_str();
		allGood &= ShellExecuteEx(&sei);
	}

	return allGood ? S_OK : E_UNEXPECTED;
}

HRESULT BankRevExtractPboToSubfolders(const std::vector<std::filesystem::path>& files) {
	auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
	if (a3ToolsPath.empty() || !std::filesystem::exists(a3ToolsPath / "BankRev" / "BankRev.exe"))
		return E_FAIL;

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
	auto path = (a3ToolsPath / "BankRev" / "BankRev.exe").native();
	sei.lpFile = path.c_str();
	//sei.hwnd = hwnd; //#TODO get HWND
	sei.nShow = SW_SHOWNORMAL;

	bool allGood = true;

	for (auto& it : files) {
		auto params = std::format(L"-t \"{}\"", it.native());
		sei.lpParameters = params.c_str();
		auto workDir = it.parent_path().native();
		sei.lpDirectory = workDir.c_str();

		allGood &= ShellExecuteEx(&sei);
	}

	return allGood ? S_OK : E_UNEXPECTED;
}

HRESULT SignPbos(const std::vector<std::filesystem::path>& files) {
	auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
	if (a3ToolsPath.empty() || !std::filesystem::exists(a3ToolsPath / "DSSignFile" / "DSSignFile.exe"))
		return E_FAIL;

	/*
	Usage: DSSignFile private_key file_to_sign [-v2]
		>> private_key - path to the bikeyprivate file used to sign the file
		>> file_to_sign - path to the file we want to sign
		>> -v2 - OPTIONAL - when used, the file is signed with a v2 signature (v3 by default)
	*/

	OPENFILENAMEW info {};

	info.lStructSize = sizeof(OPENFILENAMEW);
	//info.hwndOwner =  //#TODO
	info.lpstrFilter = L"BI Private Key\0*.biprivatekey\0\0";
	info.nFilterIndex = 1;
	info.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
	info.lpstrTitle = L"PBOExplorer: Please select the .biprivatekey to sign the PBO's with";

	auto workDir = files.front().parent_path().native();
	info.lpstrInitialDir = workDir.c_str();
	wchar_t pathBuffer[MAX_PATH] {};

	info.lpstrFile = pathBuffer;
	info.nMaxFile = MAX_PATH;

	if (!GetOpenFileNameW(&info))
		return E_FAIL;


	SHELLEXECUTEINFO sei;
	ZeroMemory(&sei, sizeof(sei));
	sei.cbSize = (DWORD)sizeof(sei);
	sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
	auto path = (a3ToolsPath / "DSSignFile" / "DSSignFile.exe").native();
	sei.lpFile = path.c_str();
	//sei.hwnd = hwnd; //#TODO get HWND
	sei.nShow = SW_SHOWNORMAL;

	bool allGood = true;

	for (auto& it : files) {
		auto params = std::format(L"\"{}\" \"{}\"", pathBuffer, it.native());
		sei.lpParameters = params.c_str();
		auto workDir = it.parent_path().native();
		sei.lpDirectory = workDir.c_str();

		allGood &= ShellExecuteEx(&sei);
	}

	return allGood ? S_OK : E_UNEXPECTED;
}

#pragma endregion ContextMenuTools





std::vector<ContextMenuItem> ShellExt::CreateContextMenu_SingleFolder()
{

	// Pack with makePbo
	// Pack with pboProject
	// Pack with fileBank
	// Pack with PboExplorer
	// cpbo? how find?

	ContextMenuItem rootItem{ L"Pack PBO with..", L"packWithAny", [](const std::vector<std::filesystem::path>& files) {
		//#TODO select first available child
		return E_NOTIMPL;
	} };

	auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
	if (!a3ToolsPath.empty()) {

		if (std::filesystem::exists(a3ToolsPath / "FileBank" / "FileBank.exe"))
			rootItem.AddChild({ L"..FileBank", L"packWithFileBank", [a3ToolsPath](const std::vector<std::filesystem::path>& files) {
				//#TODO manually parse pboprefix
				/*
				 FileBank {options} source [source]
				   source may be directory name or log file name (.log)

				   Options:
				   -property name=value   Store a named property
				   -exclude filename      List of patterns which should not be included in the pbo file
				   -dst path              path to folder to store PBO
				*/


				SHELLEXECUTEINFO sei;
				ZeroMemory(&sei, sizeof(sei));
				sei.cbSize = (DWORD)sizeof(sei);
				sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
				auto path = (a3ToolsPath / "FileBank" / "FileBank.exe").native();
				sei.lpFile = path.c_str();
				auto params = std::format(L"dst \"{}\" \"{}\"", files.front().parent_path().native(), files.front().native());
				sei.lpParameters = params.c_str();
				auto workDir = files.front().parent_path().native();
				sei.lpDirectory = workDir.c_str();
				//sei.hwnd = hwnd; //#TODO get HWND
				sei.nShow = SW_SHOWNORMAL;

				return ShellExecuteEx(&sei) ? S_OK : E_UNEXPECTED;
			} });
	
		if (std::filesystem::exists(a3ToolsPath / "AddonBuilder" / "AddonBuilder.exe"))
			rootItem.AddChild({ L"..AddonBuilder", L"packWithAddonBuilder", [a3ToolsPath](const std::vector<std::filesystem::path>& files) {
				// cheaty trick to open UI with our target path, set the stored last used source folder path, and then just open UI and it will use that

				// find addon builder config

				std::filesystem::path addonBuilderConfig;
				wchar_t appPath[MAX_PATH];
				SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appPath);
				std::error_code ec;
				for (auto& p : std::filesystem::recursive_directory_iterator(std::filesystem::path(appPath) / "Bohemia_Interactive", ec)) {
					if (!p.is_regular_file(ec))
						continue;

					if (!p.path().filename().native().starts_with(L"user.config"))
						continue;

					auto test = p.path().lexically_relative(std::filesystem::path(appPath) / "Bohemia_Interactive").native();
					if (!p.path().lexically_relative(std::filesystem::path(appPath) / "Bohemia_Interactive").native().starts_with(L"AddonBuilder.exe"))
						continue;

					addonBuilderConfig = p.path();
					break;
				}

				if (!addonBuilderConfig.empty()) {
					//#TODO wstring support, utf8
					std::ifstream t(addonBuilderConfig);
					std::string str((std::istreambuf_iterator<char>(t)),
									std::istreambuf_iterator<char>());

					auto srcDirOffs = str.find("<setting name=\"SourceDir\" serializeAs=\"String\">");

					if (srcDirOffs != std::string::npos) {
						auto srcDirValOffs = str.find("<value>", srcDirOffs);
						auto srcDirValEndOffs = str.find("</value>", srcDirValOffs);
						if (srcDirValOffs != std::string::npos && srcDirValEndOffs != std::string::npos)
							str.replace(srcDirValOffs + 7, srcDirValEndOffs - (srcDirValOffs + 7), files.front().string());
					}

					auto dstDirOffs = str.find("<setting name=\"DestDir\" serializeAs=\"String\">");

					if (dstDirOffs != std::string::npos) {
						auto dstDirValOffs = str.find("<value>", dstDirOffs);
						auto dstDirValEndOffs = str.find("</value>", dstDirValOffs);
						if (dstDirValOffs != std::string::npos && dstDirValEndOffs != std::string::npos) 
							str.replace(dstDirValOffs + 7, dstDirValEndOffs - (dstDirValOffs + 7), (files.front().parent_path() / (files.front().filename().string() + ".pbo")).string());
					}

					std::ofstream to(addonBuilderConfig);
					to.write(str.data(), str.length());

				}
				


				SHELLEXECUTEINFO sei;
				ZeroMemory(&sei, sizeof(sei));
				sei.cbSize = (DWORD)sizeof(sei);
				sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
				auto path = (a3ToolsPath / "AddonBuilder" / "AddonBuilder.exe").native();
				sei.lpFile = path.c_str();
				sei.lpParameters = nullptr;
				auto workDir = files.front().parent_path().native();
				sei.lpDirectory = workDir.c_str();
				//sei.hwnd = hwnd; //#TODO get HWND
				sei.nShow = SW_SHOWNORMAL;

				return ShellExecuteEx(&sei) ? S_OK : E_UNEXPECTED;
			} });

	}

	auto MikeroToolsPath = GCache.GetFromCache("MikeroToolsPath", GetMikeroToolsPath);
	if (!MikeroToolsPath.empty()) {
		if (std::filesystem::exists(MikeroToolsPath / "bin" / "MakePbo.exe"))
			rootItem.AddChild({ L"..MakePbo", L"packWithMakePbo", [MikeroToolsPath](const std::vector<std::filesystem::path>& files) {
				/*
				MakePbo Version 2.04, Dll 7.46 "no_file"
				------args---------
				Syntax: MakePbo [-option(s)] pbofolder [FolderAndOrFile[.pbo|.ebo|.xbo|.ifa]]
				options:-! obfuscate
						-@=Prefix
						-$ Enable no Prefix
						-U Allow unbinarised p3d's
						-P Don't pause
						-D Don't produce datestamps
						-Z=Compression (see documentation)
						-A Arma (default)
						-C CWC
						-E Elite
						-R Resistance
						-V Vbs (see documentation)
						-N wyswig: (almost no error checking) OR
								-B Rapify (default)
								-M Do not Rapify mission.sqm
								-F rebuild requiredAddons
								-S show rapified output
								-G check for missing files
								-W warnings are errors
								-X=exclude files (see documentation)
				*/


				SHELLEXECUTEINFO sei;
				ZeroMemory(&sei, sizeof(sei));
				sei.cbSize = (DWORD)sizeof(sei);
				sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
				auto path = (MikeroToolsPath / "bin" / "MakePbo.exe").native();
				sei.lpFile = path.c_str();
				//#TODO can specify multiple files here and reuse exact same code for multipbo
				auto params = std::format(L"\"{}\"", files.front().native());
				sei.lpParameters = params.c_str();
				auto workDir = files.front().parent_path().native();
				sei.lpDirectory = workDir.c_str();
				//sei.hwnd = hwnd; //#TODO get HWND
				sei.nShow = SW_SHOWNORMAL;

				return ShellExecuteEx(&sei) ? S_OK : E_UNEXPECTED;
			} });

		if (std::filesystem::exists(MikeroToolsPath / "bin" / "pboProject.exe"))
			rootItem.AddChild({ L"..PboProject", L"packWithPboProject", [MikeroToolsPath](const std::vector<std::filesystem::path>& files) {
				/*
				syntax:
					+|-? or -syntax, show syntax and options
					+|-A Remove appid
					+|-A=123 set appid
					+|-B=Do/Don't binarise sqm or cpp
					+|-C Clear/Don't clear temp\project folder
					+|-E=whatever. Engine is arrowhead arma3 or dayz
					+|-F Deprecated: was (don't)rebuild required addons
					+|-G=Do/Don't convert wav/wss to ogg.
					+|-H=Do/Don't convert png to paa. (Dayz only)
					+|-I=somefolder see wrp config documentation
					+|-K enable/disable signing
					+K=whatever  use this key
					+|-P do not pause
					+|-M=SomeFolder mod output
					+|-N Noisy to log on/off
					+|-!
					+|-O Obfuscate (same)
					+|-R Restore original settings after this session
					+|-S (don't)stop processing after any error
					+|-W=warnings are errors on/off
					+|-Z compress on/off

					Command line params are bad. See documentation for syntax
				*/

				// cheaty trick to open UI with our target path, set the stored last used source folder path, and then just open UI and it will use that

				HKEY hKey;
				DWORD dwDisp;

				auto lResult = RegCreateKeyEx(HKEY_CURRENT_USER,
					L"SOFTWARE\\Mikero\\pboProject\\Settings",
					0,
					nullptr,
					REG_OPTION_NON_VOLATILE,
					KEY_WRITE,
					nullptr,
					&hKey,
					&dwDisp);

				if (lResult == NOERROR)
				{
					auto folderPath = files.front().native();
					lResult = RegSetValueExW(hKey,
						L"fs_source_folder",
						0,
						REG_SZ,
						(LPBYTE)folderPath.c_str(),
						(DWORD)folderPath.length() * 2 + 2);

					RegCloseKey(hKey);
				}

				SHELLEXECUTEINFO sei;
				ZeroMemory(&sei, sizeof(sei));
				sei.cbSize = (DWORD)sizeof(sei);
				sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_INVOKEIDLIST;
				auto path = (MikeroToolsPath / "bin" / "pboProject.exe").native();
				sei.lpFile = path.c_str();
				sei.lpParameters = nullptr;
				auto workDir = files.front().parent_path().native();
				sei.lpDirectory = workDir.c_str();
				//sei.hwnd = hwnd; //#TODO get HWND
				sei.nShow = SW_SHOWNORMAL;

				return ShellExecuteEx(&sei) ? S_OK : E_UNEXPECTED;
			} });
	}


	rootItem.AddChild({ L"..PboExplorer", L"packWithNative", [MikeroToolsPath](const std::vector<std::filesystem::path>& files) {
		return E_NOTIMPL;
	} });

	if (rootItem.HasChildren())	// only if we have actual unpack options //#TODO remove when adding native unpack
		return { rootItem };
	return {};
}

std::vector<ContextMenuItem> ShellExt::CreateContextMenu_MultiFolder()
{
	return {};
}

std::vector<ContextMenuItem> ShellExt::CreateContextMenu_SinglePbo()
{
	//#TODO sign with bisign, need multiple toplevel items

	ContextMenuItem rootItem{ L"Unpack with..", L"unpackWithAny", [](const std::vector<std::filesystem::path>& files) { 
		// prefer mikero if available

		auto MikeroToolsPath = GCache.GetFromCache("MikeroToolsPath", GetMikeroToolsPath);
		if (!MikeroToolsPath.empty() && std::filesystem::exists(MikeroToolsPath / "bin" / "ExtractPbo.exe")) {
			return MikeroExtractPboToSubfolders(files);
		}
		else 
		{
			auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
			if (!a3ToolsPath.empty() && std::filesystem::exists(a3ToolsPath / "BankRev" / "BankRev.exe")) {
				return BankRevExtractPboToSubfolders(files);
			}
		}

		return E_FAIL;
	}};

	auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
	if (!a3ToolsPath.empty()) {

		if (std::filesystem::exists(a3ToolsPath / "BankRev" / "BankRev.exe"))
			rootItem.AddChild({ L"..BankRev", L"unpackWithBankRev", [a3ToolsPath](const std::vector<std::filesystem::path>& files) {


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
				auto path = (a3ToolsPath / "BankRev" / "BankRev.exe").native();
				sei.lpFile = path.c_str();
				//#TODO can specify multiple files here and reuse exact same code for multipbo
				auto params = std::format(L"-t \"{}\"", files.front().native());
				sei.lpParameters = params.c_str();
				auto workDir = files.front().parent_path().native();
				sei.lpDirectory = workDir.c_str();
				//sei.hwnd = hwnd; //#TODO get HWND
				sei.nShow = SW_SHOWNORMAL;

				return ShellExecuteEx(&sei) ? S_OK : E_UNEXPECTED;
			} });
	}

	auto MikeroToolsPath = GCache.GetFromCache("MikeroToolsPath", GetMikeroToolsPath);
	if (!MikeroToolsPath.empty()) {
		if (std::filesystem::exists(MikeroToolsPath / "bin" / "ExtractPbo.exe"))
			rootItem.AddChild({ L"..ExtractPbo", L"unpackWithExtractPbo", MikeroExtractPboToSubfolders });
	}

	std::vector<ContextMenuItem> result;

	if (rootItem.HasChildren())	// only if we have actual unpack options //#TODO remove when adding native unpack
		result.emplace_back(std::move(rootItem));

	if (!a3ToolsPath.empty() && std::filesystem::exists(a3ToolsPath / "DSSignFile" / "DSSignFile.exe")) {
		result.emplace_back(L"Sign PBOs", L"signPbo", SignPbos);
	}

	return result;
}

std::vector<ContextMenuItem> ShellExt::CreateContextMenu_MultiPbo()
{
	//#TODO sign with bisign, need multiple toplevel items


	auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
	auto MikeroToolsPath = GCache.GetFromCache("MikeroToolsPath", GetMikeroToolsPath);

#pragma region UnpackerDefinitions
	auto unpackBankRevHere = [a3ToolsPath](const std::vector<std::filesystem::path>& files) {

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
		auto path = (a3ToolsPath / "BankRev" / "BankRev.exe").native();
		sei.lpFile = path.c_str();
		//sei.hwnd = hwnd; //#TODO get HWND
		sei.nShow = SW_SHOWNORMAL;

		bool allGood = true;

		for (auto& it : files) {
			auto params = std::format(L"-t \"{}\" -f .", it.native()); // -f . == current directory (workdir is parent)
			sei.lpParameters = params.c_str();
			auto workDir = it.parent_path().native();
			sei.lpDirectory = workDir.c_str();

			allGood &= ShellExecuteEx(&sei);
		}

		return allGood ? S_OK : E_UNEXPECTED;
	};

	auto unpackBankRevSubfolder = BankRevExtractPboToSubfolders;

	auto unpackExtractPboHere = [MikeroToolsPath](const std::vector<std::filesystem::path>& files) {
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
		auto path = (MikeroToolsPath / "bin" / "ExtractPbo.exe").native();
		sei.lpFile = path.c_str();
		//sei.hwnd = hwnd; //#TODO get HWND
		sei.nShow = SW_SHOWNORMAL;

		bool allGood = true;

		for (auto& it : files) {
			auto params = std::format(L"-P \"{}\" \"{}\"", it.native(), it.parent_path().native());
			sei.lpParameters = params.c_str();
			auto workDir = it.parent_path().native();
			sei.lpDirectory = workDir.c_str();

			allGood &= ShellExecuteEx(&sei);
		}

		return allGood ? S_OK : E_UNEXPECTED;


		return ShellExecuteEx(&sei) ? S_OK : E_UNEXPECTED;
	};

	auto unpackExtractPboSubfolder = MikeroExtractPboToSubfolders;

#pragma endregion UnpackerDefinitions

	auto unpackSubfolderWithAny = [unpackExtractPboSubfolder, unpackBankRevSubfolder](const std::vector<std::filesystem::path>& files) {

		// prefer mikero if available

		auto MikeroToolsPath = GCache.GetFromCache("MikeroToolsPath", GetMikeroToolsPath);
		if (!MikeroToolsPath.empty() && std::filesystem::exists(MikeroToolsPath / "bin" / "ExtractPbo.exe")) {
			return unpackExtractPboSubfolder(files);
		}
		else 
		{
			auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
			if (!a3ToolsPath.empty() && std::filesystem::exists(a3ToolsPath / "BankRev" / "BankRev.exe")) {
				return unpackBankRevSubfolder(files);
			}
		}

		return E_FAIL;
	};

	auto unpackHereWithAny = [unpackExtractPboHere, unpackBankRevHere](const std::vector<std::filesystem::path>& files) {
		// prefer mikero if available

		auto MikeroToolsPath = GCache.GetFromCache("MikeroToolsPath", GetMikeroToolsPath);
		if (!MikeroToolsPath.empty() && std::filesystem::exists(MikeroToolsPath / "bin" / "ExtractPbo.exe")) {
			return unpackExtractPboHere(files);
		}
		else
		{
			auto a3ToolsPath = GCache.GetFromCache("A3ToolsPath", GetA3ToolsPath);
			if (!a3ToolsPath.empty() && std::filesystem::exists(a3ToolsPath / "BankRev" / "BankRev.exe")) {
				return unpackBankRevHere(files);
			}
		}

		return E_FAIL;
	};



	ContextMenuItem rootItem{ L"Unpack..", L"unpackAny", unpackSubfolderWithAny};


	//  Unpack...
	//  	Unpack Here with
	//  		Bankrev...
	//  	Unpack to *\ with
	//  		Bankrev...

	ContextMenuItem unpackHereRoot{ L"Here with..", L"unpackHereWithAny", unpackHereWithAny };
	ContextMenuItem unpackSubfolderRoot{ L"to *\\ with..", L"unpackSubfolderWithAny", unpackSubfolderWithAny };



	if (!a3ToolsPath.empty() && std::filesystem::exists(a3ToolsPath / "BankRev" / "BankRev.exe")) {
		unpackHereRoot.AddChild({ L"..BankRev", L"unpackHereWithBankRev",  unpackBankRevHere });
		unpackSubfolderRoot.AddChild({ L"..BankRev", L"unpackSubfolderWithBankRev", unpackBankRevSubfolder });
	}

	if (!MikeroToolsPath.empty() && std::filesystem::exists(MikeroToolsPath / "bin" / "ExtractPbo.exe")) {
		unpackHereRoot.AddChild({ L"..ExtractPbo", L"unpackHereWithExtractPbo",  unpackExtractPboHere });
		unpackSubfolderRoot.AddChild({ L"..ExtractPbo", L"unpackSubfolderWithExtractPbo",  unpackExtractPboSubfolder });
	}

	if (unpackHereRoot.HasChildren())
		rootItem.AddChild(unpackHereRoot);
	if (unpackSubfolderRoot.HasChildren())
		rootItem.AddChild(unpackSubfolderRoot);

	std::vector<ContextMenuItem> result;

	if (rootItem.HasChildren())	// only if we have actual unpack options //#TODO remove when adding native unpack
		result.emplace_back(std::move(rootItem));

	if (!a3ToolsPath.empty() && std::filesystem::exists(a3ToolsPath / "DSSignFile" / "DSSignFile.exe")) {
		result.emplace_back(L"Sign PBOs", L"signPbo", SignPbos);
	}

	return result;
}
