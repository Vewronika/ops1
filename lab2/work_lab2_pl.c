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

ssize_t bulk_write(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (TEMP_FAILURE_RETRY(nanosleep(&ts, &ts)))
        ERR("nanosleep");
}

void usage(int argc, char* argv[])
{
    printf("%s n\n", argv[0]);
    printf("\t1 <= n <= 4 -- number of moneyboxes\n");
    exit(EXIT_FAILURE);
}


void child_work(){

    printf("[%d] Collection box opened\n", getpid());


    char name[100];
    pid_t pid = getpid();
    snprintf(name, 100, "skarbona_%d.txt", pid);



    int out;
    if((out = open(name, O_CREAT | O_TRUNC | O_RDONLY, 0777)) < 0) ERR("open");




    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);


    siginfo_t info;
    int sig;
    int sum = 0;


    while(1){
        sig  = sigwaitinfo(&mask, &info);
        if(sig==SIGTERM){
            break;
        }
        else{
        char pidd[64];
        char donation[64];

        //printf("%d received %d\n", getpid(), sig);

        int count;
        if ((count = read(out, pidd, sizeof(int))) < 0) ERR("read");
        if ((count = read(out, donation, sizeof(int))) < 0) ERR("read");
        sum += atoi(donation);


        printf("[%d] Citizen %s threw in %s PLN. Thank you! Total collected: %d PLN\n", getpid(), pidd, donation, sum);
        }

    }

        if(close(out) == -1) ERR("close");

}


void donor_work(int n, int buff[1024]){

    
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);


    siginfo_t info;
    int sig = sigwaitinfo(&mask, &info);

    int num;
    switch(sig){
        case SIGUSR1:
            num = 0;
            break;
        case SIGUSR2:
            num = 1;
            break;

        case SIGPIPE:
            num = 2;
            break;
        case SIGINT:
            num = 3;
            break;

    }

    printf("[%d] Directed to collection box no %d\n", getpid(), num);


    srand(getpid());

    if(num>n){
        printf("[%d] Nothing here, I'm going home!\n", getpid());
    }

    else{
        char name[100];
        pid_t pid = buff[num];
        snprintf(name, 100, "skarbona_%d.txt", pid);


        const int fd = open(name, O_APPEND | O_WRONLY, 0777);


        if(fd < 0) ERR("open");
        
        int s = rand()%1901 + 100;

        char buf1[64];
        int len1 = snprintf(buf1, sizeof(buf1), "%d", getpid());
        char buf2[64];
        int len2 = snprintf(buf2, sizeof(buf2), "%d", s);


        int count;

        if ((count = write(fd, buf1, len1)) < 0) ERR("read");


        //if ((count = bulk_write(fd, buf2, (ssize_t)len2)) < 0)  ERR("read");
        if ((count = write(fd, buf2, len2)) < 0) ERR("read");


        if((close(fd))<0) ERR("close");

        printf("[%d] I'm throwing in %d PLN\n", getpid(), s);
    

        kill(pid, SIGUSR1);
        printf("%d send %d to %d\n", getpid(), SIGUSR1, pid);
        
    }


}

void term_handler(int sigNo){
    sethandler(SIG_IGN, SIGTERM);
    if((kill(0, SIGTERM))<0){
        ERR("kill");
    }
    work=0;
}



int main(int argc, char** argv){


    if (argc != 2) ERR("argv");

    int n = atoi(argv[1]);


    if(n < 1 || n > 4) ERR("argv");


    int buff[1024];
    pid_t pid;

    for(int i =0; i < n; i++){

        if((pid = fork()) < 0){
            ERR("pid");
        }
        buff[i] = pid;

        if(pid == 0){
            child_work();
            return EXIT_SUCCESS;
        }
        
    }



    static const int signals[] = {SIGUSR1, SIGUSR2, SIGPIPE, SIGINT};



    srand(getpid());
    int sig;


        struct timespec t = {0, 10*1000000};



    int buff2[1024];
    pid_t pid2;
    sethandler(term_handler, SIGTERM);

    while(work){

        if((pid2 = fork()) < 0){
            ERR("pid");
        }

        sig = rand() % (sizeof(signals)/sizeof(signals[0]));
        buff2[i] = pid2;

        

        if(pid2 == 0){
            donor_work(n, buff);
            return EXIT_SUCCESS;
        }

            nanosleep(&t, NULL);

            if(kill(buff2[i], signals[sig]) < 0) ERR("kill");

        printf("sending %d to %d\n", sig, buff2[i]);

        waitpid(buff2[i], NULL,  0);
        
    }


        while(wait(NULL)>0){

    }

    printf("Collection ended!\n");

    return EXIT_SUCCESS;
}