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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#ifndef TEMP_FAILURE_RETRY
# define TEMP_FAILURE_RETRY(expression) \
  (__extension__                                                              \
    ({ long int __result;                                                     \
       do __result = (long int) (expression);                                 \
       while (__result == -1L && errno == EINTR);                             \
       __result; }))
#endif

# define MAX_NUM 1000
# define MAX_TRIES 3
# define MAX_LEN 3
# define MAX_CLIENTS 5

#define BACKLOG 3

volatile sig_atomic_t do_work = 1;

void usage(char *name)
{
	fprintf(stderr, "USAGE: %s socket port\n", name);
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

int make_socket(int domain, int type)
{
	int sock;
	sock = socket(domain, type, 0);
	if (sock < 0)
		ERR("socket");
	return sock;
}


int bind_tcp_socket(uint16_t port)
{
	struct sockaddr_in addr;
	int socketfd, t = 1;
	socketfd = make_socket(PF_INET, SOCK_STREAM);
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
		ERR("setsockopt");
	if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		ERR("bind");
	if (listen(socketfd, BACKLOG) < 0)
		ERR("listen");
	return socketfd;
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
int sethandler(void (*f)(int), int sigNo)
{
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1 == sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

int add_new_client(int sfd)
{
	int nfd;
	if((nfd = TEMP_FAILURE_RETRY(accept(sfd,NULL,NULL))) < 0)
	{
		if(EAGAIN == errno || EWOULDBLOCK == errno ) return -1;
		ERR("accpet");
	}
	return nfd;
}
void sigint_handler(int sig)
{
	do_work = 0;
}

void communicate(int cfd,int32_t *current_max)
{
	ssize_t size;
	int32_t data[MAX_LEN];
    int32_t value;
	while((size = bulk_read(cfd, (char *)data, sizeof(int32_t[MAX_LEN]))) < 0 && errno == EAGAIN)   ;
	if (size == (int)sizeof(int32_t[MAX_LEN])) 
    {
		for(int i =0; i<MAX_LEN; i++)
        {
            value = ntohl(data[i]);
            fprintf(stderr,"dostalem liczbe: %d \n",value);
            *current_max = *current_max > value ? *current_max : value;
        }
        fprintf(stderr,"max_number: %d \n",*current_max);
        int32_t answer = htonl(*current_max);

		if (bulk_write(cfd, (char*)&answer, sizeof(int32_t))< 0 && errno != EPIPE) ERR("write:");

	}
}

int set_nonblock(int desc) {
    int oldflags = fcntl(desc, F_GETFL, 0);
    if (oldflags == -1)
        return -1;
    oldflags |= O_NONBLOCK;
    return fcntl(desc, F_SETFL, oldflags);
}



void doServer(int server_socket,int32_t *current_max)
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
    int clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; ++i)
        clients[i] = -1;
    while (do_work) {
        fd_set rdset;
        FD_ZERO(&rdset);
        FD_SET(server_socket, &rdset);

        int maxfd = server_socket;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] != -1) {
                FD_SET(clients[i], &rdset);
                if (clients[i] > maxfd)
                    maxfd = clients[i];
            }
        }

        printf("Calling select()!\n");
        int ret = pselect(maxfd + 1, &rdset, NULL, NULL, NULL,&oldmask);
        if (ret < 0) {
            if(errno == EINTR) continue;
            perror("select()");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(server_socket, &rdset)) {
            struct sockaddr client_addr;
            socklen_t client_addrlen = sizeof(client_addr);
            int client = accept(server_socket, &client_addr, &client_addrlen);

            if (client >= 0) {
                printf("Accepted new client!\n");
                set_nonblock(client);
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (clients[i] == -1) {
                        clients[i] = client;
                        break;
                    } else if (i == MAX_CLIENTS - 1) {
                        char neat_reply[] = "No space for you :(\n";
                        send(client, neat_reply, sizeof(neat_reply), 0);
                        close(client);
                    }
                }
            } else if (errno != EAGAIN) {
                perror("accept()");
                exit(EXIT_FAILURE);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (FD_ISSET(clients[i], &rdset)) {
                int32_t buff[16];
                ssize_t ret = recv(clients[i], buff, sizeof(buff), 0);
                printf("recv returned %ld bytes\n", ret);
                if (ret > 0) 
                {
                    printf("Client sent: '%.*s'\n", (int)ret, buff);
                    for(int i =0; i<MAX_LEN; i++)
                    {
                        int32_t value = ntohl(buff[i]);
                        fprintf(stderr,"dostalem liczbe: %d \n",value);
                        *current_max = *current_max > value ? *current_max : value;
                    }
                    fprintf(stderr,"max_number: %d \n",*current_max);
                    int32_t answer = htonl(*current_max);
                    send(clients[i],(char*)&answer,sizeof(int32_t),0);
                } else if (ret == 0) {
                    close(clients[i]);
                    clients[i] = -1;
                } else if (errno != EAGAIN) {
                    perror("recv()");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    int fdT;
	int new_flags;
    int current_max = 0;
	if (argc != 2) 
    {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
    if (sethandler(SIG_IGN, SIGPIPE))   ERR("Seting SIGPIPE:");
	if (sethandler(sigint_handler, SIGINT)) ERR("Seting SIGINT:");

    fdT = bind_tcp_socket(atoi(argv[1]));


	new_flags = fcntl(fdT, F_GETFL) | O_NONBLOCK;
	fcntl(fdT, F_SETFL, new_flags);
    doServer(fdT,&current_max);

    if (TEMP_FAILURE_RETRY(close(fdT)) < 0) ERR("close");
	fprintf(stderr, "Server has terminated.\n");
	return EXIT_SUCCESS;
}