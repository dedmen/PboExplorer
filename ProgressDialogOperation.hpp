#pragma once
#include <functional>
#include <string>

#include <Unknwnbase.h>

#include "ComRef.hpp"
struct IProgressDialog;

class ProgressDialogOperation : public RefCounted<ProgressDialogOperation>
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

