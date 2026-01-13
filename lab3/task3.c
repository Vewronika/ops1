#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#define MAXLINE 4096
#define DEFAULT_N 30
#define DEFAULT_M 3

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef unsigned int UINT;

typedef struct thread_args {
    int* track;
    pthread_mutex_t* mxTrack;
    int n;
    int number;
    int* count;
    pthread_mutex_t* mxCount;
    UINT seed;
}thread_args_t;


void sigint(void* _args) {
        free(_args);
}


void ReadArguments(int argc, char** argv, int* n, int* m) {
    *n = DEFAULT_N;
    *m = DEFAULT_M;
    if (argc >= 2) {
        *n = atoi(argv[1]);
    }
    if (argc >= 3) {
        *m = atoi(argv[2]);
    }
}

void* work(void* arg) {
    thread_args_t* args = arg;
    pthread_cleanup_push(sigint, args);
    
    pthread_mutex_lock(&args->mxTrack[0]);
    args->track[0] += 1;
    pthread_mutex_unlock(&args->mxTrack[0]);
    int current = 0;
    int running = 1;
    while (running) {
        int ms = rand_r(&args->seed) % 1320 + 200;
        usleep(ms * 1000);
        int dist = rand_r(&args->seed) % 5 + 1;
        int old = current;
        int moved = 0;



        if (current + dist < 2 * args->n - 2) {
            current = current + dist;
        }
        else {
            current = 2 * args->n - 2;
            pthread_mutex_lock(args->mxCount);
            (*args->count)++;
            printf("Dog finished, place: %d\n", *args->count);
            pthread_mutex_unlock(args->mxCount);
            moved = 1;
            running = 0;
        }
        if (current <= args->n - 1) {
            

            if (args->track[current] > 0) {
                printf("waf waf waf\n");
                current = old;
            }
            else {
                pthread_mutex_lock(&args->mxTrack[current]);
                args->track[current] += 1;
                pthread_mutex_unlock(&args->mxTrack[current]);
                printf("Number: %d, position: %d\n", args->number, current);
                moved = 1;
            }
            
        }
        else {
            

            if (args->track[2 * args->n - current - 2] > 0) {
                printf("waf waf waf\n");
                current = old;
            }
            else {
                pthread_mutex_lock(&args->mxTrack[2 * args->n - current - 2]);
                args->track[2 * args->n - current - 2] += 1;
                pthread_mutex_unlock(&args->mxTrack[2 * args->n - current - 2]);
                printf("Number: %d, position: %d\n", args->number, 2 * args->n - current - 2);
                moved = 1;
            }
            
        }
        if (moved == 1 && old <= args->n - 1) {
            pthread_mutex_lock(&args->mxTrack[old]);
            args->track[old] -= 1;
            pthread_mutex_unlock(&args->mxTrack[old]);
        }
        else if(moved == 1 && old > args->n - 1) {
            pthread_mutex_lock(&args->mxTrack[2 * args->n - old - 2]);
            args->track[2 * args->n - old - 2] -= 1;
            pthread_mutex_unlock(&args->mxTrack[2 * args->n - old - 2]);
        }
        

    }
    pthread_cleanup_pop(1);

    return NULL;
}



int main(int argc, char** argv) {

    int m;
    int n;

    ReadArguments(argc, argv, &n, &m);
    if (n <= 20) {
        ERR("Input");
    }
    if (m <= 2) {
        ERR("Input");
    }
    
    pthread_t* tids = (pthread_t*)malloc(sizeof(pthread_t) * m);
    int* track = malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++) {
        track[i] = 0;
    }


    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    siginfo_t si;
    struct timespec timeout;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0) {
        perror("pthread_sigmask");
    }


    pthread_mutex_t* mxTrack = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * n);
    for (int i = 0; i < n; i++) {
        if (pthread_mutex_init(&mxTrack[i], NULL)) {
            ERR("pthread_mutex_init");
        }
    }
    pthread_mutex_t mxCount = PTHREAD_MUTEX_INITIALIZER;
    int c = 0;
    int* count = &c;
    srand(time(NULL));
    for (int i = 0; i < m; i++) {
        thread_args_t* thread_arg = (thread_args_t*)malloc(sizeof(thread_args_t));
        if (!thread_arg) { ERR("malloc"); }
        thread_arg->track = track;
        thread_arg->mxTrack = mxTrack;
        thread_arg->seed = rand();
        thread_arg->n = n;

        thread_arg->number = i;
        thread_arg->mxCount = &mxCount;
        thread_arg->count = count;
        int err = pthread_create(&tids[i], NULL, work, thread_arg);
        if (err != 0) {
            ERR("Couldn't create thread");
        }
    }
    int running = 1;

    while (running) {
        int sig = sigtimedwait(&sigset, &si, &timeout);
        if (sig == SIGINT) {
            for (int i = 0; i < m; ++i) {
                pthread_cancel(tids[i]);
            }
            for (int i = 0; i < m; ++i) {
                pthread_join(tids[i], NULL);
            }
            running = 0;
            printf("Interrupted by SIGINT — race aborted\n");
            break;
        }
        pthread_mutex_lock(&mxCount);
        if (*count == m)
            running = 0;
        pthread_mutex_unlock(&mxCount);
        printf("Track: ");
        for (int i = 0; i < n; i++) {
            pthread_mutex_lock(&mxTrack[i]);
        }
        for (int i = 0; i < n; i++) {
            printf("%d, ", track[i]);
        }
        printf("\n");
        for (int i = 0; i < n; i++) {
            pthread_mutex_unlock(&mxTrack[i]);
        }
    }

    for (int i = 0; i < m; i++) {
        if (pthread_join(tids[i], NULL)) {
            ERR("Can't join with 'signal handling' thread");
        }

    }


    free(tids);
    free(track);
    for (int i = 0; i < n; i++) {
        pthread_mutex_destroy(&mxTrack[i]);
    }
    pthread_mutex_destroy(&mxCount);
    free(mxTrack);


    return EXIT_SUCCESS;
}