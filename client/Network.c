#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h> //For tcp_nodelay
#include <pthread.h>
#include <poll.h>

#include "Network.h"
#include "Input.h"
#include "MB_ITQ.h"
#include "Gamestate.h"

#define PORT 3131
mb_itq netitq;
int sockfd = -1;
struct pollfd serverpoll;
int connected = 0;
pthread_t netthread;
void* netListenLoop(void* none);
void initNetworking(char* srvAddr){
	mb_itqInit(&netitq);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		printf("Failed to create socket\n");
	}
	struct sockaddr_in server = {.sin_family = AF_INET, .sin_addr.s_addr = inet_addr(srvAddr), .sin_port=htons(PORT)};
	printf("Connecting to server at %s\n", srvAddr);
	if(connect(sockfd, (struct sockaddr*)&server, sizeof(server))){
		printf("Failed to connect to server\n");
		connected = 0;
	}else{
		connected = 1;
		serverpoll.fd = sockfd;
		serverpoll.events = POLLIN;
		pthread_create(&netthread, NULL, &netListenLoop, NULL);
	}
}

void processMessage(char* buf, int len){
	char* msg = malloc(len+1);
	memcpy(msg, buf, len);
	msg[len] = 0;
	mb_itqAdd(&netitq, msg);
}

int bufLen;
int bufUse;
char* buf;
void* netListenLoop(void* none){
	buf = NULL;
	bufLen = 0;
	bufUse = 0;
	char subbuf[4096];
	while(gamestate.running){
		int pollret = poll(&serverpoll, 1, 1000);
		if(pollret == 0){
			continue;
		}else if(pollret < 0){
			printf("Poll encountered error\n");
		}
		ssize_t msize = recv(sockfd, subbuf, 4096, 0);
		if(msize + bufUse > bufLen){
			bufLen = msize+bufUse+1;
			buf = realloc(buf, bufLen);
			printf("New network buffer size: %d\n", bufLen);
		}
		memcpy(buf+bufUse, subbuf, msize);
		bufUse += msize;
		buf[bufUse] = 0;
		int foundSomething = 1;
		while(foundSomething){
			foundSomething = 0;
			int msgLen = ((int*)buf)[0];
			//printf("buf is now: %30s... (msglen: %d)\n", buf+4, msgLen);
			if(bufUse-4 >= msgLen){
				processMessage(buf+4, msgLen);
				memmove(buf, buf+4+msgLen, bufUse - (4+msgLen));
				bufUse -= (4+msgLen);
				foundSomething = 1;
			}
		}
	}
	return NULL;
}

void sendInputs(){
	char msg[51];
	snprintf(msg, 50, "CTRL %x#", control.s);
	//printf("%s\n", msg);
	sendMessage(msg, strlen(msg), 1);//this should use a parallel UDP
}
void sendMessage(void* msg, int len, int nodelay){
	if(connected){
		int flag = nodelay;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
		send(sockfd, msg, len, 0);
		flag = 0;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	}
}

void shutdownNetwork(){
	int joinRet = pthread_join(netthread, NULL);
	if(joinRet){
		printf("Network thread join error: %d\n", joinRet);
	}else{
		printf("Network thread joined\n");
	}
	close(sockfd);
}
