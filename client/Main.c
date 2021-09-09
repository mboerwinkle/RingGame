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
#include "ExternLinAlg.h"
#include "Gamestate.h"

struct gamestate_ gamestate = {.running = 1,
.myShipId = -1,
.localRotation = {1,0,0,0},
.viewRotation = {1,0,0,0},
.screen = NONE,
.console={
	.comm = {0},
	.commLen = 0,
	.cursorPos = 0,
	.historyStart = NULL,
	.historyEnd = NULL,
	.historyView = NULL,
	.historyUsage = 0
}
};

void processGraphicsRequests(){
	int8_t buf[12] = "\0\0\0\0RDEF";
	*(int32_t*)buf = htonI32(8);
	int32_t* uid = mb_itqDequeueNoBlock(&graphicsitq);
	while(uid){
		(*(int32_t*)(buf+8)) = htonI32(*uid);
		free(uid);
		sendMessage(buf, 12, 0);
		uid = mb_itqDequeueNoBlock(&graphicsitq);
	}
}
int8_t pingmessage[8] = {0x0,0x0,0x0,0x4,'P','I','N','G'};
int8_t pongmessage[8] = {0x0,0x0,0x0,0x4,'P','O','N','G'};
int processNetData(int8_t* buf){
	int ret = 0;//Used to tell the main loop if this represents a new frame (for ageing definitions, input, etc)
	int8_t* originalBuf = buf;//for freeing
	if(!strncmp("FRME", (char*)buf, 4)){
		ret = 1;
		struct frame_* oldFrame = gamestate.frame;
		struct frame_* frame = (struct frame_*)malloc(sizeof(struct frame_));
		frame->me = NULL;
		buf+=4;//get past "FRME"
		frame->teamcount = *(buf++);
		frame->teamscores = (int32_t*)malloc(frame->teamcount * sizeof(int32_t));
		for(int teamIdx = 0; teamIdx < frame->teamcount; teamIdx++){
			frame->teamscores[teamIdx] = ntohI32(*(int32_t*)buf);
			buf+=4;
		}
		frame->objcount = ntohI32(*(int32_t*)buf);
		buf+=4;
		frame->obj = (object*)malloc(frame->objcount * sizeof(object));
		for(int oidx = 0; oidx < frame->objcount; oidx++){
			object o;
			o.uid = ntohI32(*(int32_t*)buf);
			buf+=4;
			o.revision = *(buf++);
			for(int dim = 0; dim < 3; dim++){
				o.loc[dim] = ntohI32(*(int32_t*)buf);
				buf+=4;
			}
			buf += ntohQuat(o.rot, buf);
			frame->obj[oidx] = o;
			if(o.uid == gamestate.myShipId){
				frame->me = &(frame->obj[oidx]);
			}
		}
		frame->linecount = ntohI32(*(int32_t*)buf);
		buf+=4;
		frame->line = (lineseg*)malloc(frame->linecount * sizeof(lineseg));
		for(int lidx = 0; lidx < frame->linecount; lidx++){
			lineseg l;
			l.uid = ntohI32(*(int32_t*)buf);
			buf+=4;
			l.movement = ntohI32(*(int32_t*)buf);
			buf+=4;
			frame->line[lidx] = l;
		}
		//swap the frames
		sem_wait(&(gamestate.frameAccess));
		gamestate.frame = frame;
		sem_post(&(gamestate.frameAccess));
		//Free the old frame
		if(oldFrame){
			free(oldFrame->obj);
			free(oldFrame->line);
			free(oldFrame->teamscores);
			free(oldFrame);
		}
	}else if(!strncmp("PING", (char*)buf, 4)){
		sendMessage(pongmessage, 8, 1);
	}else if(!strncmp("PONG", (char*)buf, 4)){
		
	}else if(!strncmp("CONS", (char*)buf, 4)){
		appendHistory((char*)(buf+4));
	}else if(!strncmp("ODEF", (char*)buf, 4)){
		buf+=4;
		uint8_t otype = *(buf++);
		int32_t uid = ntohI32(*(int32_t*)buf);
		buf+=4;
		//self.uid, self.mid, self.predMode, self.color[0], self.color[1], self.color[2])+(self.name+'\0'
		sem_wait(&(gamestate.frameAccess));
		objDef* t = objDefGet(uid);
		//if the graphics loop hasn't created this request, create one anyway
		if(!t){
			t = objDefInsert(uid);
		}
		sem_post(&(gamestate.frameAccess));
		t->otype = otype;
		if(otype == 'o'){
			t->revision = *(buf++);
			t->odat.modelId = ntohI32(*(int32_t*)buf);
			buf+=4;
		}else if(otype == 'l'){
			for(int dimidx = 0; dimidx < 3; dimidx++){
				t->ldat.loc[dimidx] = ntohI32(*(int32_t*)buf);
				buf+=4;
			}
			for(int dimidx = 0; dimidx < 3; dimidx++){
				int32_t val = ntohI32(*(int32_t*)buf);
				t->ldat.offset[dimidx] = val;
				t->ldat.vec[dimidx] = val;
				buf+=4;
			}
			norm3f(t->ldat.vec);
		}else{
			assert(otype == 's');
			assert(0);//Temporary
		}
		uint8_t color[4];
		memcpy(color, buf, 4);
		for(int chanIdx = 0; chanIdx < 4; chanIdx++){
			t->color[chanIdx] = (float)(color[chanIdx]) / 255.0;
		}
		buf+=4;
		if(otype == 'o'){//FIXME move color after name
			t->odat.name = calloc(strlen((char*)buf)+1, 1);
			strcpy(t->odat.name, (char*)buf);
		}
		t->pending = DONE;
	}else if(!strncmp("ASGN", (char*)buf, 4)){
		buf += 4;
		gamestate.myShipId = ntohI32(*(int32_t*)(buf));
		buf += 4;
		gamestate.turnspeed = (float)ntohI16(*(int16_t*)buf) / 100.0;
		buf+=2;
	}else{
		if(strlen((char*)buf) < 4){
			printf("Unknown short message\n");
		}else printf("Unknown message type '%.4s'\n", buf);
	}
	free(originalBuf);
	return ret;
}

int main(int argc, char** argv){
	appendHistory("Welcome to RingGame.");
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
	glfwSetCharCallback(window, &char_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if(glfwRawMouseMotionSupported()){
		printf("Enabling Raw Mouse.\n");
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}else{
		printf("Raw Mouse not supported.\n");
	}
	glfwSetCursorPosCallback(window, &cursorpos_callback);
	printf("# Entering Game Loop\n");
	while (!handleInput()){
		char* msg = mb_itqDequeueTimed(&netitq, 0, 8333333);//process input at 120fps
		processGraphicsRequests();
		updateRotations(120);
		if(msg){
			//did we get a frame message? if so, then we can step our frame-dependent stuff
			int newframe = 0;
			while(msg){
				newframe |= processNetData((int8_t*)msg);
				msg = mb_itqDequeueNoBlock(&netitq);
			}
			if(newframe){
				//if we haven't seen this object in some time, delete it.
				sem_wait(&(gamestate.frameAccess));
				objDefDeleteOld(30);
				objDefAgeAll();
				sem_post(&(gamestate.frameAccess));
				//only send controls as often as we recieve frames
				sendInputs();
				sendOrientation(gamestate.localRotation);
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
	#define C (gamestate.console)
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
		h->prev = C.historyStart;
		if(h->prev){
			h->prev->next = h;
		}
		C.historyStart = h;
		if(C.historyView == h->prev){
			C.historyView = h;
		}
		if(C.historyEnd == NULL){
			C.historyEnd = h;
		}

		C.historyUsage += len;
		if(msg[len] == '\n'){
			msg += len+1;
		}else{
			break;
		}
	}
	while(C.historyUsage > HISTORY_SIZE){
		struct historyLine* del = C.historyEnd;
		C.historyEnd = C.historyEnd->next;
		if(C.historyEnd) C.historyEnd->prev = NULL;
		C.historyUsage -= del->length;
		if(C.historyView == del) C.historyView = C.historyEnd;
		if(C.historyStart == del) C.historyStart = NULL;
		free(del->text);
		free(del);
	}
	#undef C
}
