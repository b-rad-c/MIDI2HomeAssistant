#include <stdio.h>
#include <stdlib.h>
#include "portmidi.h"
#include "porttime.h"

#define TIME_MSEC 10
#define BUFFER_SIZE 1024

int main() {
    PmError err;
    PmStream *midi;
    PmEvent buffer[BUFFER_SIZE];
    int i, dev;

    err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Error initializing PortMidi: %s\n", Pm_GetErrorText(err));
        return 1;
    }

    dev = Pm_GetDefaultOutputDeviceID();
    if (dev < 0) {
       printf("No default MIDI output device found.\n");
       Pm_Terminate();
       return 1;
    }

    err = Pm_OpenOutput(&midi, dev, NULL, BUFFER_SIZE, NULL, NULL, 0);
    if (err != pmNoError) {
        printf("Error opening MIDI output: %s\n", Pm_GetErrorText(err));
        Pm_Terminate();
        return 1;
    }

    Pt_Start(TIME_MSEC, NULL, NULL);

    // Send some MIDI messages (Note On/Off)
    for (i = 60; i < 70; i++) {
        buffer[0].message = Pm_Message(0x90, i, 100); // Note On, channel 1
        buffer[0].timestamp = Pt_Time();
        Pm_Write(midi, buffer, 1);
        Pt_Sleep(500);

        buffer[0].message = Pm_Message(0x80, i, 0); // Note Off, channel 1
        buffer[0].timestamp = Pt_Time();
        Pm_Write(midi, buffer, 1);
        Pt_Sleep(500);
    }

    Pt_Sleep(2000);
    Pm_Close(midi);
    Pm_Terminate();
    return 0;
}
