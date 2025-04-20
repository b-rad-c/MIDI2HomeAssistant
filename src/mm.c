/* mm.c -- midi monitor */

#include "stdlib.h"
#include "ctype.h"
#include "string.h"
#include "stdio.h"
#include "porttime.h"
#include "portmidi.h"
#include "signal.h"
#include <unistd.h>


#define MIDI_CODE_MASK  0xf0
#define MIDI_CHN_MASK   0x0f
#define MIDI_CTRL       0xb0
#define MIDI_ALL_SOUND_OFF 0x78

#define private static

#ifndef false
#define false 0
#define true 1
#endif

typedef int boolean;

int debug = false;	/* never set, but referenced by userio.c */
PmStream *midi_in;      /* midi input */
boolean active = false;     /* set when midi_in is ready for reading */
volatile sig_atomic_t done = 0;       /* when non zero, exit */;


/*****************************************************************************
*    Imported variables
*****************************************************************************/

extern  int     abort_flag;

/*****************************************************************************
*    Routines local to this module
*****************************************************************************/

private    void    handle_midi_event(PmMessage data);
private    int     get_user_input_as_number(const char *prompt);


char *nano_kontrol2_control_name(int control);


int get_user_input_as_number(const char *prompt) {
    /* read a number from console */
    int n = 0, i;
    fputs(prompt, stdout);
    while (n != 1) {
        n = scanf("%d", &i);
        while (getchar() != '\n') ;
    }
    return i;
}


void poll_midi_device(PtTimestamp timestamp, void *userData) {
    PmEvent event;
    int count;
    if (!active) return;
    while ((count = Pm_Read(midi_in, &event, 1))) {
        if (count == 1) handle_midi_event(event.message);
        else            puts(Pm_GetErrorText(count));
    }
}


void interrupt_handler(int dummy) {
    done = 1;
    printf("Caught signal %d\n", dummy);
}

void help_menu(int exit_code) {
    puts("Usage: mm [run|list] [--device <device_name>]");
    puts("Commands:");
    puts("  run                     Start the MIDI monitor.");
    puts("  list                    List available MIDI devices.");
    puts("Options:");
    puts("  --device <device_name>  Specify the MIDI device name to use. Default: 'nanoKONTROL2 nanoKONTROL2 _ CTR'");
    puts("  -h, --help              Show this help message.");
    exit(exit_code);
}


/****************************************************************************
*               main
****************************************************************************/

int main(int argc, char **argv) {

    PmError err;
    int i;
    char *command;
    char *device_name = "nanoKONTROL2 nanoKONTROL2 _ CTR";
    int device_index = -1;

    /*
    parse cli arguments
    */


    /* get command */
    if (argc > 1) {
        if (strcmp(argv[1], "run") == 0) {
            command = "run";
        } else if (strcmp(argv[1], "list") == 0) {
            command = "list";
        } else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            help_menu(0);
        } else {
            puts("Unknown command. Exiting.");
            help_menu(1);
        }
    }else {
        puts("No command provided. Exiting.");
        help_menu(1);
    }

    /* get options */
    if (argc > 2) {
        if (strcmp(argv[2], "--device") == 0) {
            if (argc > 3) {
                device_name = argv[3];
            } else {
                puts("Did not provide device name. Exiting.");
                help_menu(1);
            }
        } else if (strcmp(argv[2], "-h") == 0 || strcmp(argv[2], "--help") == 0) {
            help_menu(0);
        } else {
            puts("Unknown option. Exiting.");
            help_menu(1);
        }
    }

    /* 
    select / list input device 
    */
    
    Pt_Start(1, poll_midi_device, 0);
    if (strcmp(command, "list") == 0) puts("MIDI input devices:");

    for (i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);

        if (strcmp(command, "list") == 0 && info->input) printf("  %d) '%s'\n", i, info->name);

        if (strcmp(info->name, device_name) == 0) {
            device_index = i;
        }
    }

    if (strcmp(command, "list") == 0) {
        Pt_Stop();
        exit(0);
    }
    
    if (device_index == -1) {
        printf("Could not find device '%s'.\n", device_name);
        exit(1);
    }

    /* 
    init midi 
    */

    err = Pm_OpenInput(&midi_in, device_index, NULL, 512, NULL, NULL);
    if (err) {
        puts(Pm_GetErrorText(err));
        Pt_Stop();
        exit(1);
    }

    printf("Midi device opened: %s\n", device_name);

    printf("Midi Monitor ready. (pid: %d) (Control+C to exit)\n", getpid());
    active = true;

    /* 
    main loop 
    */

    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);

    while (!done) {}

    /* 
    clean up and exit 
    */

    printf("Midi Monitor exiting.\n");
    active = false;
    Pm_Close(midi_in);
    Pt_Stop();
    Pm_Terminate();

    return 0;

}


private void handle_midi_event(PmMessage data) {

    int command;    /* the current command */
    int chan;   /* the midi channel of the current event */
    int control; /* the controller number of the current event */
    int value;  /* the controller value of the current event */

    command = Pm_MessageStatus(data) & MIDI_CODE_MASK;
    chan = Pm_MessageStatus(data) & MIDI_CHN_MASK;
    control = Pm_MessageData1(data);
    value = Pm_MessageData2(data);

    if (command == MIDI_CTRL) {
        if (Pm_MessageData1(data) < MIDI_ALL_SOUND_OFF) {
            printf("%s Chan %2d Ctrl %2d Val %2d\n", nano_kontrol2_control_name(control), chan, control, value);
        }
    }
    fflush(stdout);
}

char* nano_kontrol2_control_name(int control) {
    switch (control) {
        case 0: return "fader1";
        case 1: return "fader2";
        case 2: return "fader3";
        case 3: return "fader4";
        case 4: return "fader5";
        case 5: return "fader6";
        case 6: return "fader7";
        case 7: return "fader8";

        case 16: return "pot1";
        case 17: return "pot2";
        case 18: return "pot3";
        case 19: return "pot4";
        case 20: return "pot5";
        case 21: return "pot6";
        case 22: return "pot7";
        case 23: return "pot8";

        case 32: return "solo1";
        case 33: return "solo2";
        case 34: return "solo3";
        case 35: return "solo4";
        case 36: return "solo5";
        case 37: return "solo6";
        case 38: return "solo7";
        case 39: return "solo8";
        
        case 41: return "play";
        case 42: return "stop";
        case 43: return "rewind";
        case 44: return "fast-forward";
        case 45: return "record";

        case 46: return "cycle";

        case 48: return "mute1";
        case 49: return "mute2";
        case 50: return "mute3";
        case 51: return "mute4";
        case 52: return "mute5";
        case 53: return "mute6";
        case 54: return "mute7";
        case 55: return "mute8";

        case 58: return "track-left";
        case 59: return "track-right";

        case 60: return "marker-set";
        case 61: return "marker-left";
        case 62: return "marker-right";

        case 64: return "record1";
        case 65: return "record2";
        case 66: return "record3";
        case 67: return "record4";
        case 68: return "record5";
        case 69: return "record6";
        case 70: return "record7";
        case 71: return "record8";

        default: return "unknown";
    }
}