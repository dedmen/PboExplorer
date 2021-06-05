#include "Util.hpp"
void Util::WaitForDebuggerSilent() {
    while (!IsDebuggerPresent())
        Sleep(10);    
}

void Util::WaitForDebuggerPrompt() {
    while (!IsDebuggerPresent())
        MessageBoxA(0, "PboExplorer waiting for debugger!", "PboExplorer waiting for debugger!", 0);
}
