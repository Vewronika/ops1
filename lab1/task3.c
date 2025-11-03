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
#define MAX_LENGTH 1024

#define ERR(source) (perror(source), printf("Err"), exit(EXIT_FAILURE))
void usage(char *pname)
{
    fprintf(stderr, "USAGE:%s -n Name -p OCTAL -s SIZE\n", pname);
    exit(EXIT_FAILURE);
}



void delete_line(const char* line, FILE* req){

    FILE* f;
    if((f = fopen("file.txt", 'a')) == NULL) ERR(("fopen"));

    char buffer[MAX_LENGTH];
    while(fgets(buffer, MAX_LENGTH, req)!= NULL){
        if(strstr(line, buffer) == NULL){
            fprintf(f, "s", buffer);
        }
        else {
            continue;
        }
    }

    unlink("requirements.txt");
    rename("file.txt", "requirements.txt");
}



void create_env(int create, const char* name_env, const char* name_version, const char* name_remove){
    FILE* s1;
    if(create==1){
    if(mkdir(name_env, 0777)!=0){
        ERR("mkdir");
    }
    }
    if (chdir(name_env)){
            ERR("chdir");
    }

    if ((s1 = fopen("requirements.txt", "a")) == NULL){
        ERR("fopen");
    }
    if(name_version==NULL){
        return;
    }
    if(name_version){
        char* t = strchr(name_version, '=');
        char* ver = t+2;
        *t='\0';
        fprintf(s1, "%s %s", name_version, ver);
    }
    if (fclose(s1))
        ERR("fclose");


    if(name_version!=NULL){
    FILE* pack;
    const char* cat = strcat(name_version, ".txt");
    if ((pack = fopen(cat, "wb")) == NULL){
        ERR("fopen");
    }
    FILE* urand;
    if ((urand = fopen("/dev/urandom", "rb")) == NULL){
        ERR("fopen");
    }
    unsigned char buffer[1024];
    size_t blocks = 10;

    for(int i =0; i<blocks; i++){
        ssize_t rd = fread(buffer, 1, sizeof(buffer), urand);
        ssize_t n = fwrite(buffer, 1, sizeof(buffer), pack);
    }

    if (fclose(pack))
        ERR("fclose");
    }

    if(name_remove!=NULL){
    if ((s1 = fopen("requirements.txt", "a")) == NULL){
        ERR("fopen");
    }

    char buffer[MAX_LENGTH];
    while(fgets(buffer, MAX_LENGTH, s1)!=NULL){
        if(strstr(name_remove, buffer)!=NULL){
            
        }
    }

    if (fclose(s1))
        ERR("fclose");
    }

}


int main(int argc, char** argv){

    int c;
    int create=0;
    const char* name_env;
    char* temp;
    char* temp2;
    const char* name_version;
    const char* name_remove;

    while ((c = getopt(argc, argv, "cv:ri:")) != -1){
        switch (c)
        {
            case 'c':
                create = 1;

                break;
            case 'v':
                temp = optarg;
                temp2 = malloc(strlen(temp) + 1);
                strncpy(temp2, temp, strlen(temp));
                temp2[strlen(temp)]='\0';
                temp2[strlen(temp)-1]='\0';
                name_env=temp2+1;
                printf("%s\n", name_env);
                break;
            case 'i':
                name_version = optarg;
            break;
            case 'r':
                name_remove=optarg;
            break;
            case '?':
            default:
                usage(argv[0]);
        }
    }


    char path_cwd[MAX_PATH];
    if (getcwd(path_cwd, MAX_PATH) == NULL)
        ERR("getcwd");

    create_env(create, name_env, name_version, name_remove);

    if (chdir(path_cwd)){
            ERR("chdir");
    }
    free(temp2);
    return EXIT_SUCCESS;
}