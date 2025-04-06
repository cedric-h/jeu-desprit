cd build
gcc ../obj_cooker.c -g -Werror -fsanitize=address -o obj_cooker || { exit 1; }
cd ..
./build/obj_cooker
