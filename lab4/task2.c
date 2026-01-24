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


#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define DECK_TOTAL 52
#define HAND_SIZE 7


typedef unsigned int UINT;

typedef struct thread_args {
  pthread_t tid;
  int deck[7];
  UINT seed;
  pthread_barrier_t *barrier;
  int *win;
  pthread_mutex_t *mxWin;
  int index;
  int *exchange_array;
  int n;
  pthread_cond_t *cond;


}thread_args_t;

void Read_args(int argc, char** argv, int *n) {
  if (argc != 2 ) {
    ERR("input");
  }
  else {
    if (atoi(argv[1]) > 7 || atoi(argv[1]) <= 0) {
      ERR("input");
    }
    *n = atoi(argv[1]);
  }
}

void Count_suits(int *array, int *deck){
    for(int i = 0; i<7; i++){
        if(deck[i]%4==0){
            array[0]++;
        }
        else if(deck[i]%4==1){
            array[1]++;
        }
        else if(deck[i]%4==2){
            array[2]++;
        }
        else if(deck[i]%4==3){
            array[3]++;
        }
    }
}

void print_deck(const int* deck, int size)
{
    const char* suits[] = { " of Hearts", " of Diamonds", " of Clubs", " of Spades" };
    const char* values[] = { "2","3","4","5","6","7","8","9","10","Jack","Queen","King","Ace" };

    char buffer[1024];
    int offset = 0;

    if (size < 1 || size > HAND_SIZE)
        return;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[");

    for (int i = 0; i < size; ++i) {
        int card = deck[i];
        if (card < 0 || card >= DECK_TOTAL)  
            return;
        int suit = card % 4;
        int value = card / 4;

        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s%s", values[value], suits[suit]);
        if (i < size - 1)
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, ", ");
    }
    snprintf(buffer + offset, sizeof(buffer) - offset, "]");
    puts(buffer);
}

void* work(void* arg) {
  thread_args_t* args = arg;

//pthread_barrier_wait(args->barrier);
pthread_cond_wait()
  int array[4];

  while(1){
    print_deck(args->deck, 7);
    for(int i =0; i<4; i++){
        array[i]=0;
    }
  Count_suits(array, args->deck);
  if(array[0]==7 || array[1]==7 || array[2]==7 || array[3]==7){
    printf("I win\n");
    pthread_mutex_lock(args->mxWin);
    *args->win = 1;
    pthread_mutex_unlock(args->mxWin);
  }
  
  pthread_barrier_wait(args->barrier);
    if(*args->win){

        return NULL;
    }

    if(args->index==0){
        int card_index = rand_r(&args->seed)%7;
        args->exchange_array[1] = args->deck[card_index];
        while(args->exchange_array[0]==-1){}
        int new_card = args->exchange_array[0];
        args->deck[card_index]=new_card;
    }
    else{
        while(args->exchange_array[args->index]==-1){}
        int new_card = args->exchange_array[args->index];
        int card_index = rand_r(&args->seed)%8;
        if(card_index==7){
            args->exchange_array[(args->index+1)%args->n] = new_card;
        }
        else{
            args->exchange_array[(args->index+1)%args->n] = args->deck[card_index];
            args->deck[card_index] = new_card;
        }
        
        
    }

  pthread_barrier_wait(args->barrier);
  if(args->index==0){
    for(int i =0; i<args->n; i++){
        args->exchange_array[i]=-1;
    }
    
  }
  }
  
  printf("returning\n");
  return NULL;
}

int main(int argc, char** argv) {
  int n;
  Read_args(argc, argv, &n);
  int taken[52];
  for (int i = 0; i < 52; i++) {

    taken[i] = 0;
  }

  srand(time(NULL));


  sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

  pthread_sigmask(SIG_BLOCK, &set, NULL);

pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, n);

    int win=0;
pthread_mutex_t mxWin =  PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  
  
  thread_args_t* args = (thread_args_t*)malloc(sizeof(thread_args_t) * n);
  if (!args) {
    ERR("malloc");
  }
  int* exchange_array = (int*)malloc(sizeof(int) * n);
  if (!exchange_array) {
    ERR("malloc");
  }
  for(int i = 0; i<n; i++){
    exchange_array[i] = -1;
  }
  int sig;
  printf("%d\n", getpid());

  for (int i = 0; i < n; i++) {
    sigwait(&set, &sig);
    printf("Creating new thread:\n");
    args[i].seed = rand();
    args[i].barrier = &barrier;
    args[i].win = &win;
    args[i].mxWin = &mxWin;
    args[i].index=i;
    args[i].n=n;
    args[i].cond = &cond;
    args[i].exchange_array=exchange_array;
    int ind;
    for (int j = 0; j < 7; j++) {
      ind = rand() % 52;
      while (taken[ind]==1) {
        ind = rand() % 52;
      }
      args[i].deck[j] = ind;
      taken[ind] = 1;
    }

    if (pthread_create(&args[i].tid, NULL, work, &args[i])) {
      ERR("Couldn't create thread");
    }
      
    

  }


  for (int i = 0; i < n; i++) {
    int err = pthread_join(args[i].tid, NULL);
    if (err != 0)
      ERR("Can't join with a thread");
  }
  
free(exchange_array);
pthread_barrier_destroy(&barrier);
  free(args);

  return EXIT_SUCCESS;
}
