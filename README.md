# to run

"bs" stands for "build scripts" 😘

## loonix 🐧
install make, cmake, sdl3, pkg-config and OpenGL ES 3 drivers from your package manager.
```
./bs/linux_install.sh
./bs/linux_build.sh
```

## MackOhEss 🍎
`brew install sdl3 pkg-config`

This will try to copy ANGLE's libGLESv2.dylib out of your Google Chrome install.

More details, scripts, and examples [here](https://github.com/erik-larsen/gles-for-mac).

If you don't have one, you should clone and build ANGLE (TODO: automate this as part of the build script; there are example commands in the link above)

```
./bs/macos_install.sh
./bs/macos_build.sh
```
Release build:
```
RELEASE=1 ./bs/macos_build.sh
```

## Windoze 🪟
highly recommended debugger: [raddebugger](https://github.com/EpicGamesExt/raddebugger/)

highly recommended profiler: [superluminal](https://superluminal.eu/)

(call is not needed if using powershell)
```
call ./bs/win_install.bat
```

download ANGLE from [here](https://github.com/mmozeiko/build-angle/releases), rename it "angle" and drop it in `build/`

Example:
```
move "C:\Users\cedric\Downloads\angle-x64-2025-03-23\angle-x64" "build\angle"
```

Run install again; you should get a window now
```
call ./bs/win_install.bat
```

### Build and run

Henceforth you can simply call `win_build_run` to build and run:
```
call ./bs/win_build_run.bat
```

### Debugger

Or if you're using a debugger, you may prefer to simply `call win_build.bat`
```
call ./bs/win_build.bat
```

### Release

Release build, cmd.exe:
```
set RELEASE=1 && bs\win_build.bat
```

Release build, powershell:
```
$env:RELEASE="1"; .\bs\win_build.bat
```
