#pragma once

#include "pather/environment.h"
#include <string>
#include <vector>

namespace pather {

enum class Command { Help, List, ListPath, RemoveName, RemovePath, RemovePathEntry, AppendPath, Set, Check };

struct Options {
    Command command = Command::List;
    Scope scope = Scope::All;
    std::wstring name;
    std::wstring value;
    bool scopeSpecified = false;
};

bool parse(int argc, wchar_t* argv[], Options& options, std::wstring& error);
bool print_usage(bool error_stream = true);
int run(const Options& options);

}
