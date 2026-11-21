#!/bin/sh
# Builds Epingle.exe with LLVM only (clang, lld-link, llvm-rc, llvm-dlltool).
# No Windows SDK and no C runtime needed. Works on Linux, macOS or Windows (Git Bash).
set -e
cd "$(dirname "$0")"
mkdir -p build

for d in lib/*.def; do
  llvm-dlltool -m i386:x86-64 -d "$d" -l "build/$(basename "$d" .def).lib"
done

(cd src && llvm-rc /nologo /C 65001 /fo ../build/app.res app.rc)

clang -target x86_64-pc-windows-msvc -O2 -Wall -Wextra -Wno-unused-parameter \
  -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe \
  -c src/main.c -o build/main.obj

lld-link /nologo /subsystem:windows,10.0 /entry:WinMainCRTStartup /nodefaultlib \
  /opt:ref /opt:icf /dynamicbase /nxcompat /highentropyva /release \
  /out:build/Epingle.exe build/main.obj build/app.res \
  build/kernel32.lib build/user32.lib build/gdi32.lib build/shell32.lib \
  build/advapi32.lib build/dwmapi.lib build/shcore.lib

ls -l build/Epingle.exe
