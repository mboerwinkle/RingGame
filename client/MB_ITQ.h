//My inter-thread queue.
#ifndef MB_ITQ_H
#define MB_ITQ_H
#include <semaphore.h>

/*typedef struct mb_itqElem{
	struct mb_itqElem* next;
	void* data;
}mb_itqElem;
typedef struct mb_itq{
	mb_itqElem* enqueue;//The enqueue pointer
	sem_t size;
	sem_t lock;
	mb_itqElem* prev;
	mb_itqElem* curr;
	int init;
}mb_itq;*/

#define MB_ITQ_BUFLEN 10
typedef struct mb_itq{
	void* buf[MB_ITQ_BUFLEN];
	int enqueue;
	int dequeue;
	sem_t size;
	sem_t enqueue_lock;
	sem_t dequeue_lock;
}mb_itq;

extern void mb_itqAdd(mb_itq* q, void* data);
extern void mb_itqInit(mb_itq* q);
extern void* mb_itqDequeue(mb_itq* q);
extern void* mb_itqDequeueNoBlock(mb_itq* q);
extern void* mb_itqDequeueTimed(mb_itq* q, int sec, long nsec);
#endif

