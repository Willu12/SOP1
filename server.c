#define _GNU_SOURCE
#include <signal.h>
#include <bits/types/sigevent_t.h>
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define MAX_MSG 10
#define MAX_BUFF 100
#define QUEUE_COUNT 4


#define ERR(source)                                                                                                    \
	(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct queue_info_t
{
    mqd_t desc;
    int d;
}queue_info_t;

void create_server_queues(queue_info_t queues[]);
void destroy_server_queues(queue_info_t queues[]);
void mq_handler(int sig, siginfo_t *info, void *p);
void sigint_handler(int sig, siginfo_t *info, void *p);
void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo);


volatile sig_atomic_t last_sig = 0;

int main(int argc, char** argv)
{   
    queue_info_t queues[QUEUE_COUNT];
    for(int i = 0; i<QUEUE_COUNT; i++)
    {
        queues[i].d = atoi(argv[1+i]);
    }

    sethandler(mq_handler, SIGRTMIN);
    sethandler(sigint_handler, SIGINT);
    
    create_server_queues(queues);
    

    while(last_sig != SIGINT)
    {
        printf("woken\n");
        pause();
    }
    
    destroy_server_queues(queues);


    return EXIT_SUCCESS;   
}

void create_server_queues(queue_info_t queues[])
{
    pid_t pid = getpid();
    char name[MAX_BUFF];
    static struct sigevent noti[QUEUE_COUNT];

    struct mq_attr attr;
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MAX_BUFF;
    for(int i =0; i<QUEUE_COUNT; i++)
    {
        snprintf(name, MAX_BUFF , "/%d_%d", pid,queues[i].d);
        if(mq_unlink(name) && errno != ENOENT) ERR("mq_unlink");

        if((queues[i].desc = mq_open(name,O_RDWR | O_CREAT, 0600,&attr)) == (mqd_t) - 1 ) ERR("mq_open");

        //notficiations
        noti[i].sigev_notify = SIGEV_SIGNAL;
        noti[i].sigev_signo = SIGRTMIN;
        noti[i].sigev_value.sival_ptr = &queues[i];
        if(mq_notify(queues[i].desc, &noti[i]) < 0) ERR("mq_notify");

        fprintf(stdout, "created %s with divisor [%d]\n",name,queues[i].d);
    }
    //create notifications
}
void destroy_server_queues(queue_info_t queues[])
{
    char name[MAX_BUFF];

    for(int i =0; i< QUEUE_COUNT; i++)
    {

        snprintf(name, MAX_BUFF , "/%d_%d", getpid(),queues[i].d);
        mq_close(queues[i].desc);
        mq_unlink(name);
        if(mq_unlink(name) < 0 && errno != ENOENT) ERR("queue cant unlink");

    }
}

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_sigaction = f;
	act.sa_flags = SA_SIGINFO;
	if (-1 == sigaction(sigNo, &act, NULL))
		ERR("sigaction");
}

void mq_handler(int sig, siginfo_t *info, void *p)
{
    char buf[MAX_BUFF];
    char client_name[MAX_BUFF];
    char num_buff[10];
    mqd_t client_desc;
    queue_info_t* queue_info = (queue_info_t*)info->si_value.sival_ptr;

    struct sigevent noti;

    noti.sigev_signo = SIGRTMIN;
    noti.sigev_value.sival_ptr = &queue_info->desc;
    noti.sigev_notify = SIGEV_SIGNAL;
    if(mq_notify(queue_info->desc, &noti) < 0) ERR("mq_notify");

    if(TEMP_FAILURE_RETRY(mq_receive(queue_info->desc,buf,MAX_BUFF,NULL)) < 1) ERR("mq_receibe");

    fprintf(stdout, "%s\n",buf);

    //get name

    //find starting point
    int ind = 0;
    while(buf[ind++] != '-') ;
    strncpy(client_name,buf,ind-1);
    client_name[ind-1] = '\0';
    printf("%s\n",client_name);

    //open client
    if((client_desc = TEMP_FAILURE_RETRY(mq_open(client_name,O_RDWR))) == -1) ERR("mq_open");


    int base_ind = ind;
    int count = 0;

    while(buf[ind] != ' ' && buf[ind] != '\n')
    {
        ind++;
    }
    count = ind - base_ind;
    strncpy(num_buff,buf + base_ind,count);

    int number  = atoi(num_buff);
    printf("[%d]\n",number);

    //clear buff
    int c = snprintf(buf, MAX_BUFF, "liczba %d", number);

    if( number % queue_info->d == 0)   snprintf(buf + c, MAX_BUFF - c," jest podzielna przez %d \n",queue_info->d);
    else    snprintf(buf + c, MAX_BUFF - c," nie jest podzielna przez %d \n",queue_info->d);

    if(TEMP_FAILURE_RETRY(mq_send(client_desc,buf,MAX_BUFF,1)) == -1 ) ERR("mq_send");
}

void sigint_handler(int sig, siginfo_t *info, void *p)
{
    last_sig = SIGINT;
}