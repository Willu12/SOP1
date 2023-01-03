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


typedef struct generArgs
{
    unsigned int seed;
    pthread_t* ptids;
    pthread_mutex_t *pmxseed;
    pthread_mutex_t *pmxtids;
}   generArgs_t;

typedef struct superArgs
{
    unsigned int t;
    int n;
    pthread_t* tids;
    pthread_mutex_t *pmxtids;
} superArgs_t;
void usage(char *name)
{
	fprintf(stderr, "USAGE: %s 0<n\n", name);
	exit(EXIT_FAILURE);
}
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

void* supervisorLife(void* voidArg)
{
    superArgs_t *Arg = voidArg;
    int t = Arg->t;
    int remsec = t /1000;
    int remnano =  (t%1000) * 1000 * 1000;
    struct timespec rem_time = {remsec,remnano};
    int count;
    int deleted;
    while(last_signal != SIGQUIT)
    {
        count =0;
        nanosleep(&rem_time,NULL);
        pthread_mutex_lock(Arg->pmxtids);
        for(int i = 0; i<2*Arg->n; i++)
        {
            if(Arg->tids[i] != 0) count++;
        }
        if(count > Arg->n)
        {
            deleted = 0;
            for(int i =0; i<2*Arg->n; i++)
            {
                if(Arg->tids[i] != 0)
                {
                    if(pthread_cancel(Arg->tids[i])) ERR("cancel");
                    deleted++;
                    Arg->tids[i] = 0;
                    if(deleted > count/2 )  break;
                }
            }
        }

        pthread_mutex_unlock(Arg->pmxtids);
        //printf("count:%d \n",count);

        
    }
    printf("zamykam nadzocrce\n");
    return NULL;
}

void* generatorLife(void* voidArg)
{
    pthread_t tid = pthread_self();
    generArgs_t *Arg = voidArg;
    int s;
    //s = 3000;
    int remsec;
    int remnano;
    //printf("nano:%d",remsec);
    //printf("%d\n",Arg->pmxseed->__sig);

    struct timespec rem_time;

    while(last_signal != SIGQUIT)
    {
        pthread_mutex_lock(Arg->pmxseed);
        s = rand_r(&Arg->seed) % 901 + 100;
        pthread_mutex_unlock(Arg->pmxseed);
        remsec = s /1000;
        remnano =  (s%1000) * 1000 * 1000;
        rem_time.tv_nsec = remnano;
        rem_time.tv_sec = remsec;
        nanosleep(&rem_time,NULL);

        


        printf("%d\n",tid);
    }
    printf("zamykam generator\n");
    return NULL;
}
void create_new_thread(pthread_t *tids,int n,generArgs_t* genArg)
{
    int i;
    for(i=0; i<2*n;i++)
    {
       // printf("[%d]",tids[i]);   
        if(tids[i] == 0)   break;

    }
    if(i<2*n)
    {
        printf(" empty index: %d\n",i);
        if(pthread_create(&(tids[i]),NULL,generatorLife,genArg)) ERR("pthread_create");
    }
    else
    {
        printf("nie mam miejsca\n");
    }
}


int main(int argc, char** argv)
{
    if(argc < 3) usage(argv[0]);
    int n,t;
    n = atoi(argv[1]);
    t = atoi(argv[2]);
    if( n > 20 || n<1 || t > 5000 || t< 100) usage(argv[0]);

    pthread_t *tids = (pthread_t*)malloc(sizeof(pthread_t) * 2 * n);
    pthread_t supervisortid;
    pthread_mutex_t mxseed = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mxtids = PTHREAD_MUTEX_INITIALIZER;

    srand(time(NULL));
    generArgs_t genArg ={rand(),tids, &mxseed,&mxtids};
    superArgs_t supArg ={t,n,tids,&mxtids};


    for(int i = 0; i< 2*n; i++)
    {
        tids[i] = 0;
    }

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask,SIGINT);

    sethandler(sig_handler,SIGINT);
    sethandler(sig_handler,SIGQUIT);
    if(pthread_sigmask(SIG_BLOCK,&mask,&oldmask)) ERR("pthread_sigmask");

    pthread_create(&supervisortid,NULL,supervisorLife,&supArg);

    while(last_signal != SIGQUIT)
    {
        sigsuspend(&oldmask);
        if(last_signal == SIGINT)
        {
            create_new_thread(tids,n,&genArg);
        }
    }
    for(int i =0; i< 2*n; i++)
    {
        if(tids[i] != 0)
        {
            if(pthread_join(tids[i],NULL)) ERR("join");
        }
    }
    if(pthread_join(supervisortid,NULL)) ERR("join");

    return EXIT_SUCCESS;
}

