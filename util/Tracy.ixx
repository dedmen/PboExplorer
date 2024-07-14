module;


export module Tracy;
import std;
import std.compat;


extern "C++" namespace tracy {
    class ScopedZone;
}

export class ProfilingScope {
#if _DEBUG
    std::unique_ptr<tracy::ScopedZone> ___tracy_scoped_zone;
#endif
public:
    ProfilingScope(std::source_location sloc = std::source_location::current());
    ~ProfilingScope();
    void SetValue(std::string_view val);
};

