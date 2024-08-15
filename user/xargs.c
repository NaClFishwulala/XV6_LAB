#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAXLINE 1024

int
main(int argc, char *argv[])
{
    char *newArgv[MAXARG];
    char buf[MAXLINE];
    int bufIndex = 0;
    char c;
    if(argc < 2) {
        fprintf(2, "Usage: xargs <comands> ...\n");
    }

    for(int i = 1; i < argc; ++i) {
        newArgv[i - 1] = argv[i];
    }
    while(read(0, &c, 1) > 0) {
        if(c == '\n') {
            buf[bufIndex] = 0;
            newArgv[argc - 1] = buf;
            newArgv[argc] = 0;
            if(fork() == 0) {
                exec(newArgv[0], newArgv);
            } else {
                wait(0);
                bufIndex = 0;
            }
        } else {
            buf[bufIndex++] = c;
        }
    }
    exit(0);
}