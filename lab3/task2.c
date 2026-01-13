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
#define DEFAULT_P 2

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef unsigned int UINT;

typedef struct print_args {
  pthread_t tid;
  int* array;
  int n;
  pthread_mutex_t* mxArray;
  int* active_threads;
  pthread_mutex_t* mxActive;


}print_args_t;


typedef struct swap_args {
  pthread_t tid;
  int a;
  int b;
  int n;
  int* array;
  pthread_mutex_t* mxArray;
  int* active_threads;
  pthread_mutex_t* mxActive;
}swap_args_t;



void* print_work(void* arg) {
  print_args_t* args = arg;
  printf("Array: ");
  for (int i = 0; i < args->n; i++) {
    pthread_mutex_lock(&(args->mxArray[i]));
  }
  for (int i = 0; i < args->n; i++) {
    
    printf("%d, ", args->array[i]);
  }
  printf("\n");
  for (int i = 0; i < args->n; i++) {
    pthread_mutex_unlock(&(args->mxArray[i]));
  }
  pthread_mutex_lock(args->mxActive);
  (*args->active_threads)--;
  pthread_mutex_unlock(args->mxActive);
  free(args);
  return NULL;
}

void* swap_work(void* arg) {
  swap_args_t* args = arg;
  pthread_mutex_lock(&(args->mxArray[args->a]));
  pthread_mutex_lock(&(args->mxArray[args->b]));

  int temp = args->array[args->a];
  args->array[args->a] = args->array[args->b];
  args->array[args->b] = temp;

  pthread_mutex_unlock(&(args->mxArray[args->a]));
  pthread_mutex_unlock(&(args->mxArray[args->b]));

  pthread_mutex_lock(args->mxActive);
  (*args->active_threads)--;
  pthread_mutex_unlock(args->mxActive);
  free(args);
  return NULL;
}

void ReadArguments(int argc, char** argv, int *n, int *p) {
  *n = DEFAULT_N;
  *p = DEFAULT_P;
  if (argc >= 2) {
    *n = atoi(argv[1]);
    if (*n < 8 || *n>256) {
      ERR("usage");
    }
  }
  if (argc >= 3) {
    *p = atoi(argv[2]);
    if (*p < 1 || *p>16) {
      ERR("usage");
    }
  }
  


}

int main(int argc, char** argv) {

  int n;
  int p;
  ReadArguments(argc, argv, &n, &p);

  int* array = malloc(sizeof(int) * n);
  if (!array) { ERR("malloc"); }
  for (int i = 0; i < n; i++) {
    array[i] = i;
  }

  printf("%d\n",getpid());
  pthread_mutex_t* mxArray = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * n);
  if (!mxArray) { ERR("malloc"); }
  for (int i = 0; i < n; i++) {
    if (pthread_mutex_init(&mxArray[i], NULL))
      ERR("pthread_mutex_init");
  }

  sigset_t oldMask, newMask;
  sigemptyset(&newMask);
  sigaddset(&newMask, SIGUSR1);
  sigaddset(&newMask, SIGUSR2);
  sigaddset(&newMask, SIGINT);
  if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask)) {
    ERR("SIG_BLOCK error");
  }
  int signo;
  srand(time(NULL));
  int act_t = 0;
  int* active_threads = &act_t;
  pthread_mutex_t mxAct = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t* mxActive = &mxAct;
  pthread_attr_t threadAttr;
  int running = 1;
  while (running) {
    if (sigwait(&newMask, &signo)) {
      ERR("sigwait failed.");
    }

    switch (signo) {
    case SIGUSR1:
      pthread_mutex_lock(mxActive);
      if (*active_threads == p) {
        printf("Can`t craete more threads\n");
        pthread_mutex_unlock(mxActive);
        break;
      }
      int b = rand() % (n-1) + 1;
      int a = rand() % b;

      swap_args_t* swap_arg = (swap_args_t*)malloc(sizeof(swap_args_t));
      if (!swap_arg) { ERR("malloc"); }
      swap_arg->a = a;
      swap_arg->b = b;
      swap_arg->n = n;
      swap_arg->array = array;
      swap_arg->mxArray = mxArray;
      swap_arg->mxActive = mxActive;
      swap_arg->active_threads = active_threads;

      
      if (pthread_attr_init(&threadAttr))
        ERR("Couldn't create pthread_attr_t");
      if (pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED))
        ERR("Couldn't setdetachsatate on pthread_attr_t");


      int err = pthread_create(&swap_arg->tid, &threadAttr, swap_work, swap_arg);
      if (err != 0) {
        ERR("Couldn't create thread");
      }
      (*active_threads)++;
      pthread_attr_destroy(&threadAttr);
      pthread_mutex_unlock(mxActive);
      break;
    case SIGUSR2:
      pthread_mutex_lock(mxActive);
      if (*active_threads == p) {
        printf("Can`t craete more threads\n");
        pthread_mutex_unlock(mxActive);
        break;
      }
      print_args_t* print_arg = (print_args_t*)malloc(sizeof(print_args_t));
      if (!print_arg) { ERR("malloc"); }
      print_arg->n = n;
      print_arg->array = array;
      print_arg->mxArray = mxArray;
      print_arg->mxActive = mxActive;
      print_arg->active_threads = active_threads;

      if (pthread_attr_init(&threadAttr))
        ERR("Couldn't create pthread_attr_t");
      if (pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED))
        ERR("Couldn't setdetachsatate on pthread_attr_t");

      err = pthread_create(&print_arg->tid, &threadAttr, print_work, print_arg);
      if (err != 0) {
        ERR("Couldn't create thread");
      }
      (*active_threads)++;
      pthread_attr_destroy(&threadAttr);


      pthread_mutex_unlock(mxActive);
      break;
    case SIGINT:
      pthread_mutex_lock(mxActive);
      while (*active_threads > 0) {
        pthread_mutex_unlock(mxActive);
        usleep(1000);
        pthread_mutex_lock(mxActive);
      }
      pthread_mutex_unlock(mxActive);
      running=0;
      
      break;
    default:
      printf("unexpected signal %d\n", signo);
      exit(1);
    }
  }

  if (pthread_sigmask(SIG_UNBLOCK, &newMask, &oldMask)) {
    ERR("SIG_BLOCK error");
  }
  pthread_mutex_destroy(mxActive);
  for (int i = 0; i < n; i++)
    pthread_mutex_destroy(&mxArray[i]);
  free(mxArray);
  free(array);

  return EXIT_SUCCESS;
}
