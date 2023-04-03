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


mqd_t desc = -1;


void client_work(mqd_t*client_desc,queue_info_t* queues);
void create_children(mqd_t* client_desc,queue_info_t* queues);
void create_server_queues(queue_info_t queues[]);
void destroy_server_queues(queue_info_t queues[]);



int main(int argc, char** argv)
{   
    queue_info_t queues[QUEUE_COUNT];
    mqd_t client_desc;
    for(int i = 0; i<QUEUE_COUNT; i++)
    {
        queues[i].d = atoi(argv[1+i]);
    }

    create_server_queues(queues);
    create_children(&client_desc,queues);

    char buf[MAX_BUFF];
    char num_buff[MAX_BUFF];
    //czytaj dane z pierwszej kolejki
    if(TEMP_FAILURE_RETRY(mq_receive(queues[0].desc,buf,MAX_BUFF,NULL)) < 1) ERR("mq_receibe");

    fprintf(stdout, "%s\n",buf);

    //find starting point
    int ind = 0;
    while(buf[ind++] != '-') ;

    int base_ind = ind;
    int count = 0;
    int number = 0;
    while(buf[ind] != ' ' && buf[ind] != '\n')
    {
        ind++;
    }
    count = ind - base_ind;

    strncpy(num_buff,buf + base_ind,count);

    number  = atoi(num_buff);
    printf("[%d]\n",number);

    //clear buff
    int c = snprintf(buf, MAX_BUFF, "liczba %d", number);
    if(number % queues[0].d == 0)
    {
        snprintf(buf + c, MAX_BUFF - c,"jest podzielna przez %d \n",queues[0].d);

    }
    else 
    {
        snprintf(buf + c, MAX_BUFF - c,"nie jest podzielna przez %d \n",queues[0].d);

    }

        printf(" d prosciesie rodzicyu : %d\n",desc);
    if(TEMP_FAILURE_RETRY(mq_send(desc,buf,MAX_BUFF,1)) == -1 ) ERR("mq_sendk");

    destroy_server_queues(queues);

    while (wait(NULL) > 0) ;

    return EXIT_SUCCESS;


    
    
}

void create_server_queues(queue_info_t queues[])
{
    pid_t pid = getpid();

   // int pid_len = (int)((ceil(log10(pid))+1)*sizeof(char));
    //int d_len = -1;
   // int len;
    char name[MAX_BUFF];

    struct mq_attr attr;
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MAX_BUFF;
    for(int i =0; i<QUEUE_COUNT; i++)
    {
        //d_len = (int)((ceil(log10(d_tab[i]))+1)*sizeof(char));
       // len = d_len + pid_len + 2;

        snprintf(name, MAX_BUFF , "/%d_%d", pid,queues[i].d);
        if(mq_unlink(name) && errno != ENOENT) ERR("mq_unlink");

        if((queues[i].desc = mq_open(name,O_RDWR | O_CREAT, 0600,&attr)) == (mqd_t) - 1 ) ERR("mq_open");

        fprintf(stdout, "created %s with divisor [%d]\n",name,queues[i].d);

    }

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


void create_children(mqd_t* client_desc, queue_info_t* queues)
{
    switch (fork()) 
    {
        case 0:
        {
            client_work( client_desc,queues);
            exit(EXIT_SUCCESS);
        }
        case -1:
        {
            ERR("fork");
        }
    }
}

void client_work(mqd_t* client_desc,queue_info_t* queues)
{
    char name[MAX_BUFF];
    char buf[MAX_BUFF];
    snprintf(name, MAX_BUFF , "/%d", getpid());

    int name_size = (int)((ceil(log10(getpid()))+1)*sizeof(char)) + 1;
    struct mq_attr attr;
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MAX_BUFF;

    if(mq_unlink(name) < 0 && errno != ENOENT) ERR("queue cant unlink");

    if((desc = mq_open(name,O_RDWR | O_CREAT,0600,&attr)) == (mqd_t) -1 ) ERR("mq_open");

    //mqd_t desc = *client_desc;

    printf("%d",desc );
    printf("KLIENT TWORZY KOLEJKE [%s]\n",name);
    snprintf(buf, MAX_BUFF,"%s-",name);
    //send from stdin
    int c = read(0,buf+name_size ,MAX_BUFF);
    //int8_t ni = buf[0];

    if(TEMP_FAILURE_RETRY(mq_send(queues[0].desc,(char*)&buf,MAX_BUFF,1)) == -1) ERR("mq_sned");

        printf(" d prosciesie xiekcu : %d\n",desc);


    if(TEMP_FAILURE_RETRY(mq_receive(desc,buf,MAX_BUFF,NULL)) < 1) ERR("mq_receie");

    fprintf(stdout,"%s\n",buf);



    sleep(3);





    mq_close(*client_desc);
    if(mq_unlink(name) < 0 && errno != ENOENT) ERR("queue cant unlink");
}