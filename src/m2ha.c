/* mm.c -- midi monitor */

#include "stdlib.h"
#include "ctype.h"
#include "string.h"
#include "stdio.h"
#include "porttime.h"
#include "portmidi.h"
#include "signal.h"
#include <unistd.h>
#include <curl/curl.h>


#define MIDI_CODE_MASK  0xf0
#define MIDI_CHN_MASK   0x0f

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

struct kontrol2_control {
    char *name;
    int channel;
};

struct kontrol2_control get_nano_kontrol2_control(int control);

int api_call(char *endpoint, char *body);

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

    int midi_command;    /* the current command */
    int midi_channel;   /* the midi channel of the current event */
    int midi_control; /* the controller number of the current event */
    int midi_value;  /* the controller value of the current event */

    midi_command = Pm_MessageStatus(data) & MIDI_CODE_MASK;
    midi_channel = Pm_MessageStatus(data) & MIDI_CHN_MASK;
    midi_control = Pm_MessageData1(data);
    midi_value = Pm_MessageData2(data);

    struct kontrol2_control control = get_nano_kontrol2_control(midi_control);

    float percent = (float)midi_value / 127.0f;
    char body[100];

    if (strcmp(control.name, "fader") == 0) {
        int int_percent = (int)(percent * 100);
        // printf("Fader %d: %d\n", control.channel, int_percent);
        sprintf(body, "{\"entity_id\": \"light.0xb0ce1814001af427\", \"brightness_pct\": %d}", int_percent);
        api_call("light/turn_on", body);

    } else if (strcmp(control.name, "pot") == 0) {
        int kelvin = (int)(2000 + (percent * (6493 - 2000)));
        // printf("Pot %d: %d\n", control.channel, kelvin);

        sprintf(body, "{\"entity_id\": \"light.0xb0ce1814001af427\", \"kelvin\": %d}", kelvin);
        api_call("light/turn_on", body);

    } else {
        if(midi_value == 127) {
            printf("%s (%2d) - press\n", control.name, control.channel);
        }else{
            printf("toggling switch\n");
            api_call("switch/toggle", "{\"entity_id\": \"switch.0x282c02bfffee12e7\"}");
        }
        
    }

    fflush(stdout);
}

struct kontrol2_control get_nano_kontrol2_control(int control) {
    struct kontrol2_control c;
    switch (control) {
        case 0: 
            c.name = "fader";
            c.channel = 1;
            break;
        case 1:
            c.name = "fader";
            c.channel = 2;
            break;
        case 2:
            c.name = "fader";
            c.channel = 3;
            break;
        case 3:
            c.name = "fader";
            c.channel = 4;
            break;
        case 4:
            c.name = "fader";
            c.channel = 5;
            break;
        case 5:
            c.name = "fader";
            c.channel = 6;
            break;
        case 6:
            c.name = "fader";
            c.channel = 7;
            break;
        case 7:
            c.name = "fader";
            c.channel = 8;
            break;

        case 16:
            c.name = "pot";
            c.channel = 1;
            break;
        case 17:
            c.name = "pot";
            c.channel = 2;
            break;
        case 18:
            c.name = "pot";
            c.channel = 3;
            break;
        case 19:
            c.name = "pot";
            c.channel = 4;
            break;
        case 20:
            c.name = "pot";
            c.channel = 5;
            break;
        case 21:
            c.name = "pot";
            c.channel = 6;
            break;
        case 22:
            c.name = "pot";
            c.channel = 7;
            break;
        case 23:
            c.name = "pot";
            c.channel = 8;
            break;


        case 32:
            c.name = "solo";
            c.channel = 1;
            break;
        case 33:
            c.name = "solo";
            c.channel = 2;
            break;
        case 34:
            c.name = "solo";
            c.channel = 3;
            break;
        case 35:
            c.name = "solo";
            c.channel = 4;
            break;
        case 36:
            c.name = "solo";
            c.channel = 5;
            break;
        case 37:
            c.name = "solo";
            c.channel = 6;
            break;
        case 38:
            c.name = "solo";
            c.channel = 7;
            break;
        case 39:
            c.name = "solo";
            c.channel = 8;
            break;
        
        case 41:
            c.name = "play";
            c.channel = -1;
            break;
        case 42:
            c.name = "stop";
            c.channel = -1;
            break;
        case 43:
            c.name = "rewind";
            c.channel = -1;
            break;
        case 44:
            c.name = "fast-forward";
            c.channel = -1;
            break;
        case 45:
            c.name = "record";
            c.channel = -1;
            break;

        case 46:
            c.name = "cycle";
            c.channel = -1;
            break;

        case 48:
            c.name = "mute";
            c.channel = 1;
            break;
        case 49:
            c.name = "mute";
            c.channel = 2;
            break;
        case 50:
            c.name = "mute";
            c.channel = 3;
            break;
        case 51:
            c.name = "mute";
            c.channel = 4;
            break;
        case 52:
            c.name = "mute";
            c.channel = 5;
            break;
        case 53:
            c.name = "mute";
            c.channel = 6;
            break;
        case 54:
            c.name = "mute";
            c.channel = 7;
            break;
        case 55:
            c.name = "mute";
            c.channel = 8;
            break;

        case 58:
            c.name = "track-left";
            c.channel = -1;
            break;
        case 59:
            c.name = "track-right";
            c.channel = -1;
            break;

        case 60:
            c.name = "marker-set";
            c.channel = -1;
            break;
        case 61:
            c.name = "marker-left";
            c.channel = -1;
            break;
        case 62:
            c.name = "marker-right";
            c.channel = -1;
            break;

        case 64:
            c.name = "record";
            c.channel = 1;
            break;
        case 65:
            c.name = "record";
            c.channel = 2;
            break;
        case 66:
            c.name = "record";
            c.channel = 3;
            break;
        case 67:
            c.name = "record";
            c.channel = 4;
            break;
        case 68:
            c.name = "record";
            c.channel = 5;
            break;
        case 69:
            c.name = "record";
            c.channel = 6;
            break;
        case 70:
            c.name = "record";
            c.channel = 7;
            break;
        case 71:
            c.name = "record";
            c.channel = 8;
            break;

        default: 
            c.name = "unknown";
            c.channel = -1;
            break;
    }
    return c;
}



size_t write_null(void *buffer, size_t size, size_t nmemb, void *userp) {
    return size * nmemb;
}

int api_call(char *endpoint, char *body) {

    CURL *curl;
    CURLcode response;
    curl = curl_easy_init();
    if(curl) {

        /* disable printing */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_null);

        /* headers */
        struct curl_slist *headers = NULL;
        char *token = getenv("TOKEN");
        if (token == NULL) {
            fprintf(stderr, "TOKEN environment variable not set\n");
            return 1;
        }
        char *auth_header = malloc(strlen("Authorization: Bearer ") + strlen(token) + 1);
        sprintf(auth_header, "Authorization: Bearer %s", token);
        headers = curl_slist_append(headers, auth_header);
        free(auth_header);
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        /* post body */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

        /* url */
        char url[100];
        sprintf(url, "http://homeassistant.local:8123/api/services/%s", endpoint);
        curl_easy_setopt(curl, CURLOPT_URL, url);

        /* Perform the request, res will get the return code */
        response = curl_easy_perform(curl);

        // printf("result code: %d\n", response);

        /* check for errors */
        if(response != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(response));
            return 1;
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
        return 0;
    }
    fprintf(stderr, "Failed to initialize curl\n");
    return 1;
}