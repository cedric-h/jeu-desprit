# to run

"bs" stands for "build scripts" 😘

## loonix 🐧
install make, cmake, OpenGLES2, etc.
```
./bs/linux_install.sh
./bs/linux_build.sh
```

## MackOhEss 🍎
`brew install sdl3 pkg-config`

This will try to copy ANGLE's libGLESv2.dylib out of your Google Chrome install.

If you don't have one, you should clone and build ANGLE (TODO: automate this as part of the build script)

```
./bs/macos_install.sh
./bs/macos_build.sh
```

## Windoze 🪟
recommended: [raddebugger](https://github.com/EpicGamesExt/raddebugger/), [superliminal](https://superluminal.eu/)
```
./bs/windows_bat.sh
./bs/windows_bat.sh
```
