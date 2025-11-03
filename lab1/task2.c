#define _XOPEN_SOURCE 500

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define MAXFD 20
#define MAX_PATH 1024

#define ERR(source) (perror(source), printf("Err"), exit(EXIT_FAILURE))
void usage(char *pname)
{
    fprintf(stderr, "USAGE:%s -n Name -p OCTAL -s SIZE\n", pname);
    exit(EXIT_FAILURE);
}



void scan_dir(const char *path, int depth, const char *extension)
{
    DIR* dirp;
    struct dirent *dp;
    struct stat filestat;

    if((dirp = opendir(".")) == NULL){
        ERR("opendir");
    }
    do
    {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL)
        {
            if (lstat(dp->d_name, &filestat))
                ERR("lstat");
            if (S_ISDIR(filestat.st_mode)){

                if(depth>1 && strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")){
                    char path_cwd[MAX_PATH];
                    if (getcwd(path_cwd, MAX_PATH) == NULL){ERR("getcwd");}
                    if (chdir(dp->d_name)){ERR("chdir, 1");}
                    scan_dir(dp->d_name, depth - 1, extension);
                    if(chdir(path_cwd)){ERR("chdir, 2");}
                }
                
            }
                
            else if (S_ISREG(filestat.st_mode)){
                if(extension !=NULL){
                    int len = strlen(extension);
                    if(strcmp(extension, dp->d_name + strlen(dp->d_name) - len) == 0){
                        printf("%s, %d, Comparing: %s and %s\n", dp->d_name, filestat.st_size, extension, dp->d_name + strlen(dp->d_name) - len);
                    }
                }
                else{
                    printf("%s, %d\n", dp->d_name, filestat.st_size);
                }
            }
            else if (S_ISLNK(filestat.st_mode)){continue;}
                
        }
    } while (dp != NULL);
    if (errno != 0)
        ERR("readdir");
    if (closedir(dirp))
        ERR("closedir");
}

int main(int argc, char** argv){
    if(argc < 2){
        usage(argv[0]);
    }

    const char* path = NULL;
    int c;
    int depth = -1;
    const char* extent = NULL;

    while((c=getopt(argc, argv, "p:e:d:"))!=-1){
        switch(c){
            case 'p':
                path = optarg;
                printf("Doshlo, %s", optarg);
                break;
            case 'e':
                extent = optarg;
                printf("Doshlo, %s", optarg);
                break;
            case 'd':
                            printf("Doshlo, %s", optarg);
                depth = atoi(optarg);
                break;
            
            case '?':
            default:
                usage(argv[0]);

        }
    }

    if(path == NULL || depth<=0){
        usage(argv[0]);
    }
    printf("Extension: %s\n", extent);
    if (chdir(path)){ERR("chdir");}
    scan_dir(path, depth, extent);
 
    
    return EXIT_SUCCESS;
}