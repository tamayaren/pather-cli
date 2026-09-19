#include "pather/cli.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <iostream>
#include <io.h>
#include <string>

namespace pather {
namespace {

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

bool scope_token(const std::wstring& token, Scope& scope) {
    const std::wstring value = lower(token);
    if (value == L"user") {
        scope = Scope::User;
    } else if (value == L"sys" || value == L"system") {
        scope = Scope::System;
    } else if (value == L"all") {
        scope = Scope::All;
    } else {
        return false;
    }
    return true;
}

const wchar_t* scope_label(Scope scope) {
    return scope == Scope::User ? L"user" : L"sys";
}

bool write_text(const std::wstring& text, bool error_stream) {
    FILE* stream = error_stream ? stderr : stdout;
    const intptr_t descriptor = _fileno(stream);
    const intptr_t native_handle = _get_osfhandle(descriptor);
    if (native_handle != -1) {
        const HANDLE handle = reinterpret_cast<HANDLE>(native_handle);
        DWORD console_mode = 0;
        if (GetConsoleMode(handle, &console_mode)) {
            size_t offset = 0;
            while (offset < text.size()) {
                const DWORD chunk = static_cast<DWORD>(std::min<size_t>(
                    text.size() - offset, static_cast<size_t>(MAXDWORD)));
                DWORD written = 0;
                if (!WriteConsoleW(handle, text.data() + offset, chunk, &written, nullptr) ||
                    written == 0) {
                    return false;
                }
                offset += written;
            }
            return true;
        } else {
            if (text.size() > static_cast<size_t>(INT_MAX)) return false;
            int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (bytes <= 0) {
                bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                            nullptr, 0, nullptr, nullptr);
            }
            if (bytes > 0) {
                std::string utf8(static_cast<size_t>(bytes), '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                    utf8.data(), bytes, nullptr, nullptr);
                size_t offset = 0;
                while (offset < utf8.size()) {
                    const DWORD chunk = static_cast<DWORD>(std::min<size_t>(
                        utf8.size() - offset, static_cast<size_t>(MAXDWORD)));
                    DWORD written = 0;
                    if (!WriteFile(handle, utf8.data() + offset, chunk, &written, nullptr) ||
                        written == 0) {
                        return false;
                    }
                    offset += written;
                }
                return true;
            }
            return false;
        }
    }

    std::wostream& output = error_stream ? std::wcerr : std::wcout;
    output << text;
    return !output.fail();
}

}  // namespace

void print_usage() {
    write_text(
        L"Usage:\n"
        L"  pather -ls [-e user|sys|all]\n"
        L"  pather -rm -n NAME [-e user|sys|all]\n"
        L"  pather -rm -p PATH [-e user|sys|all]\n"
        L"  pather -rm PATH [user|sys|all]\n"
        L"  pather PATH [user|sys|all]\n"
        L"  pather NAME PATH [-e user|sys|all]\n"
        L"  pather -c PATH\n",
        true);
}

bool parse(int argc, wchar_t* argv[], Options& options, std::wstring& error) {
    options = Options{};
    if (argc < 2) {
        error = L"missing command";
        return false;
    }

    int index = 1;
    const std::wstring first = lower(argv[index]);

    if (first == L"-ls") {
        options.command = Command::List;
        ++index;
    } else if (first == L"-c") {
        options.command = Command::Check;
        if (++index >= argc || std::wstring(argv[index]).empty()) {
            error = L"-c requires a non-empty PATH";
            return false;
        }
        options.value = argv[index++];
        if (index != argc) {
            error = L"-c accepts only PATH";
            return false;
        }
        return true;
    } else if (first == L"-rm") {
        ++index;
        if (index >= argc) {
            error = L"-rm requires -n NAME, -p PATH, or PATH";
            return false;
        }

        const std::wstring remove_option = lower(argv[index]);
        if (remove_option == L"-n" || remove_option == L"-p") {
            const bool by_name = remove_option == L"-n";
            if (++index >= argc || std::wstring(argv[index]).empty()) {
                error = L"remove option requires a non-empty value";
                return false;
            }
            options.command = by_name ? Command::RemoveName : Command::RemovePath;
            options.value = argv[index++];
        } else {
            if (std::wstring(argv[index]).empty() || remove_option[0] == L'-') {
                error = L"-rm requires -n NAME, -p PATH, or PATH";
                return false;
            }
            options.command = Command::RemovePathEntry;
            options.value = argv[index++];
            if (index < argc && scope_token(argv[index], options.scope)) {
                options.scopeSpecified = true;
                ++index;
            }
        }
    } else {
        if (first.empty() || first[0] == L'-' || std::wstring(argv[index]).empty()) {
            error = L"expected PATH, or non-empty NAME PATH";
            return false;
        }
        options.value = argv[index++];
        if (index == argc || lower(argv[index]) == L"-e") {
            options.command = Command::AppendPath;
        } else {
            Scope positional_scope = Scope::All;
            if (scope_token(argv[index], positional_scope)) {
                const bool has_explicit_scope_option =
                    index + 1 < argc && lower(argv[index + 1]) == L"-e";
                if (has_explicit_scope_option) {
                    options.command = Command::Set;
                    options.name = options.value;
                    options.value = argv[index++];
                } else {
                    options.command = Command::AppendPath;
                    options.scope = positional_scope;
                    options.scopeSpecified = true;
                    ++index;
                }
            } else {
                if (std::wstring(argv[index]).empty()) {
                    error = L"expected non-empty NAME PATH";
                    return false;
                }
                options.command = Command::Set;
                options.name = options.value;
                options.value = argv[index++];
            }
        }
    }

    while (index < argc) {
        if (lower(argv[index]) != L"-e") {
            error = L"unexpected argument; expected -e SCOPE";
            return false;
        }
        if (options.scopeSpecified) {
            error = L"scope may be specified only once";
            return false;
        }
        if (++index >= argc || !scope_token(argv[index], options.scope)) {
            error = L"invalid scope; use user, sys, or all";
            return false;
        }
        options.scopeSpecified = true;
        ++index;
    }

    if (options.command == Command::List && !options.scopeSpecified) {
        options.scope = Scope::All;
    } else if ((options.command == Command::Set || options.command == Command::AppendPath) &&
               !options.scopeSpecified) {
        options.scope = Scope::User;
    }
    if ((options.command == Command::AppendPath || options.command == Command::RemovePathEntry) &&
        options.value.find(L';') != std::wstring::npos) {
        error = L"Path entries must not contain ';'";
        return false;
    }
    return true;
}

int run(const Options& options) {
    std::wstring error;

    if (options.command == Command::List) {
        const auto entries = list(options.scope, error);
        for (const auto& entry : entries) {
            if (!write_text(L"[" + std::wstring(scope_label(entry.scope)) + L"] " +
                                entry.name + L"=" + entry.value + L"\n",
                            false)) {
                return 3;
            }
        }
        if (!error.empty()) {
            write_text(error + L"\n", true);
            return 3;
        }
        return 0;
    }

    if (options.command == Command::Check) {
        std::vector<Entry> matches;
        contains_path(Scope::All, options.value, matches, error);
        for (const auto& match : matches) {
            if (!write_text(L"[" + std::wstring(scope_label(match.scope)) + L"] " +
                                match.name + L"=" + match.value + L"\n",
                            false)) {
                return 3;
            }
        }
        if (!error.empty()) {
            write_text(error + L"\n", true);
            return 3;
        }
        return matches.empty() ? 1 : 0;
    }

    Result result;
    switch (options.command) {
        case Command::Set:
            result = set_value(options.scope, options.name, options.value);
            break;
        case Command::AppendPath:
            result = append_path(options.scope, options.value);
            break;
        case Command::RemoveName:
            result = remove_name(options.scope, options.value);
            break;
        case Command::RemovePath:
            result = remove_path(options.scope, options.value);
            break;
        case Command::RemovePathEntry:
            result = remove_path_entry(options.scope, options.value);
            break;
        default:
            result.error = L"invalid mutation command";
            break;
    }

    if (!result.error.empty()) {
        write_text(result.error + L"\n", true);
        if (result.changed) {
            std::wstring notification_error;
            if (!notify_environment_change(notification_error)) {
                write_text(L"change notification also failed: " + notification_error + L"\n", true);
            }
        }
        return 3;
    }
    if (!result.found) {
        return 1;
    }
    if (!result.changed) {
        return 0;
    }

    if (!notify_environment_change(error)) {
        write_text(L"change succeeded but notification failed: " + error + L"\n", true);
        return 3;
    }
    return 0;
}

}  // namespace pather
