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

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

sig_atomic_t lastSig;
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

void usage(int argc, char* argv[])
{
    printf("%s p k \n", argv[0]);
    printf("\tp - path to file with stones\n");
    printf("\t0 < k < 8 - number of child processes\n");
    exit(EXIT_FAILURE);
}
void signal_handler(int signal) { lastSig = signal; }
void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void child_work(int num, char* stone, sigset_t oldmask)
{
    while (sigsuspend(&oldmask))
    {
        if (lastSig == SIGUSR1)
            break;
    }

    printf("PROCESS with index %d terminates\n", num);
}

ssize_t read_stone(int fd, char* buf)
{
    ssize_t count = 0;
    char c;
    int lnum = 0;
    int r;
    while (lnum <= 8)
    {
        if ((r = read(fd, &c, 1)) == -1)
            ERR("read");

        if (r == 0)
            break;

        buf[count++] = c;
        if (c == '\n')
            lnum++;
    }
    buf[count] = '\0';
    return count;
}

void create_children(int k, char* p, sigset_t oldmask)
{
    struct stat s;

    if (stat(p, &s) < 0)
        ERR("stat: ");

    int fd;
    if ((fd = open(p, O_RDONLY)) == -1)
        ERR("open");

    // char* chunk = malloc((512) * sizeof(char));
    // char** stones = malloc(k * sizeof(char) * 512);
    // char** stones = malloc(k*sizeof(chunk));

    /*for (int j = 0; j < k; j++)
    {
        char* chunk = malloc((512) * sizeof(char));
        read_stone(fd, chunk);
        stones[j] = chunk;
        // printf("%s", chunk);
        free(chunk);
    }*/

    for (int i = 0; i < k; i++)
    {
        // int read;
        // int num_st = 0;
        // while(num_st <= 8){      }
        // while(c != '\n'){
        //   if ((read = bulk_read(fd, chunk, 1)) == -1) ERR("bulk_read");
        // printf("%s", chunk);
        //}
        /*for(int j = 0; j < 8; j++){
            char* line
            ssize_t l;
            size_t size = 0;
            //if((l = getline(*l, &size, fd)) == -1) ERR("getline");
            //chunk =
        }*/
        // if ((read = bulk_read(fd, chunk, fsize / n + 1)) == -1) ERR("bulk_read");
        char* chunk = malloc((512) * sizeof(char));
        // printf("%s", chunk);
        pid_t pid = fork();
        read_stone(fd, chunk);
        //printf("%s", chunk);
        if (pid < 0)
        {
            ERR("fork");
        }
        else if (pid == 0)
        {
            // printf("Child process %d\n", num);
            child_work(i, chunk, oldmask);
            exit(EXIT_SUCCESS);
        }

        free(chunk);
    }
    // free(stones);
    kill(0, SIGUSR1);
    close(fd);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);

    char* p = argv[1];
    int k = atoi(argv[2]);

    if (k > 8 || k <= 0)
        usage(argc, argv);

    pid_t pp = getppid();
    printf("Parent process id: %d\n", pp);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    sethandler(signal_handler, SIGUSR1);
    sethandler(signal_handler, SIGINT);
    // sethandler(signal_handler, EINTR);

    create_children(k, p, oldmask);
    while (wait(NULL) > 0)
        ;

    return EXIT_SUCCESS;
}
