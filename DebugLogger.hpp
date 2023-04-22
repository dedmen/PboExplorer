#pragma once
#include <source_location>
#include <string>

import DebugLogger;

#define DebugLogger_OnQueryInterfaceEntry(x) DebugLogger::OnQueryInterfaceEntry(x, std::source_location::current(), __FUNCTION__)
#define DebugLogger_OnQueryInterfaceExitUnhandled(x) DebugLogger::OnQueryInterfaceExitUnhandled(x, std::source_location::current(), __FUNCTION__)


#ifdef _DEBUG
#define RIID_TODO(x) if(IsEqualIID(riid, x)) return E_NOINTERFACE
// We know this interface is requested, but we deliberately don't implement it and don't want unimplemented warnings for it
#define RIID_IGNORED(x) if(IsEqualIID(riid, x)) return E_NOINTERFACE

#define EXPECT_SINGLE_PIDL(x) if((x)->GetLevelCount() != 1) Util::TryDebugBreak()

#else
#define RIID_TODO(x) if(false) return E_NOINTERFACE
// We know this interface is requested, but we deliberately don't implement it and don't want unimplemented warnings for it
#define RIID_IGNORED(x) if(false) return E_NOINTERFACE
#endif

