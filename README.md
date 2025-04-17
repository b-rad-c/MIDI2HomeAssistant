# MIDI2HomeAssistant

## setup on razz pi zero w

    sudo apt-get install libcurl4-openssl-dev

follow instructions in portmidi to install to system, then: 

    sudo ln -s /usr/local/lib/libportmidi.so.2 /usr/lib/libportmidi.so.2

after that examples can be compiled from `pm_test` folder like:

    gcc -o mm mm.c -lportmidi