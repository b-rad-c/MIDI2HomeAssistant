/* mm.c -- midi monitor */

/*****************************************************************************
*       Change Log
*  Date | Change
*-----------+-----------------------------------------------------------------
*  7-Apr-86 | Created changelog
* 31-Jan-90 | GWL : use new cmdline
*  5-Apr-91 | JDW : Further changes
* 16-Feb-92 | GWL : eliminate label mmexit:; add error recovery
* 18-May-92 | GWL : continuous clocks, etc.
* 17-Jan-94 | GWL : option to display notes
* 20-Nov-06 | RBD : port mm.c from CMU Midi Toolkit to PortMidi
*           |       mm.c -- revealing MIDI secrets for over 20 years!
*****************************************************************************/

#include "stdlib.h"
#include "ctype.h"
#include "string.h"
#include "stdio.h"
#include "porttime.h"
#include "portmidi.h"
#include "signal.h"

#define STRING_MAX 80

#define MIDI_CODE_MASK  0xf0
#define MIDI_CHN_MASK   0x0f
/*#define MIDI_REALTIME   0xf8
  #define MIDI_CHAN_MODE  0xfa */
#define MIDI_OFF_NOTE   0x80
#define MIDI_ON_NOTE    0x90
#define MIDI_POLY_TOUCH 0xa0
#define MIDI_CTRL       0xb0
#define MIDI_CH_PROGRAM 0xc0
#define MIDI_TOUCH      0xd0
#define MIDI_BEND       0xe0

#define MIDI_SYSEX      0xf0
#define MIDI_Q_FRAME	0xf1
#define MIDI_SONG_POINTER 0xf2
#define MIDI_SONG_SELECT 0xf3
#define MIDI_TUNE_REQ	0xf6
#define MIDI_EOX        0xf7
#define MIDI_TIME_CLOCK 0xf8
#define MIDI_START      0xfa
#define MIDI_CONTINUE	0xfb
#define MIDI_STOP       0xfc
#define MIDI_ACTIVE_SENSING 0xfe
#define MIDI_SYS_RESET  0xff

#define MIDI_ALL_SOUND_OFF 0x78
#define MIDI_RESET_CONTROLLERS 0x79
#define MIDI_LOCAL	0x7a
#define MIDI_ALL_OFF	0x7b
#define MIDI_OMNI_OFF	0x7c
#define MIDI_OMNI_ON	0x7d
#define MIDI_MONO_ON	0x7e
#define MIDI_POLY_ON	0x7f


#define private static

#ifndef false
#define false 0
#define true 1
#endif

typedef int boolean;

int debug = false;	/* never set, but referenced by userio.c */
PmStream *midi_in;      /* midi input */
boolean active = false;     /* set when midi_in is ready for reading */
boolean in_sysex = false;   /* we are reading a sysex message */
boolean inited = false;     /* suppress printing during command line parsing */
volatile sig_atomic_t done = 0;       /* when non zero, exit */;
boolean notes = true;       /* show notes? */
boolean controls = true;    /* show continuous controllers */
boolean bender = true;      /* record pitch bend etc.? */
boolean excldata = true;    /* record system exclusive data? */
boolean verbose = true;     /* show text representation? */
boolean realdata = true;    /* record real time messages? */
boolean clksencnt = true;   /* clock and active sense count on */
boolean chmode = true;      /* show channel mode messages */
boolean pgchanges = true;   /* show program changes */
boolean flush = false;	    /* flush all pending MIDI data */

uint32_t filter = 0;            /* remember state of midi filter */

uint32_t clockcount = 0;        /* count of clocks */
uint32_t actsensecount = 0;     /* cout of active sensing bytes */
uint32_t notescount = 0;        /* #notes since last request */
uint32_t notestotal = 0;        /* total #notes */

char val_format[] = "    Val %d\n";

/*****************************************************************************
*    Imported variables
*****************************************************************************/

extern  int     abort_flag;

/*****************************************************************************
*    Routines local to this module
*****************************************************************************/

private    void    mmexit(int code);
private    void    output(PmMessage data);
private    int     get_number(const char *prompt);

char *nanoKONTROL2_getControlName(int control);

/* read a number from console */
/**/
int get_number(const char *prompt)
{
    int n = 0, i;
    fputs(prompt, stdout);
    while (n != 1) {
        n = scanf("%d", &i);
        while (getchar() != '\n') ;
    }
    return i;
}


void receive_poll(PtTimestamp timestamp, void *userData)
{
    PmEvent event;
    int count;
    if (!active) return;
    while ((count = Pm_Read(midi_in, &event, 1))) {
        if (count == 1) output(event.message);
        else            puts(Pm_GetErrorText(count));
    }
}

void interrupt_handler(int dummy) {
    done = 1;
    printf("Caught signal %d\n", dummy);
}


/****************************************************************************
*               main
* Effect: prompts for parameters, starts monitor
****************************************************************************/

int main(int argc, char **argv)
{
    char *argument;

    PmError err;
    int i;
    int preferred = -1;
    int indexToUse;

    /* use porttime callback to empty midi queue and print */
    Pt_Start(1, receive_poll, 0);
    /* list device information */
    puts("MIDI input devices:");
    for (i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info->input) printf("%d: %s, '%s'\n", i, info->interf, info->name);
        if (strcmp(info->name, "nanoKONTROL2 nanoKONTROL2 _ CTR") == 0) {
            preferred = i;
        }
    }
    
    if (preferred != -1) {
        indexToUse = preferred;
        printf("Using preferred device: %d\n", preferred);
    } else {
        indexToUse = get_number("Type input device number: ");
    }

    signal(SIGINT, interrupt_handler);

    err = Pm_OpenInput(&midi_in, indexToUse, NULL, 512, NULL, NULL);
    if (err) {
        puts(Pm_GetErrorText(err));
        Pt_Stop();
        mmexit(1);
    }
    Pm_SetFilter(midi_in, filter);
    inited = true; /* now can document changes, set filter */
    printf("Midi Monitor ready. (Control+C to exit)\n");
    active = true;
    char c;
    while (!done) {}
    printf("Midi Monitor exiting.\n");
    active = false;
    Pm_Close(midi_in);
    Pt_Stop();
    Pm_Terminate();
    mmexit(0);
    return 0; // make the compiler happy be returning a value
}



private void mmexit(int code)
{
    /* if this is not being run from a console, maybe we should wait for
     * the user to read error messages before exiting
     */
    exit(code);
}


/****************************************************************************
*               output
* Inputs:
*    data: midi message buffer holding one command or 4 bytes of sysex msg
* Effect: format and print  midi data
****************************************************************************/

char vel_format[] = "    Vel %d\n";

private void output(PmMessage data)
{
    int command;    /* the current command */
    int chan;   /* the midi channel of the current event */
    int control; /* the controller number of the current event */
    int value;  /* the controller value of the current event */
    // int len;    /* used to get constant field width */

    /* printf("output data %8x; ", data); */

    command = Pm_MessageStatus(data) & MIDI_CODE_MASK;
    chan = Pm_MessageStatus(data) & MIDI_CHN_MASK;
    control = Pm_MessageData1(data);
    value = Pm_MessageData2(data);

    if (command == MIDI_CTRL) {
        /* controls 121 (MIDI_RESET_CONTROLLER) to 127 are channel
         * mode messages. */
        if (Pm_MessageData1(data) < MIDI_ALL_SOUND_OFF) {
            printf("%s Chan %2d Ctrl %2d Val %2d\n", nanoKONTROL2_getControlName(control), chan, control, value);
        }
    }
    fflush(stdout);
}

char* nanoKONTROL2_getControlName(int control) {
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