#define _XOPEN_SOURCE 500
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(const char* name)
{
    fprintf(stderr, "USAGE: %s command input\n", name);
    exit(EXIT_FAILURE);
}
int histogram[256];
int histogram2[256];

int qsort_comparison_function(const void* left, const void* right)
{
    //return 0;
    return (int*)left > (int*)right;
}




void analyze_file(const char* path){
    FILE* s1;
    for(int i =0; i<256; i++){
        histogram[i]=0;
    }
    if ((s1 = fopen(path, "r")) == NULL){
        ERR("fopen");
    }
    int kodASCII;
    int count=0;
    while((kodASCII=fgetc(s1))!=EOF){
        histogram[kodASCII]++;
    }
    for(int i =0; i<256; i++){
        if(histogram[i]==0){
            continue;
        }
        else{
            count++;
            printf("%c %d\n", (char)i, histogram[i]);
        }
    }
    if (fclose(s1))
        ERR("fclose");
    FILE* s2;
    if ((s2 = fopen("output/analyze.bin", "wb")) == NULL){
        ERR("fopen");
    }
    fwrite(&count, 4,1,s2);

    for(int i =0; i<256; i++){
        if(histogram[i]==0){
            continue;
        }
        else{
            char t = (char)i;
            fwrite(&t,sizeof(t),1,s2);
        }
    }
    for(int i =0; i<256; i++){
        if(histogram[i]==0){
            continue;
        }
        else{
            fwrite(&histogram[i],sizeof(histogram[i]),1,s2);
        }
    }



    if (fclose(s2))
        ERR("fclose");
    return;
}
void decode_message(const char* path){
    const int fd_1 = open(path, O_RDONLY);
    int buf0;
    struct iovec iov[1];
    iov[0].iov_base=&buf0;
    iov[0].iov_len=sizeof(buf0);
    ssize_t len1 = readv(fd_1, iov, sizeof(iov)/sizeof(struct iovec));

    if(len1<=0) ERR("readv");

    //char buf1[buf0];
    char *buf1 = (char *)malloc(sizeof(char) * buf0);
    printf("buffer line 98 %d\n", buf0);

    struct iovec iov1[1];
    iov1[0].iov_base=buf1;
    iov1[0].iov_len=sizeof(char)*buf0;
    ssize_t len2 = readv(fd_1, iov1, sizeof(iov1)/sizeof(struct iovec));

        if(len2<=0) ERR("readv");

    //int buf2[buf0];
    int *buf2 = (int *)malloc(sizeof(int) * buf0);

    struct iovec iov2[1];
    iov2[0].iov_base=buf2;
    iov2[0].iov_len=sizeof(int)*buf0;
    ssize_t len3 = readv(fd_1, iov2, sizeof(iov2)/sizeof(struct iovec));

        if(len3<=0) ERR("readv");


    for(int i =0; i<256; i++){
        histogram2[i]=0;
    }

    for(int i=0; i<buf0; i++){
        //printf("%c\n", buf1[i]);
        histogram2[(int)buf1[i]]=buf2[i];
    }
     for(int i =0; i<256; i++){
        if(histogram2[i]==0){
            continue;
        }
        else{
            printf("%c %d\n", (char)i, histogram2[i]);
        }
    }


        char *buf3 = (char *)malloc(sizeof(char) * buf0);
        for(int i =0; i<buf0; i++){
            buf3[i] = buf1[i];
        }

        qsort(buf3, buf0, sizeof(char), qsort_comparison_function);

        for(int i = 0; i<buf0;i++){
            printf("%c",buf3[i]);
        }


    free(buf1);
    free(buf2);
    free(buf3);

    return;
}
void batch_decode(){
        printf("batch\n");
    return;
}

int main(int argc, char** argv){

    if(argc!=3){
        usage(argv[0]);
    }

    const char* path = argv[2];
    char* command = argv[1];
    struct stat filestat;

    if(strcmp(command, "analyze")==0){
        
        if (stat(path, &filestat)){
            ERR("filestat");
        }
        if(S_ISREG(filestat.st_mode)){
            analyze_file(path);
        }
        else{
            perror("BAD ANALYZE");
        }
    }
    else if(strcmp(command, "decode")==0){
        if(lstat(path, &filestat)){
            ERR("lstat");
        }
        if(S_ISREG(filestat.st_mode)){
            decode_message(path);
        }
        else{
            perror("BAD DECODE");
        }
    }
    else if(strcmp(command, "batch")==0){
        if(lstat(path, &filestat)){
            ERR("lstat");
        }
        if(S_ISDIR(filestat.st_mode)){
            batch_decode();
        }
        else{
            perror("BAD BATCH");
        }
    }
    else{
        perror("BAD COMMAND");
    }


    printf("\n\n");



    analyze_file("./data/surname");
    decode_message("./data/surname");






    return EXIT_SUCCESS;
}