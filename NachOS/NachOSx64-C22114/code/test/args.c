/* args.c
 *	Prints its arguments:  ../test/args uno dos tres
 */
#include "syscall.h"

int strlen(char *s) {
    int n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

void PrintInt(int n) {
    char digits[12];
    int i = 11;

    digits[i] = '\0';
    if (n == 0)
        digits[--i] = '0';
    while (n > 0) {
        digits[--i] = '0' + n % 10;
        n /= 10;
    }
    Write(&digits[i], 11 - i, ConsoleOutput);
}

int main(int argc, char *argv[]) {
    int i;

    Write("argc = ", 7, ConsoleOutput);
    PrintInt(argc);
    Write("\n", 1, ConsoleOutput);
    for (i = 0; i < argc; i++) {
        Write("argv[", 5, ConsoleOutput);
        PrintInt(i);
        Write("] = ", 4, ConsoleOutput);
        Write(argv[i], strlen(argv[i]), ConsoleOutput);
        Write("\n", 1, ConsoleOutput);
    }
    Exit(argc);
}
