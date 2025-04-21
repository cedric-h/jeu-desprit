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

# Updating the map

1. Open models/blend/map.blend in Blender 4.4.0

2. Duplicate the Map Collection
<img width="229" alt="Screenshot 2025-04-20 at 9 11 03 PM" src="https://github.com/user-attachments/assets/c753628c-f453-4691-94be-249d6f06cf4c" />

3. Hide the old Map Collection, select everything in the new one.
<img width="653" alt="Screenshot 2025-04-20 at 9 11 42 PM" src="https://github.com/user-attachments/assets/9de118ef-fcb4-4138-9db5-433e0798ed8b" />

4. Join them all into one object
<img width="521" alt="Screenshot 2025-04-20 at 9 12 59 PM" src="https://github.com/user-attachments/assets/cd1e620b-1b8b-47ff-aedd-9398545b5da0" />

5. Name the object appropriately, e.g. IntroGravestoneTerrain

5. Select the resulting object and Export > Wavefront OBJ
<img width="583" alt="Screenshot 2025-04-20 at 9 13 16 PM" src="https://github.com/user-attachments/assets/9a8618fe-cd63-4e17-ba4e-199f2cb07ad9" />

6. Configure like so (It can help to create an Operator Preset with these options)
<img width="238" alt="Screenshot 2025-04-20 at 9 14 08 PM" src="https://github.com/user-attachments/assets/44170dfc-21b9-441d-85ef-58ecb070cb4c" />

7. Save it to the `models/obj/` folder, not the `models/blend/` folder (which will be the default option)
<img width="569" alt="Screenshot 2025-04-20 at 9 14 44 PM" src="https://github.com/user-attachments/assets/87c1a009-8af0-4678-a9d6-005e3cf71fe9" />

9. Delete the duplicate Collection or exit without saving.

10. `cd` into `models/` and run `./build.sh` (Run the OBJ cooker)
<img width="862" alt="Screenshot 2025-04-20 at 9 16 14 PM" src="https://github.com/user-attachments/assets/5ed81832-cb80-43cb-8571-83634a114dd3" />

11. Compile and run as usual
