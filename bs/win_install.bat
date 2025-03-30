mkdir build

cd build

git clone https://github.com/libsdl-org/SDL
cd SDL
git checkout release-3.2.8
cmake -S ./ -B ./build
cmake --build build
cd ..

copy angle\bin\libGLESv2.dll .\
copy angle\bin\libEGL.dll .\
copy SDL\build\SDL3.dll .\

cd ..

call .\bs\win_build_run.bat
