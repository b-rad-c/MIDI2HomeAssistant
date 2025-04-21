#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int opt;
    char *device = "default_device";
    int throttle = 100000;
    char *command = "";

    while ((opt = getopt(argc, argv, "d:t:")) != -1) {
        switch (opt) {
            case 'd':
                device = optarg;
                break;
            case 't':
                throttle = atoi(optarg);
                break;
            case '?':
                fprintf(stderr, "Usage: %s -d device -a age [run|list]\n", argv[0]);
                return 1;
        }
    }

    if (optind < argc) {
        command = argv[optind];
    }else {
        command = "not set";
    }

    printf("Device: %s\n", device);
    printf("Throttle: %d\n", throttle);
    printf("Command: %s\n", command);
    return 0;
}
