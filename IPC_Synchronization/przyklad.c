#define _GNU_SOURCE
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define BACKLOG 3
#define CHUNKSIZE 500
#define NMMAX 30
#define THREAD_NUM 3
#define START_LETTER 'A'
#define ERRSTRING "No such file or directory\n"
volatile sig_atomic_t work = 1;
volatile sig_atomic_t thread_work = 1;

typedef struct {
	int id;
	int *idlethreads;
	int *socket;
	int *condition;
	pthread_cond_t *cond;
	pthread_mutex_t *mutex;
    sem_t *semaphores;
} thread_arg;

void siginthandler(int sig)
{
	work = 0;
    thread_work = 0;
}

void usage(char *name)
{
	fprintf(stderr, "USAGE: %s port workdir\n", name);
	exit(EXIT_FAILURE);
}

void sethandler(void (*f)(int), int sigNo)
{
	struct sigaction act;
	memset(&act, 0x00, sizeof(struct sigaction));
	act.sa_handler = f;

	if (-1 == sigaction(sigNo, &act, NULL))
		ERR("sigaction");
}

ssize_t bulk_read(int fd, char *buf, size_t count)
{
	int c;
	size_t len = 0;
	do {
		c = TEMP_FAILURE_RETRY(read(fd, buf, count));
		if (c < 0)
			return c;
		if (c == 0)
			return len;
		buf += c;
		len += c;
		count -= c;
	} while (count > 0);
	return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count)
{
	int c;
	size_t len = 0;
	do {
		c = TEMP_FAILURE_RETRY(write(fd, buf, count));
		if (c < 0)
			return c;
		buf += c;
		len += c;
		count -= c;
	} while (count > 0);
	return len;
}

int make_socket(int domain, int type)
{
	int sock;
	sock = socket(domain, type, 0);
	if (sock < 0)
		ERR("socket");
	return sock;
}

void cleanup(void *arg)
{
	pthread_mutex_unlock((pthread_mutex_t *)arg);
}

void send_control_msg(int clientfd)
{
    char buffer[CHUNKSIZE];
    memset(buffer,0,CHUNKSIZE);
    strncpy(buffer,"control msg",CHUNKSIZE);
    if(TEMP_FAILURE_RETRY(send(clientfd,buffer,CHUNKSIZE,0)) == -1)
    {
        if(errno == EPIPE) return;
        ERR("send");
    } 
}

void iterate(int clientfd, thread_arg *targ)
{
    sem_wait(&targ->semaphores[targ->id - 1]);
    fprintf(stderr,"TERMINATING %c",START_LETTER + targ->id);
    sem_post(&targ->semaphores[targ->id % 3]);
    //wysyla litere o
}

void *threadfunc(void *arg)
{
	int clientfd;
	thread_arg targ;
	memcpy(&targ, arg, sizeof(targ));
	while (1) 
    {
		pthread_cleanup_push(cleanup, (void *)targ.mutex);
		if (pthread_mutex_lock(targ.mutex) != 0)    ERR("pthread_mutex_lock");
		(*targ.idlethreads)++;
		while (!*targ.condition && work)
			if (pthread_cond_wait(targ.cond, targ.mutex) != 0)  ERR("pthread_cond_wait");

		*targ.condition = 0;
		if (!work)  pthread_exit(NULL);
        if(*targ.idlethreads == 3) thread_work = 1;
        //fprintf(stderr,"idle processes: %d",*targ.idlethreads);

		(*targ.idlethreads)--;
		clientfd = *targ.socket;
		pthread_cleanup_pop(1);
        
        while(thread_work)
        {
            if(*targ.idlethreads == 0) thread_work =0;
            send_control_msg(clientfd);
            sleep(1);
        }
        iterate(clientfd,&targ);

	}
	return NULL;
}

void init(pthread_t *thread, thread_arg *targ, pthread_cond_t *cond, pthread_mutex_t *mutex, int *idlethreads, int *socket, int *condition, sem_t* semaphores)
{
	int i;
	for (i = 0; i < THREAD_NUM; i++) 
    {
		targ[i].id = i + 1;
		targ[i].cond = cond;
		targ[i].mutex = mutex;
		targ[i].idlethreads = idlethreads;
		targ[i].socket = socket;
		targ[i].condition = condition;
        targ[i].semaphores = semaphores;
		if (pthread_create(&thread[i], NULL, threadfunc, (void *)&targ[i]) != 0)    ERR("pthread_create");
	}
}

int bind_tcp_socket(uint16_t port)
{
	struct sockaddr_in addr;
	int socketfd, t = 1;
	socketfd = make_socket(PF_INET, SOCK_STREAM);
	memset(&addr, 0x00, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))  ERR("setsockopt");
	if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) ERR("bind");
	if (listen(socketfd, BACKLOG) < 0)  ERR("listen");
	return socketfd;
}

int add_new_client(int sfd)
{
	int nfd;
	if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0) 
    {
		if (EAGAIN == errno || EWOULDBLOCK == errno)    return -1;
		ERR("accept");
	}
	return nfd;
}

void dowork(int socket, pthread_t *thread, thread_arg *targ, pthread_cond_t *cond, pthread_mutex_t *mutex,
	    int *idlethreads, int *cfd, sigset_t *oldmask, int *condition)
{
	int clientfd;
	fd_set base_rfds, rfds;
	FD_ZERO(&base_rfds);
	FD_SET(socket, &base_rfds);
	while (work) 
    {
        //fprintf(stderr,"working :)\n");
		rfds = base_rfds;
		if (pselect(socket + 1, &rfds, NULL, NULL, NULL, oldmask) > 0) 
        {
			if ((clientfd = add_new_client(socket)) == -1)  continue;
			if (pthread_mutex_lock(mutex) != 0) ERR("pthread_mutex_lock");
			if (*idlethreads == 0) 
            {
				if (TEMP_FAILURE_RETRY(close(clientfd)) == -1)  ERR("close");
				if (pthread_mutex_unlock(mutex) != 0)   ERR("pthread_mutex_unlock");
			} 
            else 
            {
				*cfd = clientfd;
				if (pthread_mutex_unlock(mutex) != 0)   ERR("pthread_mutex_unlock");
				*condition = 1;
				if (pthread_cond_signal(cond) != 0) ERR("pthread_cond_signal");
			}
		} 
        else 
        {
			if (EINTR == errno) continue;
			ERR("pselect");
		}
	}
}
int main(int argc, char **argv)
{
	int i, condition = 0, socket, new_flags, cfd, idlethreads = 0;
	pthread_t thread[THREAD_NUM];
	thread_arg targ[THREAD_NUM];
    sem_t semaphores[THREAD_NUM];
    for( i =0; i< THREAD_NUM; i++)
    {
        if(sem_init(&(semaphores[i]),0,0)) ERR("seminit");
    }
    sem_post(&semaphores[0]);
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	sigset_t mask, oldmask;
	if (argc != 2)  usage(argv[0]);
	sethandler(SIG_IGN, SIGPIPE);
	sethandler(siginthandler, SIGINT);
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	socket = bind_tcp_socket(atoi(argv[1]));
	new_flags = fcntl(socket, F_GETFL) | O_NONBLOCK;
	if (fcntl(socket, F_SETFL, new_flags) == -1)    ERR("fcntl");
	init(thread, targ, &cond, &mutex, &idlethreads, &cfd, &condition,semaphores);
	dowork(socket, thread, targ, &cond, &mutex, &idlethreads, &cfd, &oldmask, &condition);
	if (pthread_cond_broadcast(&cond) != 0) ERR("pthread_cond_broadcast");
	for (i = 0; i < THREAD_NUM; i++)
		if (pthread_join(thread[i], NULL) != 0) ERR("pthread_join");
	if (TEMP_FAILURE_RETRY(close(socket)) < 0)  ERR("close");
	return EXIT_SUCCESS;
}