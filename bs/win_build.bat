@echo off

setlocal

set FLAGS=
if defined RELEASE (
    echo Building for release...
    FLAGS=03
    " for /f "delims=" %%i in (bs\cc_flags_release.txt) do set FLAGS=%FLAGS% %%i
) else (
    echo Building for development...
    FLAGS=-g
    " for /f "delims=" %%i in (bs\cc_flags_dev.txt) do set FLAGS=%FLAGS% %%i
)

cd build

if not exist angle (
    echo ERROR: No build/angle Directory; see Windows section in README
    exit /b 1
)

clang -o jeux.exe %FLAGS% ^
  ..\src\main.c ^
   angle\lib\libGLESv2.dll.lib ^
   SDL\build\SDL3.lib ^
   -ISDL/include ^
   -Wl,-subsystem:windows

cd ..
