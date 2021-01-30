#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "MB_ITQ.h"
void mb_itqInit(mb_itq* q){
	q->enqueue = 0;
	q->dequeue = 0;
	sem_init(&(q->size), 0, 0);
	sem_init(&(q->enqueue_lock), 0, 1);
	sem_init(&(q->dequeue_lock), 0, 1);
}
void mb_itqAdd(mb_itq* q, void* data){
	sem_wait(&(q->enqueue_lock));
	q->buf[q->enqueue] = data;
	q->enqueue = (q->enqueue+1)%MB_ITQ_BUFLEN;
	assert(q->enqueue != q->dequeue);//we wrapped around
	sem_post(&(q->enqueue_lock));
	sem_post(&(q->size));
}

void* mb_itqDequeue(mb_itq* q){
	sem_wait(&(q->size));//wait until there is something to dequeue
	sem_wait(&(q->dequeue_lock));
	void* ret = q->buf[q->dequeue];
	q->dequeue = (q->dequeue+1)%MB_ITQ_BUFLEN;
	sem_post(&(q->dequeue_lock));
	return ret;
}
void* mb_itqDequeueNoBlock(mb_itq* q){
	if(sem_trywait(&(q->size))){//check if there is something to dequeue
		return NULL;
	}
	sem_wait(&(q->dequeue_lock));
	void* ret = q->buf[q->dequeue];
	q->dequeue = (q->dequeue+1)%MB_ITQ_BUFLEN;
	sem_post(&(q->dequeue_lock));
	return ret;
}
void* mb_itqDequeueTimed(mb_itq* q, int sec, long nsec){
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);//get current time
	timeout.tv_sec += sec;//add in the requested timeout
	timeout.tv_nsec += nsec;
	if(timeout.tv_nsec >= 1000000000){
		timeout.tv_nsec -= 1000000000;
		timeout.tv_sec += 1;
	}
	if(sem_timedwait(&(q->size), &timeout)){//check if there is something to dequeue in time
		return NULL;
	}
	sem_wait(&(q->dequeue_lock));
	void* ret = q->buf[q->dequeue];
	q->dequeue = (q->dequeue+1)%MB_ITQ_BUFLEN;
	sem_post(&(q->dequeue_lock));
	return ret;
}
