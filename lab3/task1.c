#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXLINE 4096
#define DEFAULT_N 3
#define DEFAULT_K 7

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))


typedef unsigned int UINT;

typedef struct args_thread
{
  pthread_t tid;
  UINT seed;
  int k;
  float* array;
  int* processed; //0 if not, 1 if was
  float* result;
  pthread_mutex_t* mxArray;
} args_thread_t;



void ReadArguments(int argc, char** argv, int* n, int* k) {
  *n = DEFAULT_N;
  *k = DEFAULT_K;

  if (argc >= 2) {
    *n = atoi(argv[1]);
    if (*n <= 0)
    {
      printf("Invalid value for n");
      exit(EXIT_FAILURE);
    }
  }
  if (argc >= 3) {
    *k = atoi(argv[2]);
    if (*k <= 0)
    {
      printf("Invalid value for k");
      exit(EXIT_FAILURE);
    }
  }
}


void* work(void* arg) {
  args_thread_t* args = arg;
  int done = 0;
  while (1) {
    int cell;
    while (1) {
      cell = rand_r(&args->seed) % args->k;
      pthread_mutex_lock(&args->mxArray[cell]);
      if (args->processed[cell] == 0) {
        args->processed[cell] = 1;
        pthread_mutex_unlock(&args->mxArray[cell]);
        break;
      }
      pthread_mutex_unlock(&args->mxArray[cell]);
    }
    

    float value = args->array[cell];
    float root = sqrtf(value);
    printf("sqrt(%f) = %f\n", value, root);
    pthread_mutex_lock(&(args->mxArray[cell]));
    args->result[cell] = root;
    pthread_mutex_unlock(&(args->mxArray[cell]));
    struct timespec ts = { 0, 100000000L };
    nanosleep(&ts, NULL);

    int counter = args->k;
    for (int i = 0; i < args->k; i++) {
      pthread_mutex_lock(&args->mxArray[i]);
      if (args->processed[i] == 1) {
        counter --;
      }
      pthread_mutex_unlock(&args->mxArray[i]);
    }
    if (counter == 0) {
      done = 1;
      break;
    }
  }
  
  return NULL;
}

int main(int argc, char** argv) {

  int n;
  int k;
  ReadArguments(argc, argv, &n, &k);
  float *array = (float*)malloc(sizeof(float) * k);
  float* result = (float*)malloc(sizeof(float) * k);
  if (array == NULL) {
    ERR("malloc");
  }
    
  srand(time(NULL));
  for (int i = 0; i < k; i++) {
    float r = ((float)rand() / (float)RAND_MAX)* (60.0f - 1.0f) + 1.0f;
    array[i] = r;
  }

  if (result != NULL) {
    memcpy(result, array, k * sizeof(float));
  }
  

  args_thread_t* args = (args_thread_t*)malloc(sizeof(args_thread_t) * n);
  pthread_mutex_t* mxArray = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * k);
  int* processed = (int*)malloc(sizeof(int) * k);
  
  for (int i = 0; i < k; i++) {
    processed[i] = 0;
  }

  for (int i = 0; i < k; i++) {
    pthread_mutex_init(&mxArray[i], NULL);
  }
  for (int i = 0; i < n; i++) {
    args[i].array = array;
    args[i].mxArray = mxArray;
    args[i].processed = processed;
    args[i].result = result;
    args[i].k = k;
    args[i].seed = (UINT)rand();
  }

  for (int i = 0; i < n; i++) {
    int err = pthread_create(&(args[i].tid), NULL, work, &args[i]);
    if (err != 0) {
      ERR("Couldn't create thread");
    }
      
  }


  for (int i = 0; i < n; i++) {
    if (pthread_join(args[i].tid, NULL)) {
      ERR("Can't join with the thread");
    }
      
  }

  printf("Array: ");
  for (int i = 0; i < k; i++) {
    printf("%f, ", array[i]);
  }
  printf("\n");
  printf("Roots: ");
  for (int i = 0; i < k; i++) {
    printf("%f, ", result[i]);
  }
  printf("\n");


  free(array);
  free(result);
  free(processed);
  for (int i = 0; i < k; i++) {
    pthread_mutex_destroy(&mxArray[i]);
  }
  free(mxArray);
  
  return EXIT_SUCCESS;
}