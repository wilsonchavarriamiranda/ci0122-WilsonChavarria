/* cp.c
 *	../test/cp source destination
 */
#include "syscall.h"

int main(int argc, char *argv[]) {
    char buffer[128];
    OpenFileId input, output;
    int n;

    if (argc != 3) {
        Write("usage: cp source destination\n", 29, ConsoleError);
        Exit(1);
    }
    input = Open(argv[1]);
    if (input < 0) {
        Write("cp: cannot open source\n", 23, ConsoleError);
        Exit(1);
    }
    Create(argv[2]);
    output = Open(argv[2]);
    if (output < 0) {
        Write("cp: cannot create destination\n", 30, ConsoleError);
        Exit(1);
    }
    while ((n = Read(buffer, 128, input)) > 0)
        Write(buffer, n, output);
    Close(input);
    Close(output);
    Exit(0);
}
