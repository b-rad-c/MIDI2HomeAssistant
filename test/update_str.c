#include <stdio.h>
#include <string.h>

char str1[30] = "initial val";

int main() {
    printf("%s\n", str1);
    
    strcpy(str1, "a longer string value");
    printf("%s\n", str1);

    strcpy(str1, "short");
    printf("%s\n", str1);

    return 0;
}
