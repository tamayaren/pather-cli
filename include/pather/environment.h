#pragma once

#include <string>
#include <vector>

namespace pather {

enum class Scope { User, System, All };

struct Entry {
    Scope scope;
    std::wstring name;
    std::wstring value;
};

struct Result {
    bool changed = false;
    bool found = false;
    std::wstring error;
};

std::vector<Entry> list(Scope scope, std::wstring& error);
Result set_value(Scope scope, const std::wstring& name, const std::wstring& value);
Result remove_name(Scope scope, const std::wstring& name);
Result remove_path(Scope scope, const std::wstring& path);
Result append_path(Scope scope, const std::wstring& path);
Result remove_path_entry(Scope scope, const std::wstring& path);
Result contains_path(Scope scope, const std::wstring& path, std::vector<Entry>& matches,
                    std::wstring& error);
bool notify_environment_change(std::wstring& error);

}
