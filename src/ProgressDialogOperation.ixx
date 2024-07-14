module;

#include <Unknwnbase.h>
#include <ShlObj.h>
#include <Windows.h>

export module ProgressDialogOperation;

import std;
import std.compat;
import ComRef;
import Util;



struct IProgressDialog;

export class ProgressDialogOperation : public RefCounted<ProgressDialogOperation>
{
	std::wstring lines[2];
	IProgressDialog* ppd = nullptr;
	HWND windowHWND;
public:
	ProgressDialogOperation(std::wstring textLine1, std::wstring textLine2, HWND windowHWND);

	void DoOperation(std::function<void(const ProgressDialogOperation&)>);


	void ResetTimer() const;
	void SetLineText(uint8_t line, std::wstring newText) const;
	void SetTitle(std::wstring newText) const;
	void SetProgress(uint64_t current, uint64_t max) const;

};





ProgressDialogOperation::ProgressDialogOperation(std::wstring textLine1, std::wstring textLine2, HWND windowHWND) : windowHWND(windowHWND)
{
    lines[0] = textLine1;
    lines[1] = textLine2;
}

void ProgressDialogOperation::DoOperation(std::function<void(const ProgressDialogOperation&)> func)
{
    if (ppd) {
        Util::TryDebugBreak();
        return;
    }

    this->AddRef();
    std::thread([this, func]() {

        HRESULT hrOleInit = []() { // #TODO cleanup, make it RTTI
            HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            if (FAILED(hr))
            {
                hr = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
            }
            return hr;
            }();

            //#TODO use https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ioperationsprogressdialog-startprogressdialog
            //thats the new dialog?

            {
                if (SUCCEEDED(CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER, IID_IProgressDialog, (void**)&ppd)))
                {
                    /*
                      `IOperationsProgressDialog';
                      `IProgressDialog';
                      `IIOCancelInformation';
                      `IOleWindow';
                      `IActionProgressDialog';
                      `IActionProgress';
                      `CObjectWithSite';
                      `IProgressDialogInternal';
                    */




                    ppd->SetTitle(L"TitleTest");

                    auto hmod = GetModuleHandle(TEXT("SHELL32"));
                    ppd->SetAnimation(hmod, 165);

                    ppd->StartProgressDialog(windowHWND, NULL, PROGDLG_AUTOTIME, NULL);

                    ppd->SetLine(1, lines[0].c_str(), 1, NULL);
                    ppd->SetLine(2, lines[1].c_str(), 1, NULL);
                    //ppd->SetLine(2, L"test3", 0, NULL);

                    func(*this);

                    ppd->StopProgressDialog();
                    ppd->Release();
                }
            }
            this->Release();

            if (SUCCEEDED(hrOleInit))
                CoUninitialize();

        }).detach();
}

void ProgressDialogOperation::ResetTimer() const
{
    if (ppd)
        ppd->Timer(PDTIMER_RESET, NULL);
}

void ProgressDialogOperation::SetLineText(uint8_t line, std::wstring newText) const
{
    if (ppd)
        ppd->SetLine(line, newText.c_str(), 0, NULL);
}

void ProgressDialogOperation::SetTitle(std::wstring newText) const
{
    if (ppd)
        ppd->SetTitle(L"TitleTest");

}

void ProgressDialogOperation::SetProgress(uint64_t current, uint64_t max) const
{
    if (ppd)
        ppd->SetProgress64(current, max);
}
