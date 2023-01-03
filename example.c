#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>


#define MAXLINE 4096
#define ELAPSED(start, end) ((end).tv_sec - (start).tv_sec) + (((end).tv_nsec - (start).tv_nsec) * 1.0e-9)
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;


void sethandler(void (*f)(int), int sigNo)
{
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1 == sigaction(sigNo, &act, NULL))
		ERR("sigaction");
}

void sig_handler(int sig)
{
	//printf("[%d] received signal %d\n", getpid(), sig);
	last_signal = sig;
}

typedef struct threadstat
{
    pthread_t tid;
    unsigned int seed;
    int *L;
    int* doneThreads;
    
    pthread_mutex_t *pmxDoneThreads;

}   threadstat_t;


void usage(char * name);
void* threadLife(void* voidArg);


int main(int argc, char** argv)
{
    if(argc == 1) usage(argv[0]);
    int n = atoi(argv[1]);
    if(n <= 0) usage(argv[0]);

    int L = 1;
    int donethreads = 0;
    pthread_mutex_t mxDoneThreads = PTHREAD_MUTEX_INITIALIZER;

    //sigset_t mask,oldmask;
    //sigemptyset(&mask);
    //sigaddset(&mask,SIGINT);

    //if(pthread_sigmask(SIG_BLOCK,&mask,&oldmask))   ERR("pthread_sigmask");
    
    sethandler(sig_handler,SIGINT);

    threadstat_t *tstats = (threadstat_t*)malloc(sizeof(threadstat_t) * n);
    if(!tstats) ERR("malloc");

    srand(time(NULL));
    for(int i =0; i<n; i++)
    {
        tstats[i].seed = rand();
        tstats[i].L = &L;
        tstats[i].pmxDoneThreads = &mxDoneThreads;
        tstats[i].doneThreads = &donethreads;
    }

    // tworzenie watkow
    for(int i = 0; i<n; i++)
    {
        if(pthread_create(&(tstats[i].tid),NULL,threadLife,&tstats[i])) ERR("create");
    }

    struct timespec rq = {0,1000 * 1000* 100};
    // odliczanie
    while(last_signal != SIGINT)
    {
        nanosleep(&rq,NULL);
        while(donethreads < n)
        {
            ;
        }
        donethreads = 0;
        
        L++;

    }
    L = -1;

    // czekanie na watki
    for(int i = 0; i<n; i++)
    {
        if(pthread_join(tstats[i].tid,NULL)) ERR("join");
    }

    exit(EXIT_SUCCESS);
}

void usage(char *name)
{
	fprintf(stderr, "USAGE: %s 0<n\n", name);
	exit(EXIT_FAILURE);
}

void* threadLife(void* voidArg)
{
    threadstat_t *args = voidArg;
    int m = rand_r(&args->seed) % 99 +2;
    //printf("%d\n",m);
    int L_old = 0;

    while(*args->L > 0)
    {
        if(L_old != *args->L)
        {   
            pthread_mutex_lock(args->pmxDoneThreads);
            L_old = *args->L;
            if(*args->L % m == 0 )
            {
                printf("%d jest podzielne przez %d\n",*args->L,m);
            }
            (*args->doneThreads) += 1;
            pthread_mutex_unlock(args->pmxDoneThreads);
        }
    }
    return NULL;
}