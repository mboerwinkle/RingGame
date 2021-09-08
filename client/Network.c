#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h> //For tcp_nodelay
#include <pthread.h>
#include <poll.h>
#include <assert.h>

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

int32_t bufLen;
int32_t bufUse;
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
			int32_t msgLen = ntohI32(*(int32_t*)buf);
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
	if(control.s == lastControl.s){
		return;
	}
	char msg[51];
	snprintf(msg+4, 46, "CTRL%x", control.s);
	*((int32_t*)msg) = htonI32(strlen(msg+4));
	sendMessage(msg, 4+strlen(msg+4), 1);//this should use a parallel UDP
	// ensure we don't multisend certain events
	control.s ^= control.s & CS_SENDONCEMAP;
	// update lastControl
	lastControl = control;
}
void sendOrientation(float* quat){
	char msg[16] = "\0\0\0\0ORNT";
	int len = htonQuat((int8_t*)&(msg[8]), quat);
	assert(sizeof(msg) >= 8+len);
	*(int32_t*)msg = htonI32(4+len);
	sendMessage(msg, 8+len, 1);
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
//Custom wrapper for easier platform dependent stuff
int32_t htonI32(int32_t i){
	return htonl(i);
}
int32_t ntohI32(int32_t i){
	return ntohl(i);
}
int16_t ntohI16(int16_t i){
	return ntohs(i);
}
int16_t htonI16(int16_t i){
	return htons(i);
}
//ret is length 4 quaternion. ndata is network data. Returns network length
#define NETQUATRES 32000.0
int ntohQuat(float* ret, int8_t* ndata){
	for(int idx = 0; idx < 4; idx++){
		int16_t val = *(int16_t*)(&(ndata[2*idx]));
		double intermed = ntohI16(val);
		ret[idx] = intermed / NETQUATRES;
	}
	return 8;
}
int htonQuat(int8_t* ndata, float* quat){
	for(int idx = 0; idx < 4; idx++){
		*(int16_t*)(&(ndata[2*idx])) = htonI16((double)(quat[idx]) * NETQUATRES);
	}
	return 8;
}
void shutdownNetwork(){
	if(connected){
		int joinRet = pthread_join(netthread, NULL);
		if(joinRet){
			printf("Network thread join error: %d\n", joinRet);
		}else{
			printf("Network thread joined\n");
		}
	}
	close(sockfd);
}
