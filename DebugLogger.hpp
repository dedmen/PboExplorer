#pragma once
#include <guiddef.h>
#include <source_location>
#include <string>


#if _DEBUG
#define LOG_DEBUG 1
#endif



#if LOG_DEBUG

#else

#endif


enum class DebugInterestLevel {
	NotInterested,
	Normal,
	Interested
};

using LookupInfoT = std::pair<std::string, DebugInterestLevel>;


class DebugLogger
{

public:
	// std::source_location::current();
	static void OnQueryInterfaceEntry(const GUID& riid, const std::source_location location, const char* funcName);
	static void OnQueryInterfaceExitUnhandled(const GUID& riid, const std::source_location location, const char* funcName);
	static void TraceLog(const std::string& message, const std::source_location location, const char* funcName);
	static void TraceLog(const std::wstring& message, const std::source_location location, const char* funcName);

	static bool IsIIDUninteresting(const GUID& riid);

private:

	static LookupInfoT GetGUIDName(const GUID& guid);
};



#define DebugLogger_OnQueryInterfaceEntry(x) DebugLogger::OnQueryInterfaceEntry(x, std::source_location::current(), __FUNCTION__)
#define DebugLogger_OnQueryInterfaceExitUnhandled(x) DebugLogger::OnQueryInterfaceExitUnhandled(x, std::source_location::current(), __FUNCTION__)


#define RIID_TODO(x) if(IsEqualIID(riid, x)) return E_NOINTERFACE

