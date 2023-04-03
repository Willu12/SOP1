#define _GNU_SOURCE
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


int main(int argc, char** argv)
{
    char name[MAX_BUFF-1];
    char buf[MAX_BUFF];
    mqd_t desc;
    mqd_t serv_desc;
    snprintf(name, MAX_BUFF-1 , "/%d", getpid());

    char* server_queue_name = argv[1];

    int name_size = (int)((ceil(log10(getpid()))+1)*sizeof(char)) + 1;
    struct mq_attr attr;
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MAX_BUFF;

    if(mq_unlink(name) < 0 && errno != ENOENT) ERR("queue cant unlink");

    if((desc = mq_open(name,O_RDWR | O_CREAT,0600,&attr)) == (mqd_t) -1 ) ERR("mq_open");



    printf("KLIENT TWORZY KOLEJKE [%s]\n",server_queue_name);


    //open server queue
    if((serv_desc = mq_open(server_queue_name,O_RDWR|O_CREAT,0600,&attr)) == (mqd_t) - 1) ERR("mq_open");


    snprintf(buf, MAX_BUFF,"%s-",name);

      printf("OPENED%s]\n",server_queue_name);

    //send from stdin
    read(0,buf+name_size ,MAX_BUFF);

    //int8_t ni = buf[0];/*
    if(TEMP_FAILURE_RETRY(mq_send(serv_desc,(char*)&buf,MAX_BUFF,1)) == -1) ERR("mq_sned");
    printf("SENT%s]\n",server_queue_name);

    if(TEMP_FAILURE_RETRY(mq_receive(desc,buf,MAX_BUFF,NULL)) < 1) ERR("mq_receie");

    fprintf(stdout,"%s\n",buf);
    mq_close(desc);
    if(mq_unlink(name) < 0 && errno != ENOENT) ERR("queue cant unlink");
}