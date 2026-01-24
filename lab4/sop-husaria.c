#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define FS_NUM 3
#define WAGON 4
typedef unsigned int UINT;

void ms_sleep(unsigned int milli)
{
    struct timespec ts = {milli / 1000, (milli % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

void usage(int argc, char* argv[])
{
    printf("%s N M\n", argv[0]);
    printf("\t10 <= N <= 20 - number of banner threads\n");
    printf("\t2 <= M <= 8 - number of artillery threads\n");
    exit(EXIT_FAILURE);
}

typedef struct{
    pthread_t tid;
    sem_t* semaphore;
    int id;
    int *enemy_hp;
    pthread_mutex_t *mxEnemy;
     pthread_cond_t *cond;
    int *charge;
    UINT seed;
    pthread_barrier_t *barrier;
    sem_t *wagon;

}banners_args_t;

typedef struct{
    pthread_t tid;
    int *enemy_hp;
    pthread_mutex_t *mxEnemy;
    UINT seed;
    pthread_barrier_t *barrier;
     pthread_cond_t *cond;
     int n;
     int *charge;
}artillery_args_t;


void ReadArguments(int argc, char **argv, int *n, int *m){
    if(argc!=3){
        usage(argc, argv);
    }
    *n = atoi(argv[1]);
    *m = atoi(argv[2]);
}

void *work(void *arg){
    banners_args_t *args = arg;

    sem_wait(args->semaphore);
    int time = 80+rand_r(&args->seed)%41;
    ms_sleep((UINT)time);
    printf("CAVALRY %d: IN POSITION\n", args->id);
    if (sem_post(args->semaphore) == -1)
        ERR("sem_post");

    pthread_mutex_lock(args->mxEnemy);
    while(!*args->charge){
        pthread_cond_wait(args->cond, args->mxEnemy);
    }
    printf("Cavalry %d: ready to charge\n", args->id);
     pthread_mutex_unlock(args->mxEnemy);

     for(int i =0; i<3; i++){
        int result = pthread_barrier_wait(args->barrier);
        if(result == PTHREAD_BARRIER_SERIAL_THREAD){
            printf("Charge %d\n", i+1);

        }
        ms_sleep(500);
        pthread_mutex_lock(args->mxEnemy);
        (*args->enemy_hp)--;
        pthread_mutex_unlock(args->mxEnemy);

        sem_wait(args->wagon);
        ms_sleep(100);
        printf("Cavalry %d: lance retreived", args->id);
        sem_post(args->wagon);


     }




    return NULL;
}

void *work_a(void *arg){
    artillery_args_t *args = arg;
    while(*args->enemy_hp>=50){
        pthread_barrier_wait(args->barrier);

        int hp = 1+rand_r(&args->seed)%6;
        ms_sleep(400);
        pthread_mutex_lock(args->mxEnemy);

        *(args->enemy_hp)-=hp;
        pthread_mutex_unlock(args->mxEnemy);

        int result = pthread_barrier_wait(args->barrier);
        if(result == PTHREAD_BARRIER_SERIAL_THREAD){
            printf("ARTILLERY: ENEMY HP %d\n", *args->enemy_hp);
        }
    }
    int result = pthread_barrier_wait(args->barrier);
        if(result == PTHREAD_BARRIER_SERIAL_THREAD){
            printf("Below 50: ENEMY HP %d\n", *args->enemy_hp);
            pthread_mutex_lock(args->mxEnemy);
            *args->charge = 1;
             pthread_cond_broadcast(args->cond);
             pthread_mutex_unlock(args->mxEnemy);
            
        }
    
    return NULL;
}

int main(int argc, char* argv[]) { 
    
    int n;
    int m;
    ReadArguments(argc, argv, &n, &m);

    banners_args_t* args = (banners_args_t*)malloc(sizeof(banners_args_t)*n);
    if(!args){
        ERR("malloc");
    }

    artillery_args_t* args_a = (artillery_args_t*)malloc(sizeof(artillery_args_t)*m);
    if(!args_a){
        ERR("malloc");
    }

    sem_t semaphore;
    if (sem_init(&semaphore, 0, FS_NUM) != 0)
        ERR("sem_init");

    sem_t wagon;
    if (sem_init(&wagon, 0, WAGON) != 0)
        ERR("sem_init");

    srand(time(NULL));

    int x=100;
    int *enemy_hp =&x;
    int y = 0;
    int *charge = &y;
    pthread_mutex_t mxEnemy = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, m);

    for(int i=0; i<n; i++){
        args[i].id = i;
        args[i].semaphore = &semaphore;
        args[i].seed = rand();
        args[i].enemy_hp=enemy_hp;
        args[i].mxEnemy = &mxEnemy;
        args[i].cond = &cond;
        args[i].charge = charge;
        args[i].barrier = &barrier;
        args[i].wagon = &wagon;
    }
    for(int i =0; i<m ;i++){
        args_a[i].seed = rand();
        args_a[i].enemy_hp=enemy_hp;
        args_a[i].barrier = &barrier;
        args_a[i].mxEnemy = &mxEnemy;
        args_a[i].cond = &cond;
        args_a[i].n=n;
        args_a[i].charge = charge;

    }
    for(int i =0; i<n; i++){
        pthread_create(&args[i].tid, NULL, work, &args[i]);
    }
    for(int i=0; i<m; i++){
        pthread_create(&args_a[i].tid, NULL, work_a, &args_a[i]);
    }
    for(int i =0; i<m;i++){
        pthread_join(args_a[i].tid, NULL);
    }

    for(int i =0; i<n;i++){
        pthread_join(args[i].tid, NULL);
    }

    printf("Battle ended, %d\n", *enemy_hp);
    if(*enemy_hp<=0){
        printf("VENIMUS, VIDIMUS, DEUS VICIT!\n");
    }
    

    //pthread_mutex_destroy()
    pthread_mutex_destroy(&mxEnemy);
    free(args);
    
    free(args_a);
    return EXIT_SUCCESS; 

}