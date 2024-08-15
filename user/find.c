#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find(char *dir, char *tagetFile)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(dir, 0)) < 0) {
        fprintf(2, "find: cannot open: %s\n", dir);
        return;
    }
    strcpy(buf, dir);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0) {
            printf("find: cannot stat: %s\n", buf);
            continue;
        }
        switch(st.type) {
        case T_FILE:
            if(strcmp(de.name, tagetFile) == 0) {
                printf("%s\n", buf);
            }
            break;
        case T_DIR:
            find(buf, tagetFile);
            break;
        }
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    if(argc != 3) {
        fprintf(2, "Usage: find path tagetFile\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}