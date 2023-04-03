#define _GNU_SOURCE
#include <signal.h>
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#define LIFE_SPAN 10
#define MAX_NUM 10

#define ERR(source)                                                                                                    \
	(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))


volatile sig_atomic_t children_left = 0;

typedef struct queue_info_t
{
    mqd_t descriptor;
    int index;
    int n;
    mqd_t* desc_tab;
} queue_info_t;

void usage(void);
void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo);
void createQueues(int n,queue_info_t* queues);
void closeQueues(int n, queue_info_t* queues);
void ReadandWrite(int n, queue_info_t* queues);
static void tfunc(union sigval sv);

int main(int argc, char** argv)
{
    int n;
    if(argc != 2) usage();
    n = atoi(argv[1]);
    if(n< 1 || n >100) usage();
    queue_info_t* queues = (queue_info_t*)malloc(n* sizeof(queue_info_t));
    mqd_t* tab_desc = (mqd_t*)malloc(n * sizeof(mqd_t));
    for(int i =0; i<n; i++) queues[i].desc_tab = tab_desc;
    createQueues(n, queues);

    //
    struct sigevent* noti = (struct sigevent*)malloc(n * sizeof(struct sigevent));
    for(int i =0; i<n; i++)
    {
        noti[i].sigev_notify = SIGEV_THREAD;
        noti[i].sigev_notify_function = tfunc;
        noti[i].sigev_notify_attributes = NULL;
        noti[i].sigev_value.sival_ptr = &(queues[i]);   

        if(mq_notify(queues[i].descriptor, &(noti[i])) == -1 )    ERR("mq_notify");

    }
    //

    ReadandWrite(n, queues);


    sleep(2);

    closeQueues(n,queues);

    free(queues);   free(noti); free(tab_desc);

    return EXIT_SUCCESS;



}

static void tfunc(union sigval sv)
{
    struct mq_attr attr;
    unsigned int nr;
    int8_t ni;
    int c;
    void * buf;
    queue_info_t* queue_info =  (queue_info_t *) sv.sival_ptr;

    //renew notify
    static struct sigevent noti;
    noti.sigev_notify = SIGEV_THREAD;
    noti.sigev_notify_function = tfunc;
    noti.sigev_notify_attributes = NULL;
    noti.sigev_value.sival_ptr = queue_info;
    if(mq_notify(queue_info->descriptor, &noti)) ERR("mq_notify");

    if(mq_getattr(queue_info->descriptor, &attr) == -1) ERR("get_attr");

    int count = attr.mq_curmsgs;

    for(int i =0; i<count; i++)
    {
        buf = malloc(attr.mq_msgsize);
        if(buf == NULL) ERR("malloc");
        if((c = mq_receive(queue_info->descriptor, (char * )&ni, 1, &nr)) == -1) ERR("mq_receie");
    
        int num = ni - '0';
        nr++;
        if(num % nr == 0)
        {
            printf("kolejka nr: [%d]odczytano numer:  %d\n",nr -1 ,num);
            continue;
        }

        if(nr == queue_info->n) continue;

        if(mq_send(queue_info->desc_tab[nr],(char*)&ni,1,nr) == -1) ERR("mq_send");

        
    }
    //mq_notify(mqdes, const struct sigevent *notification)
   
    


}

void createQueues(int n,queue_info_t* queues)
{   
    char name[6] = {'/','q','0','\0'};

    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 1;
    for(int i =0; i<n; i++)
    {
        snprintf(name, (int)log10(i+1) + 4, "/q%d" , i+1);
        if(mq_unlink(name) < 0 && errno != ENOENT) ERR("queue already exists cant unlink");

        if((queues[i].descriptor = mq_open(name,O_RDWR | O_CREAT,0600,&attr)) == (mqd_t) -1 ) ERR("mq_open");

        queues[i].n = n;
        queues[i].index = i;
        queues[i].desc_tab[i] = queues[i].descriptor;

        printf("kolejka: [%s] z dzielnikiem [%d] \n",name,i+1);

    }
}

void closeQueues(int n, queue_info_t* queues)
{
    char name[6] = {'/','q','0','\0'};

    for(int i =0; i<n; i++)
    {
        snprintf(name, (int)log10(i+1) + 4, "/q%d" , i+1);
        mq_close(queues[i].descriptor);
        mq_unlink(name);
        if(mq_unlink(name) < 0 && errno != ENOENT) ERR("queue already exists cant unlink");

    }
}


void ReadandWrite(int n, queue_info_t* queues)
{
    //main thread reads from stdin and sends numbers to first queue
    srand(getpid());
    int index =  0;
    fprintf(stderr,"dupa\n");
    char ni[2];
    int c;
    int in;
 

    while((c = read(0,(char*)&ni,2)) > 0)
    {
        printf("gointg to send %c, to queue [%d]\n",ni[0],index);

        if(TEMP_FAILURE_RETRY(mq_send(queues[index].descriptor,(const char*)&ni[0],1,1))) ERR("mq_send");
        printf("sent %c, to queue [%d]\n",ni[0],index);
    }
}



void usage(void)
{
	fprintf(stderr, "USAGE: signals n k p l\n");
	fprintf(stderr, "100 =>n > 0 - number of children\n");
	exit(EXIT_FAILURE);
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