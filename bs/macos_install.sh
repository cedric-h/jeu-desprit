mkdir -p build
cd build

chrome_versions=("/Applications/Google Chrome.app/Contents/Frameworks/Google Chrome Framework.framework/Versions/"*)
cp "${chrome_versions[0]}/Libraries/libEGL.dylib" libEGL.dylib
cp "${chrome_versions[0]}/Libraries/libGLESv2.dylib" libGLESv2.dylib

cd ..

./bs/macos_build.sh
