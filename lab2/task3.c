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

volatile sig_atomic_t counter = 0;
volatile sig_atomic_t work = 0;
volatile sig_atomic_t sigint = 0;

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


void sig_handler_1(int sig, siginfo_t *info, void *ucontext) { 
    printf("%d Child received SIGUSR1 from parent: %d\n", getpid(), info->si_pid);
    work=1;
}
void sig_handler_int_par(int sig, siginfo_t *info, void *ucontext) { 
    work=0;
}
void sig_handler_2(int sig, siginfo_t *info, void *ucontext){
    work=0;

}

void sig_sigint(int sig, siginfo_t *info, void *ucontext){
    printf("SIGINT %d\n", getpid());
    sigint=1;

}


int child_work(int n){
    sethandler(sig_handler_1, SIGUSR1);
    sethandler(sig_handler_2, SIGUSR2);
    sethandler(sig_sigint, SIGINT);
    srand(getpid());
    while(!sigint){
    while(!work && !sigint){
        pause();
    }
    while(work && !sigint){
        int s=rand()%101+100;
        struct timespec t = {0, s*1000000};

        nanosleep(&t, NULL);
        counter++;
        printf("%d, counter: %d\n", getpid(), counter);
    }
    printf("%d sigint: %d\n", getpid(), sigint);
    }
    printf("Stopped\n");
    return EXIT_SUCCESS;
}


int main(int argc, char **argv){
    if(argc!=2){
        ERR("Input");
    }
    int n = atoi(argv[1]);
    pid_t pid;
    int buff[1024];
   
    for(int i = 0;i<n; i++){
        if((pid=fork())<0){
            ERR("fork");
        }
        buff[i]=pid;
        if(pid==0){
            child_work(i);
            return EXIT_SUCCESS;
        }
        
    }
    work=1;
    if(kill(buff[0], SIGUSR1)){
        ERR("kill");
    }
    sethandler(sig_handler_int_par, SIGINT); 
    while(work){
        for(int i =0; i<n; i++){

        
        sleep(3);
        if(kill(buff[i%n], SIGUSR2)){
            //printf("Sent sig1 to %d\n", buff[i%n]);
        ERR("kill");
    }
    //sleep(3);
    if(kill(buff[(i+1)%n], SIGUSR1)){
        ERR("kill");
    }
    }
    }

    for(int i=0; i<n; i++){
        if((kill(buff[i], SIGINT))){
            ERR("kill");
        }
    }



    
    while(wait(NULL)>0){

    }

    return EXIT_SUCCESS;
}
