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
#include <sys/time.h>


#define MIDI_CODE_MASK  0xf0
#define MIDI_CHN_MASK   0x0f

#define private static

#ifndef false
#define false 0
#define true 1
#endif

typedef int boolean;
extern  int     abort_flag;

struct kontrol2_control {
    char *name;
    int channel;
};

/*
global variables
*/

PmStream *midi_in;
int debug = false;	                    /* never set, but referenced by userio.c */
boolean active = false;                 /* set when midi_in is ready for reading */
boolean shift = false;                  /* set when shift button is pressed */
volatile sig_atomic_t done = 0;         /* when non zero, exit */;

struct timeval current_timeval;
long long last_api_call = 0;

int throttle = 100000;
char *device_name = "nanoKONTROL2 nanoKONTROL2 _ CTR";

char queued_api_endpoint[100] = "";
char queued_api_body[100] = "";

/*
local functions
*/

private void handle_midi_event(PmMessage data);
char *channel_to_entity_id(int channel, boolean shift);
struct kontrol2_control get_nano_kontrol2_control(int control);
int api_call(char *endpoint, char *body);


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
    puts("Usage: mm -dt [run|list] ");
    puts("Commands:");
    puts("  run                     Start the MIDI monitor.");
    puts("  list                    List available MIDI devices.");
    puts("  help                    Show this help message.");
    puts("Options:");
    printf("  -d <device_name>        Specify the MIDI device name to use. Default: '%s'\n", device_name);
    printf("  -t <throttle>           Set the throttle for API calls in microseconds. Default: %d\n", throttle);
    exit(exit_code);
}

/*
main
*/

int main(int argc, char **argv) {

    /*
    parse cli arguments
    */

    int opt;
    char *command;

    while ((opt = getopt(argc, argv, "d:t:")) != -1) {
        switch (opt) {
            case 'd':
                device_name = optarg;
                break;
            case 't':
                throttle = atoi(optarg);
                break;
            case '?':
                help_menu(1);
                return 1;
        }
    }

    if (optind < argc) {
        command = argv[optind];
    }else {
        printf("No command provided.\n");
        help_menu(1);
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        help_menu(0);
    }

    /* 
    select / list input device 
    */

    int device_index = -1;
    
    Pt_Start(1, poll_midi_device, 0);
    if (strcmp(command, "list") == 0) puts("MIDI input devices:");

    int i;
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

    PmError err;
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

    while (!done) {

        /*
        
        this is the main loop of the program,
        it checks for queued messages and sends them to the api
        if the throttle time has passed since the last api call.
        this throttle prevents the server from being overloaded with requests,
        but ensures the last value input from the user is sent to the api.
        
        */

        gettimeofday(&current_timeval, NULL);
        long long time_in_micros = (long long)current_timeval.tv_sec * 1000000 + current_timeval.tv_usec;

        if (time_in_micros - last_api_call > throttle) {
            if (strlen(queued_api_endpoint) > 0) {
                char *endpoint = malloc(strlen(queued_api_endpoint) + 1);
                char *body = malloc(strlen(queued_api_body) + 1);

                strcpy(endpoint, queued_api_endpoint);
                strcpy(body, queued_api_body);
                
                strcpy(queued_api_endpoint, "");
                strcpy(queued_api_body, "");

                api_call(endpoint, body);
                free(endpoint);
                free(body);
            }
        }

    }

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

    /*
    this function handles incoming midi events,
    it maps the message to an api call and queues it
    by copying the endpoint and body to a global variable
    */

    int midi_command;
    int midi_channel;
    int midi_control;
    int midi_value;

    midi_command = Pm_MessageStatus(data) & MIDI_CODE_MASK;
    midi_channel = Pm_MessageStatus(data) & MIDI_CHN_MASK;
    midi_control = Pm_MessageData1(data);
    midi_value = Pm_MessageData2(data);

    struct kontrol2_control control = get_nano_kontrol2_control(midi_control);

    float percent = (float)midi_value / 127.0f;

    if (strcmp(control.name, "fader") == 0) {
        int int_percent = (int)(percent * 100);
        char *entity_id = channel_to_entity_id(control.channel, shift);

        strcpy(queued_api_endpoint, "light/turn_on");
        sprintf(queued_api_body, "{\"entity_id\": \"%s\", \"brightness_pct\": %d}", entity_id, int_percent);

    } else if (strcmp(control.name, "pot") == 0) {
        int kelvin = (int)(2000 + (percent * (6493 - 2000)));
        char *entity_id = channel_to_entity_id(control.channel, shift);
        
        strcpy(queued_api_endpoint, "light/turn_on");
        sprintf(queued_api_body, "{\"entity_id\": \"%s\", \"kelvin\": %d}", entity_id, kelvin);

    } else if (strcmp(control.name, "play") == 0) {
        if(midi_value == 127) {
            // printf("%s (%2d) - press\n", control.name, control.channel);
        }else{
            strcpy(queued_api_endpoint, "switch/toggle");
            strcpy(queued_api_body, "{\"entity_id\": \"switch.0x282c02bfffee12e7\"}");
        }
    } else if (strcmp(control.name, "mute") == 0) {
        if(midi_value == 127) {
            // printf("%s (%2d) - press\n", control.name, control.channel);
        }else{
            char *entity_id = channel_to_entity_id(control.channel, shift);
            strcpy(queued_api_endpoint, "light/turn_off");
            sprintf(queued_api_body, "{\"entity_id\": \"%s\"}", entity_id);
        }
    } else if (strcmp(control.name, "cycle") == 0) {
        if(midi_value == 127) {
            shift = true;
        }else{
            shift = false;
        }
    } else {
        if(midi_value == 127) {
            // printf("%s (%2d) - press\n", control.name, control.channel);
        }else{
            // printf("%s (%2d) - release\n", control.name, control.channel);
        }
    }

    fflush(stdout);
}

char *channel_to_entity_id(int channel, boolean shift) {

    if (shift) {
        switch (channel) {
            case 1: return "light.0xb0ce18140015fb0c";
            case 2: return "light.0xb0ce1814001b1ee1";
            default: return "";
        }
    }else {
        switch (channel) {
            case 1: return "light.0xb0ce1814001610b3";
            case 2: return "light.0xb0ce181400163588";
            case 3: return "light.0xb0ce1814001b08fb";
            case 4: return "light.0xb0ce18140017bf5e";
            case 5: return "light.0xb0ce1814001af553";
            case 6: return "light.0xb0ce1814001af427";
            case 7: return "light.0xb0ce1814001af6f2";
            case 8: return "light.0xb0ce181400160048";
            default: return "";
        }
    }
    
}


struct kontrol2_control get_nano_kontrol2_control(int control) {
    struct kontrol2_control c;
    switch (control) {

        /* 
        faders
        */

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

        /*
        pots
        */

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

        /*
        solo buttons
        */

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

        /*
        play controls
        */
        
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

        /*
        mute buttons
        */

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

        /*
        track and markers
        */

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

        /*
        record buttons
        */

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