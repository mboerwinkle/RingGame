#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <GLFW/glfw3.h>

#include "Input.h"
#include "Network.h"
#include "MB_ITQ.h"
#include "LoadData.h"
#include "Graphics.h"
#include "Gamestate.h"

struct gamestate_ gamestate = {.running = 1, .myShipId = -1, .screen = NONE, .console={.comm = {0}, .commLen = 0, .cursorPos = 0, .history = {0}}};
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
		//printf("%s\n", buf);
		prependHistory(buf+4);
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
		memcpy(t->color, buf, 12);
		t->color[3] = 1.0;
		buf+=12;
		t->name = calloc(strlen(buf)+1, 1);
		strcpy(t->name, buf);
		t->pending = DONE;
		free(originalbuf);
	}else if(!strncmp("ASGN", buf, 4)){
		gamestate.myShipId = *(int*)(buf+4);
	}else{
		if(strlen(buf) < 4){
			printf("Unknown short message\n");
		}else printf("Unknown message type %.4s\n", buf);
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
void prependHistory(char* msg){
	memmove(&(gamestate.console.history[CONS_COL+1]), &(gamestate.console.history[0]), (CONS_COL+1)*(CONS_ROW-1));
	strncpy(gamestate.console.history, msg, CONS_COL);
	gamestate.console.history[CONS_COL] = 0;
}
