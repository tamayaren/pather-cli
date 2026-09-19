# Pather

Pather is a Windows CLI for the registry-backed user and system environment variables.
It stores user values under `HKCU\Environment` and system values under
`HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment`.

This is just my wrapper.

Build with MinGW-w64 or MSVC:

```text
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

The same sources can be built directly with MinGW-w64 GCC:

```text
g++ -std=c++17 -Iinclude -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode src/main.cpp src/cli.cpp src/environment.cpp -ladvapi32 -luser32 -o Pather.exe
```

Run the commands from a Windows/MinGW environment. System-scope changes normally
require an elevated shell.

Exit status is `0` for success, `1` when a requested entry/path is not found, `2` for invalid usage, and `3` for an operational failure. `-ls` defaults to both scopes; setting defaults to user; removal defaults to both when `-e` is omitted. System changes generally require elevation. Mutations broadcast `WM_SETTINGCHANGE` for `Environment`.

Names and values are compared case-insensitively using Windows ordinal comparison.
Path matching is exact: Pather does not trim, normalize, expand, or split values on
semicolons. Listing and checking show the matching scope and variable name.

Examples:

```text
pather -ls
pather -ls -e sys
pather MyPath "C:\\Program Files\\Tool" -e user
pather -rm -n MyPath -e all
pather -rm -p "C:\\Program Files\\Tool" -e all
pather -c "C:\\Program Files\\Tool"
```
