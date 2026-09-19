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
g++ -std=c++17 -Iinclude -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode -static -static-libgcc -static-libstdc++ src/main.cpp src/cli.cpp src/environment.cpp -ladvapi32 -luser32 -o Pather.exe
```

The MinGW runtime libraries are linked statically, so adding the folder containing
`Pather.exe` to `PATH` does not also require adding the MSYS2 `ucrt64\bin` folder.

In PowerShell, add the containing folder (for example, `G:\Projects\pather-cli`),
not the `.exe` file. Open a new terminal after changing `PATH`; an existing shell
keeps its original environment. From the current directory, run `.\pather.exe`.

Run the commands from a Windows/MinGW environment. System-scope changes normally
require an elevated shell.

Exit status is `0` for success, `1` when a requested entry/path is not found, `2` for invalid usage, and `3` for an operational failure. `-ls` defaults to both scopes; setting and Path appending default to user; removal defaults to both when `-e` is omitted. System changes generally require elevation. Mutations broadcast `WM_SETTINGCHANGE` for `Environment`.

Names and values are compared case-insensitively using Windows ordinal comparison.
Generic value matching is exact: Pather does not trim, normalize, or expand values.
The Path-specific forms treat `;` as the separator, preserve the existing registry
value type, and match each entry exactly without trimming or expansion. Listing and
checking show the matching scope and variable name.

When the second positional argument is `user`, `sys`, `system`, or `all`, the command
is interpreted as Path appending. To set an arbitrary variable to one of those literal
values, use an explicit scope option, for example `pather MODE sys -e user`.
Individual Path operands must not contain `;`.

Examples:

```text
pather -ls
pather -ls -e sys
pather MyPath "C:\\Program Files\\Tool" -e user
pather "C:\\Tools" user
pather "C:\\Tools" sys
pather -rm "C:\\Tools" user
pather -rm "C:\\Tools" all
pather -rm -n MyPath -e all
pather -rm -p "C:\\Program Files\\Tool" -e all
pather -c "C:\\Program Files\\Tool"
```

`pather PATH [user|sys|all]` appends an entry to the selected `Path` registry
variable. `pather -rm PATH [user|sys|all]` removes matching entries from that
variable. The existing `-rm -p PATH` form remains the generic exact-value removal
operation for compatibility.
