#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>



int switch_command(char mode[]);
int toggle_switch(char mode[]);
int api_call(char *endpoint, char *body);

void print_help() {
    printf("Usage:\n");
    printf("\tcurl_lights_test switch [on|off]\n");
    printf("\tcurl_lights_test light [on|off|high|low|warm|cool]\n");
}



int main(int argc, char *argv[]) {

    if (argc == 1) {
        printf("No arguments provided.\n");
        print_help();
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "switch") == 0) {
        if (strcmp(argv[2], "on") == 0) {
            return switch_command("on");
        } else if (strcmp(argv[2], "off") == 0) {
            return switch_command("off");
        } else {
            printf("Invalid switch mode: %s\n", argv[2]);
            print_help();
            return 1;
        }
    } else if (strcmp(argv[1], "light") == 0) {
        return toggle_switch(argv[2]);
    }else{
        printf("Invalid command: %s\n", argv[1]);
        print_help();
        return 1;
    }

}

//
// commands 
//

int switch_command(char mode[]) {

    char endpoint[16];
    sprintf(endpoint, "switch/turn_%s", mode);

    return api_call(endpoint, "{\"entity_id\": \"switch.0x282c02bfffee12e7\"}");

}

int toggle_switch(char mode[]) {

    if (strcmp(mode, "on") == 0) {
        return api_call("light/turn_on", "{\"entity_id\": \"light.0xb0ce1814001af427\", \"brightness_pct\": 42}");

    } else if (strcmp(mode, "off") == 0) {
        return api_call("light/turn_off", "{\"entity_id\": \"light.0xb0ce1814001af427\"}");

    }else if (strcmp(mode, "high") == 0) {
        return api_call("light/turn_on", "{\"entity_id\": \"light.0xb0ce1814001af427\", \"brightness_pct\": 75}");

    } else if (strcmp(mode, "low") == 0) {
        return api_call("light/turn_on", "{\"entity_id\": \"light.0xb0ce1814001af427\", \"brightness_pct\": 5}");

    } else if (strcmp(mode, "warm") == 0) {
        return api_call("light/turn_on", "{\"entity_id\": \"light.0xb0ce1814001af427\", \"kelvin\": 3000}");

    } else if (strcmp(mode, "cool") == 0) {
        return api_call("light/turn_on", "{\"entity_id\": \"light.0xb0ce1814001af427\", \"kelvin\": 5000}");

    } else {
        printf("Invalid light mode: %s\n", mode);
        return 1;
    }

}

//
// curl functions
//

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

        printf("result code: %d\n", response);

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