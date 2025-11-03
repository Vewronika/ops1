#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_PATH 101

void usage(char *pname)
{
    fprintf(stderr, "USAGE:%s -n Name -p OCTAL -s SIZE\n", pname);
    exit(EXIT_FAILURE);
}

ssize_t scan_dir(){
    DIR* dirp;
    struct dirent *dp;
    struct stat filestat;
    ssize_t result=0;
    if((dirp = opendir("."))==NULL){
        fprintf(stderr, "No access to folder \n");
        return 0;
    }
    do
    {
        errno = 0;

        if ((dp = readdir(dirp)) != NULL)
        {
            if (lstat(dp->d_name, &filestat)){
                ERR("lstat");
            }
            if((strcmp(dp->d_name, ".") == 0 )|| (strcmp(dp->d_name, "..")==0)){
                continue;
            }
            else{
                result+=filestat.st_size;
            }
            
            
        }
    } while (dp != NULL);

    if (errno != 0)
        ERR("readdir");
    if (closedir(dirp))
        ERR("closedir");
    return result;

}


int main(int argc, char **argv){
    if(argc%2!=1){
        usage(argv[0]);
    }

    char path_cwd[MAX_PATH];
    if (getcwd(path_cwd, MAX_PATH) == NULL)
        ERR("getcwd");
    FILE* file;
    if((file=fopen("out.txt", "a+"))==NULL){
        ERR("fopen");
    }
    for(int i =1; i<argc; i+=2){
    const char* path = argv[i];
    ssize_t size = strtol(argv[i+1], (char **)NULL, 10);
    if(size<0){
        printf("BAD INPUT");
    }

    ssize_t size_dir = -1;


    
    if(chdir(path)){
        ERR("chdir");
    }
    size_dir = scan_dir();
    if(size_dir >= size){
        fprintf(file, "%s\n", path);
        //printf()
    }
    if(chdir(path_cwd)){
        ERR("chdir");
    }
    }



    if (fclose(file)){
        ERR("fclose");
    }
    return EXIT_SUCCESS;

}
