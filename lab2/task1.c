#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t sig_count = 0;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sig_handler(int sig) { sig_count++; 
printf("Sig_count: %d\n", sig_count);
}

void parent_work(){
    sethandler(sig_handler, SIGUSR1);
    sethandler(SIG_IGN, SIGUSR2);
    while(sig_count!=100){
        pause();
    }
    printf("Reached 100\n");
    if(kill(0, SIGUSR2)){
        ERR("kill");
    }
    
}

int child_work(){
    srand(getpid());
    int s = rand()%101+100;
    struct timespec t = {0, s * 1000};
    sethandler(SIG_DFL, SIGUSR2);

    while(1){
        nanosleep(&t, NULL);
        kill(getppid(), SIGUSR1);

    }

    return EXIT_SUCCESS;

}





int main(int argc, char **argv){

    int n;
    if(argc!=2){
        ERR("Input");
    }
    sethandler(SIG_IGN, SIGUSR1);
    n=atoi(argv[1]);
    pid_t pid;
    for(int i=0; i<n; i++){
    if ((pid = fork()) < 0)
        ERR("fork");
    
    if (0 == pid){
        printf("forked, child%d\n", i);
        child_work();
    }

    }

    parent_work();
    while (wait(NULL) > 0)
        {
        }

    return EXIT_SUCCESS;
}