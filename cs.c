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

typedef struct pig
{
    int i;
    unsigned int seed;
    int* tab;
    pthread_mutex_t *mxcell;
}   pig_t;
void usage(char *name)
{
	fprintf(stderr, "USAGE: %s  0 < N < 101\n", name);
	exit(EXIT_FAILURE);
}

void printTab(int* tab,int N)
{
    printf("[");
    for(int i = 0; i<N; i++)
    {
        printf(" %d",tab[i]);
    }
    printf(" ]\n");
}

void* pigLife(void * voidArg)
{
    pig_t* arg = voidArg;
    struct timespec rq_t;
    rq_t.tv_sec = 0;
    while(1)
    {
        rq_t.tv_nsec = (rand_r(&arg->seed) %41 + 10) * 1000 * 1000;
        nanosleep(&rq_t,NULL);
        pthread_mutex_lock(arg->mxcell);
        arg->tab[arg->i]++;
        pthread_mutex_unlock(arg->mxcell);
    }

    return NULL;
}

int find_b(int * tab,int n,int b,pthread_mutex_t* mxs)
{
    for(int i = 0; i<n; i++)
    {
        pthread_mutex_lock(&mxs[i]);
        if(tab[i] == b)
        {
            pthread_mutex_unlock(&mxs[i]);
            return i;
        }
        pthread_mutex_unlock(&mxs[i]);
    }
    return -1;
}

int max_arr(int*tab,int n,pthread_mutex_t *mxs)
{
    int max = tab[0];
    for(int i =1; i<n; i++)
    {
        pthread_mutex_lock(&mxs[i]);

        if(tab[i] > max)    max = tab[i];
                
        pthread_mutex_unlock(&mxs[i]);
    }
    return max;
}

int main(int argc, char** argv)
{
    if(argc != 3)   usage(argv[0]);
    int N = atoi(argv[1]);
    int T = atoi(argv[2]);
    if( N > 100 ) usage(argv[0]);

    srand(time(NULL));

    int* tab = (int*)malloc(sizeof(int) * N);
    pthread_t* tids = (pthread_t*)malloc(sizeof(pthread_t) * N);
    pig_t* pigArgs = (pig_t*)malloc(sizeof(pig_t) * N);
    pthread_mutex_t* mxCells = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)* N);
    if(!tab) ERR("malloc");
    if(!tids) ERR("malloc");
    if(!pigArgs) ERR("malloc");
    if(!mxCells) ERR("mallocs");
    for(int i =0; i<N; i++)
    {
        if(pthread_mutex_init(&mxCells[i],NULL)) ERR("cant init");
        tab[i] = rand()%100 + 1;
    }
    printTab(tab,N);
    unsigned int seed1 = rand();

    //creating therads
    for(int i  =0; i<N; i++)
    {
        pigArgs[i].i = i;
        pigArgs[i].seed = rand();
        pigArgs[i].tab = tab;
        pigArgs[i].mxcell = &mxCells[i];
        if(pthread_create(&tids[i],NULL,pigLife,&pigArgs[i])) ERR("craete");
    }

    struct timespec start,current,rq;
    rq.tv_sec = 0;
    int ind;
    int b;
    if(clock_gettime(CLOCK_REALTIME,&start)) ERR("gettitme");
    do
    {
        rq.tv_nsec = (rand_r(&seed1) % 401 + 100) * 1000 * 1000;
        nanosleep(&rq,NULL);
        b = rand_r(&seed1) % max_arr(tab,N,mxCells) + 1;
        if((ind = find_b(tab,N,b,mxCells)) >=0)
        {
            pthread_mutex_lock(&mxCells[ind]);
            tab[ind] = 0;
            pthread_mutex_unlock(&mxCells[ind]);
            for(ind-1; ind >=0; ind--)
            {
                pthread_mutex_lock(&mxCells[ind]);
                if(tab[ind] < b && tab[ind] > 0)
                {
                    if(pthread_cancel(tids[ind])) ERR("cancel"); 
                    tab[ind] = 0;
                    printf("rozjebane%d\n",ind);
                }
                pthread_mutex_unlock(&mxCells[ind]);    
            }
        }
        if (clock_gettime(CLOCK_REALTIME, &current)) ERR("gettime");
 
    }while(ELAPSED(start,current) < T);

    for(int i =0; i<N; i++)
    {
        if(pthread_cancel(tids[i])) ERR("cancel");
    }
    //waiting for threads
    for(int i =0; i<N; i++)
    {
        if(pthread_join(tids[i],NULL)) ERR("join");
    }
    

    printTab(tab,N);

    //free(mxCells);
    free(tab);
    free(tids);
    free(pigArgs);
    for(int i =0; i<N; i++)
    {
        pthread_mutex_destroy(&mxCells[i]);
    }
    free(mxCells);
    return EXIT_SUCCESS;
}