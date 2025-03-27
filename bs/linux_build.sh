cd build
cc -o jeu_desprit $(cat ../bs/cc_flags.txt) $(cat ../bs/cc_flags_dev.txt) \
   ../src/main.c libGLESv2.dylib `pkg-config sdl3 --cflags --libs` \
   && ./jeu_desprit
