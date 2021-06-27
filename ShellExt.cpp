#include "ShellExt.hpp"

#include "resource.h"
#include "DebugLogger.hpp"

// CreatePropertySheetPageW
#pragma comment( lib, "Comctl32" )

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
	else
		__debugbreak();
	


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

	// Get the name of the first file and store it in our member variable m_szFile.
	if (0 == DragQueryFile(hDrop, 0, m_szFile, MAX_PATH))
		hr = E_INVALIDARG;

	GlobalUnlock(stg.hGlobal);
	ReleaseStgMedium(&stg);

	return hr;
}

STDMETHODIMP ShellExt::QueryContextMenu(
	HMENU hmenu, UINT uMenuIndex, UINT uidFirstCmd,
	UINT uidLastCmd, UINT uFlags)
{
	// If the flags include CMF_DEFAULTONLY then we shouldn't do anything.
	if (uFlags & CMF_DEFAULTONLY)
		return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);

	InsertMenu(hmenu, uMenuIndex, MF_BYPOSITION, uidFirstCmd, L"PboExplorer Test Item");

	return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 1);
}

BOOL OnInitDialog(HWND hwnd, LPARAM lParam);
BOOL OnApply(HWND hwnd, PSHNOTIFY* phdr);

BOOL OnInitDialog(HWND hwnd, LPARAM lParam)
{
	PROPSHEETPAGE* ppsp = (PROPSHEETPAGE*)lParam;
	LPCTSTR         szFile = (LPCTSTR)ppsp->lParam;
	HANDLE          hFind;
	WIN32_FIND_DATA rFind;

	// Store the filename in this window's user data area, for later use.
	//SetWindowLong(hwnd, GWL_USERDATA, (LONG)szFile);

	//hFind = FindFirstFile(szFile, &rFind);

	//if (INVALID_HANDLE_VALUE != hFind)
	//{
	//	// Initialize the DTP controls.
	//	SetDTPCtrl(hwnd, IDC_MODIFIED_DATE, IDC_MODIFIED_TIME,
	//		&rFind.ftLastWriteTime);
	//
	//	SetDTPCtrl(hwnd, IDC_ACCESSED_DATE, 0,
	//		&rFind.ftLastAccessTime);
	//
	//	SetDTPCtrl(hwnd, IDC_CREATED_DATE, IDC_CREATED_TIME,
	//		&rFind.ftCreationTime);
	//
	//	FindClose(hFind);
	//}
	//PathSetDlgItemPath(hwnd, IDC_FILENAME, szFile);
	return FALSE;
}

BOOL OnApply(HWND hwnd, PSHNOTIFY* phdr)
{
	//LPCTSTR  szFile = (LPCTSTR)GetWindowLong(hwnd, GWL_USERDATA);
	//HANDLE   hFile;
	//FILETIME ftModified, ftAccessed, ftCreated;

	// Open the file.
	//hFile = CreateFile(szFile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
	//	OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	//if (INVALID_HANDLE_VALUE != hFile)
	//{
	//	// Retrieve the dates/times from the DTP controls.
	//	ReadDTPCtrl(hwnd, IDC_MODIFIED_DATE, IDC_MODIFIED_TIME, &ftModified);
	//	ReadDTPCtrl(hwnd, IDC_ACCESSED_DATE, 0, &ftAccessed);
	//	ReadDTPCtrl(hwnd, IDC_CREATED_DATE, IDC_CREATED_TIME, &ftCreated);
	//
	//	// Change the file's created, accessed, and last modified times.
	//	SetFileTime(hFile, &ftCreated, &ftAccessed, &ftModified);
	//	CloseHandle(hFile);
	//}
	//else
	//	// <<Error handling omitted>>
	//
	//  // Return PSNRET_NOERROR to allow the sheet to close if the user clicked OK.
	//	SetWindowLong(hwnd, DWL_MSGRESULT, PSNRET_NOERROR);
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
	//if (PSPCB_RELEASE == uMsg)
	//	free((void*)ppsp->lParam);

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
		psp.dwFlags =PSP_USETITLE |
			PSP_USECALLBACK; // PSP_USEICONID | PSP_USEREFPARENT | 
		psp.hInstance = g_hInst;
		psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_LARGE);
		psp.pszIcon = nullptr; // MAKEINTRESOURCE(IDI_TAB_ICON);
		psp.pszTitle = L"PboExplorer";
		psp.pfnDlgProc = PropPageDlgProc;
		//psp.lParam = (LPARAM)szFile;
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

STDMETHODIMP ShellExt::GetCommandString(
	UINT_PTR idCmd, UINT uFlags, UINT* pwReserved, LPSTR pszName, UINT cchMax)
{
	//USES_CONVERSION;

	// Check idCmd, it must be 0 since we have only one menu item.
	if (0 != idCmd)
		return E_INVALIDARG;




	// If Explorer is asking for a help string, copy our string into the
	// supplied buffer.
	if (uFlags & GCS_HELPTEXT)
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

	return E_INVALIDARG;
}

STDMETHODIMP ShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO pCmdInfo)
{
	// If lpVerb really points to a string, ignore this function call and bail out.
	if (0 != HIWORD(pCmdInfo->lpVerb))
		return E_INVALIDARG;

	// Get the command index - the only valid one is 0.
	switch (LOWORD(pCmdInfo->lpVerb))
	{
	case 0:
	{
		TCHAR szMsg[MAX_PATH + 32];

		wsprintf(szMsg, L"The selected file was:\n\n%s", m_szFile);

		MessageBox(pCmdInfo->hwnd, szMsg, L"PboExplorer",
			MB_ICONINFORMATION);

		return S_OK;
	}
	break;

	default:
		return E_INVALIDARG;
		break;
	}
}