/* cat.c
 *	../test/cat file ...
 */
#include "syscall.h"

int main(int argc, char *argv[]) {
    char buffer[128];
    OpenFileId file;
    int i, n;

    for (i = 1; i < argc; i++) {
        file = Open(argv[i]);
        if (file < 0) {
            Write("cat: cannot open file\n", 22, ConsoleError);
            Exit(1);
        }
        while ((n = Read(buffer, 128, file)) > 0)
            Write(buffer, n, ConsoleOutput);
        Close(file);
    }
    Exit(0);
}
