#pragma once

#include "pather/environment.h"
#include <string>
#include <vector>

namespace pather {

enum class Command { List, RemoveName, RemovePath, RemovePathEntry, AppendPath, Set, Check };

struct Options {
    Command command = Command::List;
    Scope scope = Scope::All;
    std::wstring name;
    std::wstring value;
    bool scopeSpecified = false;
};

bool parse(int argc, wchar_t* argv[], Options& options, std::wstring& error);
void print_usage();
int run(const Options& options);

}
