#/bin/bash

# check if the build directory exists, if not create it
if [ ! -d "dist" ]; then
  mkdir dist
fi

gcc -o dist/mm src/mm.c -lportmidi