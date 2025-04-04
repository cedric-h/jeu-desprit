FLAGS="$(head bs/cc_flags.txt) $(head bs/cc_flags_dev.txt)"

if [ -n "$RELEASE" ]; then
    echo "building in release ..."
    FLAGS="$(head bs/cc_flags.txt) $(head bs/cc_flags_release.txt)"
fi

echo $FLAGS

cd build
cc -o jeu_desprit $FLAGS \
   ../src/main.c libGLESv2.dylib `pkg-config sdl3 --cflags --libs` \
   && ./jeu_desprit
