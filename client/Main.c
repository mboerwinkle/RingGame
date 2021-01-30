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

struct gamestate_ gamestate = {.running = 1, .screen = NONE, .console={.comm = {0}, .commLen = 0, .cursorPos = 0, .history = {0}}};


void processNetData(char* buf){
	if(!strncmp("FRME", buf, 4)){
		sem_wait(&frameAccess);
		free(frame);
		frame = buf;
		sem_post(&frameAccess);
	}else if(!strncmp("CONS", buf, 4)){
		//printf("%s\n", buf);
		prependHistory(buf+4);
	}else{
		if(strlen(buf) < 4){
			printf("Unknown short message\n");
		}else printf("Unknown message type %.4s\n", buf);
	}
}

int main(int argc, char** argv){
	printf("# Initializing Graphics\n");
	initGraphics();
	printf("# Loading Data\n");
	loadData();
	printf("# Starting Graphics Loop\n");
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
		if(msg){
			//puts("network data found!");
			processNetData(msg);
			if(controlChanged){//only send controls as often as we recieve data
				sendInputs();
				controlChanged = 0;
			}
		}//else puts("network timed out");
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
