#include <stdio.h>


int main(int argc, char *argv[]) {

    int i;
    int kelvin;
    float percent;

    for (i = 0; i < 128; i++) {

        percent = (float)i / 127.0f;

        // kelvin min 2000, max 6493
        kelvin = (int)(2000 + (percent * (6493 - 2000)));

        printf("%d -> %f\t%d\n", i, percent, kelvin);

    }

    
    return 0;
}