#ifndef NETWORK_H
#define NETWORK_H
#include "MB_ITQ.h"
extern void initNetworking(char* srvAddr);
extern void shutdownNetwork();
extern void sendInputs();
extern void sendMessage(void* msg, int len, int nodelay);
extern mb_itq netitq;

#endif
