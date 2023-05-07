#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_LEN 3
# define MAX_NUM 1000
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#ifndef TEMP_FAILURE_RETRY
# define TEMP_FAILURE_RETRY(expression) \
  (__extension__                                                              \
    ({ long int __result;                                                     \
       do __result = (long int) (expression);                                 \
       while (__result == -1L && errno == EINTR);                             \
       __result; }))
#endif

void usage(char *name)
{
	fprintf(stderr, "USAGE: %s socket port\n", name);
}
int sethandler(void (*f)(int), int sigNo)
{
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1 == sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

int make_socket(void)
{
	int sock;
	if((sock = socket(PF_INET,SOCK_STREAM,0)) < 0) ERR("socket");
	return sock;
}

struct sockaddr_in make_addres(char* address, char* port)
{
	int ret;
	struct sockaddr_in addr;
	struct addrinfo* result;
	struct addrinfo hints = {};
	hints.ai_family = AF_INET;

	if((ret = getaddrinfo(address,port,&hints,&result)))
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	addr = *(struct sockaddr_in*)(result->ai_addr);
	freeaddrinfo(result);
	return addr;
}

int connect_socket(char * name, char* port)
{
	struct sockaddr_in addr;
	int socketfd;
	socketfd = make_socket();
	addr = make_addres(name,port);
	if(connect(socketfd,(struct sockaddr*) &addr,sizeof(struct sockaddr_in)) < 0)
	{
		if(errno != EINTR)	ERR("connect");
		else
		{
			fd_set wfds;
			int status;
			socklen_t size = sizeof(int);
			FD_ZERO(&wfds);
			FD_SET(socketfd,&wfds);

			if(TEMP_FAILURE_RETRY(select(socketfd +1, NULL, &wfds,NULL,NULL)) < 0) ERR("select");
			if(getsockopt(socketfd,SOL_SOCKET,SO_ERROR,&status,&size) < 0) ERR("getsocktop");

			if(0 != status) ERR("connect");
		}
	}
	return socketfd;
}


ssize_t bulk_read(int fd, char *buf, size_t count)
{
	int c;
	size_t len = 0;
	do {
		c = TEMP_FAILURE_RETRY(read(fd, buf, count));
		if (c < 0)
			return c;
		if (0 == c)
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

void prepare_request(int32_t data[MAX_LEN])
{
    int32_t random;
	for(int i =0; i<MAX_LEN; i++)
    {
        random = rand() % MAX_NUM +1;
        data[i] = htonl(random);
    }
}

int main(int argc, char **argv)
{
    srand(time(NULL));
	int fd;
    int32_t answer = -1;
	int32_t data[MAX_LEN];
	if (argc != 3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (sethandler(SIG_IGN, SIGPIPE))	ERR("Seting SIGPIPE:");
	fd = connect_socket(argv[1], argv[2]);

    for(int i =0; i<3; i++)
    {
        prepare_request(data);
        fprintf(stderr,"wysylam liczbe\n");
	    if (bulk_write(fd, (char *)data, sizeof(int32_t[MAX_LEN])) < 0) ERR("write:");

	    if (bulk_read(fd, (char*)&answer, sizeof(int32_t)) < 1)  ERR("read:");
	    fprintf(stderr,"otzymano liczbe %d \n",ntohl(answer));
        sleep(3);

    }
	
	if (TEMP_FAILURE_RETRY(close(fd)) < 0)  ERR("close");
	return EXIT_SUCCESS;
}