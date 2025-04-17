#include <stdio.h>
#include <stdbool.h>
#include <string.h>

char char1 = 'A';
char char2 = 66;
bool bool1 = 1;
char string[] = "an example string";

enum Level {
    LOW,
    MEDIUM,
    HIGH
};

enum Level2 {
    LOW2 = 25,
    MEDIUM2 = 50,
    HIGH2 = 75
};

struct myStructure {
    int myNum;
    char myLetter;
    char myString[30];
};

int testFunc(int a) {
    int result = a + 1;
    return result;
}

int main(int argc, char *argv[]) {
    printf("Hello World!\n");

    // args
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("\targv[%d]: %s\n", i, argv[i]);
    }
    printf("\n");

    // variables
    printf("char1: %c\n", char1);
    printf("char2: %c\n", char2);
    printf("bool1: %d\n", bool1);
    printf("string: %s\n\n", string);

    // enum
    enum Level level = MEDIUM;
    printf("Level: %d\n", level);
    enum Level2 level2 = HIGH2;
    printf("Level2: %d\n\n", level2);

    // struct
    struct myStructure s1 = {13, 'B', "Some text"};
    printf("%d %c %s\n", s1.myNum, s1.myLetter, s1.myString);
    s1.myNum = 30;
    s1.myLetter = 'C';
    strcpy(s1.myString, "Something else");
    printf("%d %c %s\n\n", s1.myNum, s1.myLetter, s1.myString);

    // function
    printf("testFunc(5): %d\n", testFunc(5));
    printf("testFunc(10): %d\n\n", testFunc(10));

    return 0;
}