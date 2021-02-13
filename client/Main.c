#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <GLFW/glfw3.h>

#include "Input.h"
#include "Network.h"
#include "MB_ITQ.h"
#include "LoadData.h"
#include "Graphics.h"
#include "Gamestate.h"

struct gamestate_ gamestate = {.running = 1, .myShipId = -1, .screen = NONE, .console={.comm = {0}, .commLen = 0, .cursorPos = 0, .historyStart = NULL, .historyEnd = NULL, .historyView = NULL, .historyUsage = 0}};
void processGraphicsRequests(){
	int* uid = mb_itqDequeueNoBlock(&graphicsitq);
	while(uid){
		char buf[40];
		sprintf(buf, "RDEF %x#", *uid);
		sendMessage(buf, strlen(buf), 0);
		free(uid);
		uid = mb_itqDequeueNoBlock(&graphicsitq);
	}
}
void processNetData(char* buf){
	if(!strncmp("FRME", buf, 4)){
		char* oldbuf = frame;
		sem_wait(&frameAccess);
		frame = buf;
		sem_post(&frameAccess);
		free(oldbuf);
	}else if(!strncmp("CONS", buf, 4)){
		appendHistory(buf+4);
		free(buf);
	}else if(!strncmp("ODEF", buf, 4)){
		char* originalbuf = buf;
		buf+=4;
		int uid = *(int*)buf;
		buf+=4;
		//self.uid, self.mid, self.predMode, self.color[0], self.color[1], self.color[2])+(self.name+'\0'
		sem_wait(&frameAccess);
		objDef* t = objDefGet(uid);
		//if the graphics loop hasn't created this request, create one anyway
		if(!t){
			t = objDefInsert(uid);
		}
		sem_post(&frameAccess);
		t->revision = *(char*)buf;
		buf+=1;
		t->modelId = *(int*)buf;
		buf+=4;
		t->predictionMode = *(char*)buf;
		buf+=1;
		memcpy(t->color, buf, 16);
		buf+=16;
		t->name = calloc(strlen(buf)+1, 1);
		strcpy(t->name, buf);
		t->pending = DONE;
		free(originalbuf);
	}else if(!strncmp("ASGN", buf, 4)){
		gamestate.myShipId = *(int*)(buf+4);
		free(buf);
	}else{
		if(strlen(buf) < 4){
			printf("Unknown short message\n");
		}else printf("Unknown message type %.4s\n", buf);
		free(buf);
	}
}

int main(int argc, char** argv){
	printf("# Initializing Object Definition Tree\n");
	objDefInit();
	printf("# Initializing Graphics\n");
	initGraphics();
	printf("# Loading Data\n");
	loadData();
	printf("# Starting Graphics Loop\n");
	glfwMakeContextCurrent(NULL);
	startGraphicsLoop();//we have to do this separately, because loadData requires the GL context, which cannot be shared between threads
	printf("# Starting Networking\n");
	if(argc != 1){
		initNetworking(argv[1]);
	}else{
		initNetworking("127.0.0.1");
	}
	printf("# Setting Input Callbacks\n");
	glfwSetKeyCallback(window, &key_callback);
	printf("# Entering Game Loop\n");
	while (!handleInput()){
		char* msg = mb_itqDequeueTimed(&netitq, 0, 8333333);//process input at 120fps
		processGraphicsRequests();
		if(msg){
			//if we haven't seen this object in some time, delete it.
			sem_wait(&frameAccess);
			objDefDeleteOld(30);
			objDefAgeAll();
			sem_post(&frameAccess);
			while(msg){
				processNetData(msg);
				msg = mb_itqDequeueNoBlock(&netitq);
			}
			//only send controls as often as we recieve data
			if(controlChanged){
				sendInputs();
				controlChanged = 0;
			}
		}
	}
	gamestate.running = 0;
	printf("Stopping Graphics\n");
	shutdownGraphics();
	printf("Stopping Networking\n");
	shutdownNetwork();
	printf("Bye!\n");
}
void appendHistory(char* msg){
	struct console_* c = &(gamestate.console);
	while(1){
		struct historyLine* h = malloc(sizeof(struct historyLine));
		int len = 0;
		while(msg[len] != '\n' && msg[len] != 0){
			len++;
		}
		h->length = len;
		h->text = malloc(len+1);
		memcpy(h->text, msg, len);
		h->text[len] = 0;
		h->next = NULL;
		h->t = time(NULL);
		h->prev = c->historyStart;
		if(h->prev){
			h->prev->next = h;
		}
		c->historyStart = h;
		if(c->historyView == h->prev){
			c->historyView = h;
		}
		if(c->historyEnd == NULL){
			c->historyEnd = h;
		}

		c->historyUsage += len;
		if(msg[len] == '\n'){
			msg += len+1;
		}else{
			break;
		}
	}
	while(c->historyUsage > HISTORY_SIZE){
		struct historyLine* del = c->historyEnd;
		c->historyEnd = c->historyEnd->next;
		if(c->historyEnd) c->historyEnd->prev = NULL;
		c->historyUsage -= del->length;
		if(c->historyView == del) c->historyView = c->historyEnd;
		if(c->historyStart == del) c->historyStart = NULL;
		free(del->text);
		free(del);
	}
}
