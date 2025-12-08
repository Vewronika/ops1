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
volatile sig_atomic_t qty_1 = 0;

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    if (sigaction(sigNo, &act, NULL) < 0)
        ERR("sigaction");
}

void sig_handler(int sig, siginfo_t *info, void *ucontext) { 
    printf("Parent received SIGUSR1 from child PID: %d\n", info->si_pid);
    qty_1++;
    if(kill(info->si_pid, SIGUSR2)<0){
        ERR("kill");
    }
        printf("Sent SIGUSR2 to child PID: %d\n", info->si_pid);
}

void sig_usr2(int sig, siginfo_t *info, void *ucontext){
    sig_count+=1;
}

int parent_work(int p, int t, int n){
    sethandler(sig_handler, SIGUSR1);
    while(qty_1!=n*p){
        //pause();
    }
    while(wait(NULL)>0){


    }
    return EXIT_SUCCESS;
}

int child_work(int p, int t){
    struct timespec m = {0, t * 1000000};

    sethandler(sig_usr2, SIGUSR2);
    for(int i=0; i<p; i++){
        //printf("Started the part %d\n", i);
        nanosleep(&m, NULL);
        
        if(kill(getppid(), SIGUSR1)<0){
            ERR("kill");
        }
        while(sig_count==0){
            pause();
        }
        //printf("Proceed to part %d\n", i+1);
        sig_count=sig_count-1;
    }
    printf("[%d] Finished\n", getpid());
    return EXIT_SUCCESS;

}





int main(int argc, char **argv){

    if(argc<4){
        ERR("Input");
    }

    int p=atoi(argv[1]);
    int t = atoi(argv[2]);
    int n = atoi(argv[3]);

        //sethandler(SIG_IGN, SIGUSR2);

    sethandler(sig_handler, SIGUSR1);
    pid_t pid;
    for(int i=0; i<n; i++){
        if((pid = fork()) < 0){
            ERR("fork");
        }
        if(pid==0){
            child_work(p, t);
        }
    }
    parent_work(p, t, n);

    while (wait(NULL) > 0)
        {
        }

    return EXIT_SUCCESS;
}

