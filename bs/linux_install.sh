mkdir -p build
cd build

git clone https://github.com/glfw/glfw
cmake -S glfw -B glfw_build
cd glfw_build && make
cd ..

./linux_build.sh
