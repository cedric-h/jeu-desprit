@echo off

cd build
clang -o bloom.exe -DWIN32 ^
  ..\src\main.c glfw_build/src/Debug/glfw3.lib ^
   -luser32 -lgdi32 -lmsvcrt -lshell32.lib -lvcruntime.lib ^
   -Iglfw/deps -Iglfw/include ^
   -Wl,/NODEFAULTLIB:libcmt -Wl,/NODEFAULTLIB:msvcrtd

bloom.exe

cd ..
