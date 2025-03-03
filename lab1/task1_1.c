#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <linux/types.h> 
#include <sys/types.h>  // For ino_t and off_t
#include <stdint.h>     // For standard integer types
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAX_PATH 101
#define BUF_SIZE 1024

//typedef unsigned long ino_t;
//typedef long off_t;


struct linux_dirent64{
    ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};


void usage(char *pname)
{
    fprintf(stderr, "USAGE:%s _path _size\n", pname);
    exit(EXIT_FAILURE);
}


void scan_dir(long *tsize){
    DIR* d;
    struct dirent *dp;
    struct stat filestat;

    if((d = opendir(".")) == NULL) ERR("opendir");
    long total = 0;

    do{
        errno = 0;

        if((dp = readdir(d)) != NULL){
            if(lstat(dp->d_name, &filestat)){
            fprintf(stderr, "error lstat: %s\n", dp->d_name);
            continue;
        }


            //printf("%s: %ld\n", dp->d_name, filestat.st_size);
            total += filestat.st_size;
        }
    } while(dp != NULL);

    //printf("\ntotal size: %ld\n", total);
    *tsize = total;
    //printf("%ld\n", *tsize);

    if(errno != 0) ERR("readdir");

    if(closedir(d)) ERR("closedir");


}




void scan_dir_low(long *tsize){
    int fd;
    char buf[BUF_SIZE];
    struct linux_dirent64 *d;
    struct stat filestat;
    long total = 0;

    char path[MAX_PATH];
    if (getcwd(path, MAX_PATH) == NULL)
        ERR("getcwd");

    if((fd = open(".", O_RDONLY | O_DIRECTORY)) < 0){
        fprintf(stderr, "No access folder %s\n", path);
        return;
    }

    if(fd == -1) ERR("open");

    int nread, bpos;
    for(;;){
        nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);

        if(nread == -1) ERR("nread");
        if(nread == 0) break;

        for(bpos = 0; bpos < nread;){
            d = (struct linux_dirent64*) (buf + bpos);
            bpos += d->d_reclen;

            char fullpath[MAX_PATH];
            snprintf(fullpath, MAX_PATH, "./%s", d->d_name);

            if(lstat(fullpath, &filestat) == -1) {
                fprintf(stderr, "Error lstat: %s\n", fullpath);
                continue;
            }

            total += filestat.st_size;
        }
    }

    if(nread == -1) ERR("nread");

    if(close(fd) == -1) ERR("close");

    *tsize = total;
}

ssize_t bulk_write(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}




int main(int argc, char** argv){
    char path[MAX_PATH];
    if(getcwd(path, MAX_PATH) == NULL) ERR("getcwd");

    if(argc < 3 || (argc-1)%2 != 0) usage(argv[0]);


    const int fd = open("out.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if(fd == -1) ERR("open");

    for(int i = 1; i < argc; i+=2){

        char* cp = argv[i];
        size_t size = strtol(argv[i+1], (char**)NULL, 10);
        long tsize;

        if(chdir(cp)) ERR("chdir");

        int len = strlen(cp);

        //printf("%s:\n\n", cp);
        scan_dir_low(&tsize);
        //if(tsize > size) printf("%s\n", cp);

        if(tsize > size) {
            char newline = '\n';
            if(bulk_write(fd, cp, len) == -1) ERR("bulk_write");
            if (bulk_write(fd, &newline, 1) == -1) ERR("bulk_write");
        }       


        if(chdir(path)) ERR("chdir");
    }

    if(close(fd) == -1) ERR("close");

    return EXIT_SUCCESS;
}