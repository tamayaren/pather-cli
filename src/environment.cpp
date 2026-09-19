#include "pather/environment.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <functional>
#include <iterator>

namespace pather {
namespace {

struct Root {
    HKEY key;
    const wchar_t* subkey;
    const wchar_t* label;
    Scope scope;
};

Root root(Scope scope) {
    return scope == Scope::User
        ? Root{HKEY_CURRENT_USER, L"Environment", L"user", Scope::User}
        : Root{HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", L"system", Scope::System};
}

std::vector<Root> roots(Scope scope) {
    if (scope == Scope::All) {
        return {root(Scope::User), root(Scope::System)};
    }
    return {root(scope)};
}

std::wstring win_error(LONG code) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, code, 0,
                                        reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring result = length ? std::wstring(buffer, length) : L"Windows error " + std::to_wstring(code);
    if (buffer) LocalFree(buffer);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) result.pop_back();
    return result;
}

bool equal_fold(const std::wstring& left, const std::wstring& right) {
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

struct EnumeratedValue {
    std::wstring name;
    DWORD type = 0;
    std::vector<BYTE> data;
};

bool is_string_type(DWORD type) {
    return type == REG_SZ || type == REG_EXPAND_SZ;
}

LONG read_value(HKEY key, DWORD index, EnumeratedValue& value) {
    DWORD nameCapacity = 256;
    DWORD dataCapacity = 256;

    for (;;) {
        std::vector<wchar_t> name(nameCapacity);
        std::vector<BYTE> data(dataCapacity);
        DWORD nameLength = nameCapacity;
        DWORD dataSize = dataCapacity;
        DWORD type = 0;

        const LONG status = RegEnumValueW(key, index, name.data(), &nameLength, nullptr,
                                          &type, data.data(), &dataSize);
        if (status == ERROR_SUCCESS) {
            value.name.assign(name.data(), nameLength);
            value.type = type;
            value.data.assign(data.begin(), data.begin() + dataSize);
            return status;
        }
        if (status != ERROR_MORE_DATA) return status;

        if (nameLength >= nameCapacity) {
            nameCapacity = std::max(nameCapacity * 2, nameLength + 1);
        }
        if (dataSize >= dataCapacity) {
            dataCapacity = std::max(dataCapacity * 2, dataSize + 1);
        }
    }
}

std::wstring string_value(const EnumeratedValue& value) {
    if (!is_string_type(value.type) || value.data.size() < sizeof(wchar_t)) return {};

    std::wstring result(reinterpret_cast<const wchar_t*>(value.data.data()),
                        value.data.size() / sizeof(wchar_t));
    if (!result.empty() && result.back() == L'\0') result.pop_back();
    return result;
}

void append_error(std::wstring& target, const std::wstring& error) {
    if (!target.empty()) target += L"; ";
    target += error;
}

bool open_key(const Root& descriptor, REGSAM access, HKEY& key, std::wstring& error) {
    const LONG status = RegOpenKeyExW(descriptor.key, descriptor.subkey, 0, access, &key);
    if (status != ERROR_SUCCESS) {
        error = std::wstring(descriptor.label) + L" environment: " + win_error(status);
        return false;
    }
    return true;
}

Result mutation_error(const std::wstring& error) { Result result; result.error = error; return result; }

Result mutate(Scope scope, const std::function<Result(const Root&)>& operation) {
    Result total;
    for (const Root& descriptor : roots(scope)) {
        Result current = operation(descriptor);
        total.changed = total.changed || current.changed;
        total.found = total.found || current.found;
        if (!current.error.empty()) {
            if (!total.error.empty()) total.error += L"; ";
            total.error += current.error;
        }
    }
    return total;
}

Result remove_from_root(const Root& descriptor, const std::function<bool(const std::wstring&, const std::wstring&)>& predicate) {
    HKEY key = nullptr;
    std::wstring error;
    if (!open_key(descriptor, KEY_QUERY_VALUE | KEY_SET_VALUE, key, error)) return mutation_error(error);
    Result result;
    std::vector<std::wstring> names_to_delete;
    DWORD index = 0;
    while (true) {
        EnumeratedValue value;
        const LONG status = read_value(key, index, value);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS) {
            result.error = std::wstring(descriptor.label) + L" environment: " + win_error(status);
            break;
        }
        const std::wstring string = string_value(value);
        if (is_string_type(value.type) && predicate(value.name, string)) {
            names_to_delete.push_back(value.name);
        }
        ++index;
    }

    if (result.error.empty()) {
        for (const std::wstring& name : names_to_delete) {
            const LONG status = RegDeleteValueW(key, name.c_str());
            if (status != ERROR_SUCCESS) {
                append_error(result.error,
                             std::wstring(descriptor.label) + L" environment: " + win_error(status));
            } else {
                result.changed = true;
                result.found = true;
            }
        }
    }
    RegCloseKey(key);
    return result;
}

}

std::vector<Entry> list(Scope scope, std::wstring& error) {
    std::vector<Entry> entries;
    for (const Root& descriptor : roots(scope)) {
        HKEY key = nullptr;
        std::wstring openError;
        if (!open_key(descriptor, KEY_QUERY_VALUE, key, openError)) {
            append_error(error, openError);
            continue;
        }
        for (DWORD index = 0;; ++index) {
            EnumeratedValue value;
            const LONG status = read_value(key, index, value);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS) {
                append_error(error, std::wstring(descriptor.label) + L" environment: " + win_error(status));
                break;
            }
            if (is_string_type(value.type)) {
                entries.push_back({descriptor.scope, value.name, string_value(value)});
            }
        }
        RegCloseKey(key);
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.scope != b.scope) return a.scope == Scope::User;
        if (a.name != b.name) return a.name < b.name;
        return a.value < b.value;
    });
    return entries;
}

Result set_value(Scope scope, const std::wstring& name, const std::wstring& value) {
    return mutate(scope, [&](const Root& descriptor) {
        HKEY key = nullptr; std::wstring error;
        if (!open_key(descriptor, KEY_SET_VALUE, key, error)) return mutation_error(error);
        const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        const LONG status = RegSetValueExW(key, name.c_str(), 0, REG_SZ,
                                           reinterpret_cast<const BYTE*>(value.c_str()), bytes);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) return mutation_error(std::wstring(descriptor.label) + L" environment: " + win_error(status));
        Result result; result.changed = result.found = true; return result;
    });
}

Result remove_name(Scope scope, const std::wstring& name) {
    return mutate(scope, [&](const Root& descriptor) { return remove_from_root(descriptor, [&](const std::wstring& candidate, const std::wstring&) { return equal_fold(candidate, name); }); });
}

Result remove_path(Scope scope, const std::wstring& path) {
    return mutate(scope, [&](const Root& descriptor) { return remove_from_root(descriptor, [&](const std::wstring&, const std::wstring& value) { return equal_fold(value, path); }); });
}

Result contains_path(Scope scope, const std::wstring& path, std::vector<Entry>& matches, std::wstring& error) {
    for (const Root& descriptor : roots(scope)) {
        std::wstring localError; const auto values = list(descriptor.scope, localError);
        if (!localError.empty()) { error += (error.empty() ? L"" : L"; ") + localError; continue; }
        for (const Entry& entry : values) {
            if (equal_fold(entry.value, path)) matches.push_back(entry);
        }
    }
    return {};
}

bool notify_environment_change(std::wstring& error) {
    DWORD_PTR result = 0;
    if (!SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"),
                             SMTO_ABORTIFHUNG, 5000, &result)) { error = win_error(GetLastError()); return false; }
    return true;
}

}
