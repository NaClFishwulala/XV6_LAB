#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void 
SubProcess(int *p)
{
    int prime, n;
    int subPipe[2];
    pipe(subPipe);
    if(read(p[0], &prime, sizeof(prime))) {
        printf("prime %d\n", prime);
    } else {
        close(p[0]);
        exit(0);
    }
    if(fork() > 0) {
        close(subPipe[0]);
        while(read(p[0], &n, sizeof(n))) {
            if(n % prime == 0) {
                continue;
            } else {
                write(subPipe[1], &n, sizeof(n));
            }
        }
        close(p[0]);
        close(subPipe[1]);
        wait(0);
    } else {
        close(p[0]);
        close(subPipe[1]);
        SubProcess(subPipe);
    }
    exit(0);
}

int
main()
{
    int p[2];
    pipe(p);
    if(fork() > 0) {
        close(p[0]);
        for(int i = 2; i <= 35; ++i) {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);
        wait(0);
    } else {
        close(p[1]);
        SubProcess(p);
    }
    exit(0);
}