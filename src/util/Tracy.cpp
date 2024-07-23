#if _DEBUG

#define NOMINMAX
#include <winsock2.h>
#include <Windows.h>

// Cannot do this, because tracy's headers themselves do #include
//import std;
//import std.compat;


#define TRACY_ON_DEMAND
#define TRACY_ENABLE
#define TRACY_CALLSTACK 8
#define TRACY_TIMER_FALLBACK

//#ifndef _DEBUG
#define TRACY_NO_SYSTEM_TRACING
//#endif

#include "../lib/tracy/public/tracy/Tracy.hpp"

#include "../lib/tracy/public/client/TracyAlloc.cpp"
#include "../lib/tracy/public/client/TracyCallstack.cpp"
#include "../lib/tracy/public/client/TracyDxt1.cpp"
#include "../lib/tracy/public/client/TracyOverride.cpp"
#include "../lib/tracy/public/client/TracyProfiler.cpp"
#include "../lib/tracy/public/client/TracySysTime.cpp"
#include "../lib/tracy/public/client/TracySysTrace.cpp"
#include "../lib/tracy/public/client/tracy_rpmalloc.cpp"

#include "../lib/tracy/public/common/TracySystem.cpp"
#include "../lib/tracy/public/common/tracy_lz4.cpp"
#include "../lib/tracy/public/common/tracy_lz4hc.cpp"
#include "../lib/tracy/public/common/TracySocket.cpp"
#include "../lib/tracy/public/common/TracyStackFrames.cpp"
#pragma comment(lib, "ws2_32.lib")
#endif
import Tracy;


ProfilingScope::ProfilingScope(std::source_location sloc)
#if _DEBUG
    : ___tracy_scoped_zone(
        std::make_unique<tracy::ScopedZone>(
            sloc.line(),
            sloc.file_name(), strlen(sloc.file_name()),
            sloc.function_name(), strlen(sloc.function_name()),
            nullptr, 0,
            TRACY_CALLSTACK, true
        )
    )
#endif
{}

ProfilingScope::~ProfilingScope() {}
void ProfilingScope::SetValue(std::string_view val) {
#if _DEBUG
    ___tracy_scoped_zone->Text(val.data(), val.length());
#endif
}

