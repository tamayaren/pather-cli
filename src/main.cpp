#include "pather/cli.h"

#include <iostream>

int wmain(int argc, wchar_t* argv[]) {
    pather::Options options;
    std::wstring error;
    if (!pather::parse(argc, argv, options, error)) {
        std::wcerr << L"Error: " << error << L"\n";
        pather::print_usage();
        return 2;
    }
    return pather::run(options);
}
