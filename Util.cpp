#include "Util.hpp"
void Util::WaitForDebuggerSilent() {
    if (IsDebuggerPresent())
        return;

    while (!IsDebuggerPresent())
        Sleep(10);    

    // We waited for attach, so break us there
    __debugbreak();
}

void Util::WaitForDebuggerPrompt() {
    if (IsDebuggerPresent())
        return;

    while (!IsDebuggerPresent())
        MessageBoxA(0, "PboExplorer waiting for debugger!", "PboExplorer waiting for debugger!", 0);

    // We waited for attach, so break us there
    __debugbreak();
}
