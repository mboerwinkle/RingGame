#ifndef NETWORK_H
#define NETWORK_H
#include "MB_ITQ.h"
extern void initNetworking(char* srvAddr);
extern void shutdownNetwork();
extern void sendInputs();
extern void sendMessage(void* msg, int len, int nodelay);
extern int32_t htonI32(int32_t i);
extern int32_t ntohI32(int32_t i);
extern int16_t ntohI16(int16_t i);
extern int ntohQuat(float* ret, int8_t* ndata);
extern mb_itq netitq;

#endif
