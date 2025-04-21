#include <stdio.h>
#include <sys/time.h>

int main() {
    struct timeval tv;
    int i;
    long long last_time = 0;

    while (i < 5) {
        gettimeofday(&tv, NULL);
        long long time_in_micros = (long long)tv.tv_sec * 1000000 + tv.tv_usec;
        if (time_in_micros - last_time > 500000) {
            printf("Time in microseconds: %lld\n", time_in_micros);
            last_time = time_in_micros;
            i++;
        }
        
    }

    return 0;
}
