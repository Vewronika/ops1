#define _XOPEN_SOURCE 700

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

#define MAX_LENGTH 1024

// join 2 path. returned pointer is for newly allocated memory and must be freed
char* join_paths(const char* path1, const char* path2)
{
    char* res;
    const int l1 = strlen(path1);
    if (path1[l1 - 1] == '/')
    {
        res = malloc(strlen(path1) + strlen(path2) + 1);
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
    }
    else
    {
        res = malloc(strlen(path1) + strlen(path2) + 2);  // additional space for "/"
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
        res[l1] = '/';
        res[l1 + 1] = 0;
    }

    return strcat(res, path2);
}

void usage(int argc, char** argv)
{
    (void)argc;
    fprintf(stderr, "USAGE: %s path\n", argv[0]);
    exit(EXIT_FAILURE);
}

void scan_file(const char* path){
    FILE* s1;
    if ((s1 = fopen(path, "r")) == NULL){
        ERR("fopen");
    }

    char* author_value;
    char* title_value;
    char* genre_value;
    
    char str_f[MAX_LENGTH];
    while(fgets( str_f, MAX_LENGTH, s1)!=NULL){
        const char* temp = str_f;
        char* delim = strchr(temp,':');
        
        char* temp2 = delim+1;

        *delim = '\0';
        if(strcmp(temp, "author")==0){
            author_value = strdup(temp2);
        }
        else if(strcmp(temp, "title")==0){
             title_value = strdup(temp2);
        }
        else if(strcmp(temp, "genre")==0){
            genre_value = strdup(temp2);
        }
    }
    if(!genre_value){
        genre_value="missing";
    }
    if(!author_value){
        author_value="missing";
    }
     if(!title_value){
        title_value="missing";
    }

    printf("author: %s", author_value);
    printf("title: %s", title_value);
    printf("genre: %s", genre_value);


    free(author_value);
    free(title_value);
    free(genre_value);



    if (fclose(s1))
    ERR("fclose");

        
}

void scan_rec(const char* path_s){
    DIR* dirp;
    struct dirent *dp;
    struct stat filestat;
    
    if ((dirp = opendir(".")) == NULL)
        ERR("opendir");
    do
    {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL)
        {
            if (lstat(dp->d_name, &filestat))
                ERR("lstat");
            if (S_ISDIR(filestat.st_mode)&&(strcmp(dp->d_name, "."))&&(strcmp(dp->d_name, ".."))){
                char cwd[MAX_LENGTH];
                if (getcwd(cwd, MAX_LENGTH) == NULL)
                    ERR("getcwd");
                chdir(dp->d_name);
                scan_rec(path_s);
                chdir(cwd);
            }
                
            else if (S_ISREG(filestat.st_mode)){
                char cwd[MAX_LENGTH];
                if (getcwd(cwd, MAX_LENGTH) == NULL)
                    ERR("getcwd");
                char* file_path = join_paths(cwd, dp->d_name);
                chdir(path_s);
                symlink(file_path, dp->d_name);
                chdir(cwd);
                free(file_path);
            }
                

        }
    } while (dp != NULL);
    if (closedir(dirp))
        ERR("closedir");

}

void scan_dir(const char* path_s){

}


int main(int argc, char** argv) { 
    if(argc != 2){
        usage(argc, argv); 
    }

    const char* path = argv[1];
    scan_file(path);

    char cwd[MAX_LENGTH];
    if (getcwd(cwd, MAX_LENGTH) == NULL)
        ERR("getcwd");
    const char* cwd_2=cwd;
    char* ind = join_paths(cwd_2, "/index");
    const char* ind_2=ind;
    mkdir(ind_2, 0777);

    char* bvt = join_paths(cwd_2, "/index/bvt");
    const char* bvt_2=bvt;
    mkdir(bvt_2, 0777);


    chdir("library");
    scan_rec(bvt_2);


    free(ind);
    free(bvt);

    return EXIT_SUCCESS;
}
