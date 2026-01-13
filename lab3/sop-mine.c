    #define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define ORES 10
#define CART_SIZE 10

void usage(int argc, char* argv[])
{
    printf("%s p\n", argv[0]);
    printf("\tp - path to a directory\n");
    exit(EXIT_FAILURE);
}

void print_ores(int ores[ORES])
{
    printf("Ores:\n");
    printf("- Coal: %d\n", ores[0]);
    printf("- Copper: %d\n", ores[1]);
    printf("- Iron: %d\n", ores[2]);
    printf("- Silver: %d\n", ores[3]);
    printf("- Gold: %d\n", ores[4]);
    printf("- Platinum: %d\n", ores[5]);
    printf("- Diamond: %d\n", ores[6]);
    printf("- Emerald: %d\n", ores[7]);
    printf("- Ruby: %d\n", ores[8]);
    printf("- Sapphire: %d\n", ores[9]);
}

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}



typedef struct{
    int* array;
    pthread_mutex_t* arraym;
    pthread_t tid;
    char* path;
} threadArgs_t;


typedef struct{

    int* array;
    pthread_t tid;
    pthread_mutex_t* arraym;
    char* path;
} fileArgs_t;


void freed(void* arg){



}

void freef(void* arg){
    


}


void* files(void* arg){



    fileArgs_t* args = arg;

        printf("started file thread %ld\n", args->tid);

    const int fd = open(args->path, O_RDONLY);
    if (fd == -1) ERR("open");


int br = 1;

    while (br)
    {
        char buf[10];
        for(int i = 0; i < 10; i++){
            buf[i] = -1;
        }

        const ssize_t read_size = bulk_read(fd, buf, 10);
        if (read_size == -1)
            ERR("bulk_read");

        


        for(int i = 0; i < 10; i++){
            if(buf[i] == -1){
                br = 0;
                break;
            }
            if((buf[i] >= '0' && buf[i] <= '9') && buf[i] != -1){
                

                //int num = atoi(buf[i]);
                int num = buf[i] - '0';
                //printf("NUMBER %d: %d\n",i, num);


                pthread_mutex_lock(&args->arraym[num]);
                args->array[num]++;
                pthread_mutex_unlock(&args->arraym[num]);
            }
        }

    }



        printf("end file thread %ld\n", args->tid);

    free(args->path);
    free(args);

    return NULL;
}



void* scan(void* arg){
    threadArgs_t* args = arg;

        printf("started dir thread %ld\n", args->tid);

       DIR *dirp;
    struct dirent *dp;
    struct stat filestat;
    //int dirs = 0, files = 0, links = 0, other = 0;

    int counter = 0;
    pthread_t pids[1024];
            pthread_cleanup_push(freed, &args);



    //printf("array %d\n", args->array[0]);

    //printf("%s", args->path);
    if ((dirp = opendir(args->path)) == NULL)
        ERR("opendir");
    do
    {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL)
        {

            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", args->path, dp->d_name);
           
            if (lstat(path, &filestat))
                ERR("lstat");

            if((strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..")) == 0) continue;

            if (S_ISDIR(filestat.st_mode)){
                printf("%ld: %s\n", args->tid, dp->d_name);

                threadArgs_t* argst = (threadArgs_t*)malloc(sizeof(threadArgs_t));
                argst->array = args->array;
                argst->arraym = args->arraym;

                //strcpy(argst->path, path);
                argst->path = strdup(path);
                printf("path: %s\n", argst->path);

                pthread_create(&argst->tid, NULL, scan, argst);
                pids[counter] = argst->tid;
                counter++;
                break;

            }

            else if (S_ISREG(filestat.st_mode)){
                printf("%ld: %s\n", args->tid, dp->d_name);


                fileArgs_t* argsf = (fileArgs_t*)malloc(sizeof(fileArgs_t));
                argsf->array = args->array;
                argsf->arraym = args->arraym;
                argsf->path = strdup(path);

                pthread_create(&argsf->tid, NULL, files, argsf);
                pids[counter] = argsf->tid;
                counter++;
            }
        
            else if (S_ISLNK(filestat.st_mode)){

            }
                
            else{}
                
        }
    } while (dp != NULL);

    if (errno != 0)
        ERR("readdir");
    if (closedir(dirp))
        ERR("closedir");
    //printf("Files: %d, Dirs: %d, Links: %d, Other: %d\n", );




    for(int i = 0; i < counter; i++){
        pthread_join(pids[i], NULL);
    }

        pthread_cleanup_pop(1);

        printf("send die thread %ld\n", args->tid);
    free(args->path);
    free(args);

    return NULL;
}



typedef struct argsSignalHandler
{
    pthread_t tid;
    //int *pArrayCount;
    int *array;
    pthread_mutex_t *arraym;
    sigset_t *pMask;
    //bool *pQuitFlag;
    //pthread_mutex_t *pmxQuitFlag;
} argsSignalHandler_t;



void *signal_handling(void *voidArgs)
{
    argsSignalHandler_t *args = voidArgs;
    int signo;
    srand(time(NULL));
    for (;;)
    {
        if (sigwait(args->pMask, &signo))
            ERR("sigwait failed.");
        switch (signo)
        {
            case SIGUSR1:
            printf("received SIGUSR1\n");
                for(int i = 0; i <10; i++){
                    pthread_mutex_lock(&args->arraym[i]);
                }
                //pthread_mutex_lock(args->pmxArray);
                for(int i = 0; i <10; i++){
                    printf("array %d: %d\n", i, args->array[i]);
                }

                for(int i = 0; i < 10; i++){
                    pthread_mutex_unlock(args->arraym);
                }

                break;
            case SIGINT:
                printf("received sigint\n");

                // pthread_cancel all of the threads
                return NULL;
            default:
                printf("unexpected signal %d\n", signo);
                //exit(1);
        }
    }
    return NULL;
}






int main(int argc, char** argv){


    threadArgs_t* args = (threadArgs_t*)malloc(sizeof(threadArgs_t));

    //args->path = argv[1];
    args->path = strdup(argv[1]);

    argsSignalHandler_t* sigargs = (argsSignalHandler_t*)malloc(sizeof(argsSignalHandler_t));

    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    sigaddset(&newMask, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");



    int* array = (int*)malloc(sizeof(int)*10);
    pthread_mutex_t* arraym = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*10);

    for(int i = 0; i < 10; i++){
        pthread_mutex_init(&arraym[i], NULL);
    }
    
    for(int i = 0; i < 10; i++){
        array[i] = 0;
    }

    args->array = array;
    args->arraym = arraym;

    sigargs->array = array;
    sigargs->arraym = arraym;
    sigargs->pMask = &newMask;


    pthread_create(&args->tid, NULL, scan, args);

    pthread_join(args->tid, NULL);

    for(int i = 0; i < 10; i++){
        printf("ARR %d: %d\n", i, array[i]);
    }


    for(int i = 0; i < 10; i++){
        pthread_mutex_destroy(&arraym[i]);
    }

    free(arraym);
    //free(args);
    free(array);
    return EXIT_SUCCESS;
}