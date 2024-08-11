#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXLINE 256

int 
main()
{
    int p[2];
    int n;
    pipe(p);
    char buf[MAXLINE];
    if(fork() == 0) {
        n = read(p[0], buf, MAXLINE);
        buf[n] = '\0';
        close(p[0]);
        printf("%d: received %s\n", getpid(), buf);
        write(p[1], "pong", 4);
        close(p[1]);
    } else {
        write(p[1], "ping", 4);
        close(p[1]);
        wait(0);
        n = read(p[0], buf, MAXLINE);
        buf[n] = '\0';
        close(p[0]);
        printf("%d: received %s\n", getpid(), buf);
    }
    exit(0);
}

