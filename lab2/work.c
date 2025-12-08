#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define ITER_COUNT 25
volatile sig_atomic_t work = 1;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}


ssize_t read_line(int fd, char *buf, size_t maxlen){

    size_t i = 0;
    char c;
    ssize_t n;

    while(i+1 < maxlen){
        n = read(fd, &c, 1);

        if(n==0){
            break;
        }
        if(n<0){
            return -1;
        }
        buf[i++] = c;

        if(c == '\n'){
            i--;
            break;
        }
    }

    buf[i] = '\0';

    return i;

}
void handler(int sigNo){
    sethandler(SIG_IGN, SIGINT);
    kill(0, SIGINT);
    work = 0;
}


void filename(char* name, pid_t pid_org){

    sethandler(handler, SIGINT);

    printf("My name is %s with pid [%d]\n", name, getpid());
    char str1[256];
    char str2[256];

    char path[256];
    snprintf(path, sizeof(path), "%s.txt", name);

    int fd=open(path, O_RDONLY);
    if(fd<0) ERR("open");
    read_line(fd, str1, sizeof(str1));
    read_line(fd, str2, sizeof(str2));

    if(close(fd) < 0) ERR("close");



    if(strcmp("-", str1)){
        printf("%s inspecting %s\n", name, str1);
        pid_t pid;
        if((pid=fork())<0){
            ERR("fork");
        }
        if(pid==0){
            filename(str1, pid_org);
            return;
        }
        
        if(strcmp("-", str2)){
            printf("%s inspecting %s\n", name, str2);
            pid_t pid2;
            if((pid2=fork())<0){
                ERR("fork");
            }
            if(pid2==0){
                filename(str2, pid_org);
                return;
            }
        }
    }


    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);


    siginfo_t info;
    int sig;


    while(work){
        sig  = sigwaitinfo(&mask, &info);
    
    srand(time(NULL)*getpid());
    int s=rand()%3;
    if(s==1){
        printf("%s received a document. Sending to the archieve\n", name);
    }else{
        if(getpid()==pid_org){
            printf("%s received a document. Ignoring\n", name);
        }
        else{
            printf("%s received a document. Passing on to the supevisor\n", name);
            if((kill(getppid(), SIGUSR2))<0){
                ERR("kill");
            }
        }
    }
}


    while(wait(NULL)>0){

    }
    //printf("%s has inspected all submarinas\n", name);
    printf("%s is leaving the office\n", name);

}



int main(int argc, char**argv){

    if(argc != 3) ERR("argv");

    char* path = argv[1];
    char* name = argv[2];

    // sigset_t mask, oldmask;
    // sigemptyset(&mask);
    // sigaddset(&mask, SIGUSR1);
    // sigaddset(&mask, SIGUSR2);
    // sigaddset(&mask, SIGINT);
    // sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // sigset_t suspend;
    // sigemptyset(&suspend);
    // sigaddset(&suspend, SIGUSR2);
    // sigaddset(&suspend, SIGINT);
    

    printf("%d\n", getpid());

    //sigsuspend(&suspend);
    //sigwait(&suspend);


    char cwd[256];
    if(getcwd(cwd, sizeof(cwd))<0) ERR("getcwd");
    if(chdir(path) < 0) ERR("chdir");

    filename(name, getpid());

    if(chdir(cwd) < 0) ERR("chdir");



    return EXIT_SUCCESS;
}