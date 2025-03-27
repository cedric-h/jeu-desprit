mkdir build

cd build

git clone https://github.com/glfw/glfw
cmake -S glfw -B glfw_build

cmake --build glfw_build --target install

cd ..

./win_build.sh
