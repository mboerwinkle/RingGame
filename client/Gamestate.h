#ifndef GAMESTATE_H
#define GAMESTATE_H
#include <time.h>
#include <semaphore.h>
#define CONS_ROW 25
#define CONS_COL 80
#define HISTORY_SIZE 4000
#define COMMAND_SIZE 200
//chat or console messages have CHAT_PROM_TIME seconds of promiscuity
#define CHAT_PROM_TIME 5
#if (CONS_COL*CONS_ROW) > HISTORY_SIZE
 #warning Possibly insufficient HISTORY_SIZE
#endif
struct historyLine{
	char* text;
	struct historyLine* next;
	struct historyLine* prev;
	time_t t;
	short length;
};
typedef struct object{
	int32_t loc[3];
	float rot[4];
	int32_t uid;
	int8_t revision;
} object;
struct frame_{
	object* me;
	object* obj;
	int32_t* teamscores;
	int8_t teamcount;
	int32_t objcount;
};
struct gamestate_{
	int running;
	int32_t myShipId;
	float localRotation[4];
	float viewRotation[4];
	float turnspeed;
	enum{
		NONE, CONS
	} screen;
	struct console_{
		int historyUsage;
		struct historyLine* historyStart;
		struct historyLine* historyEnd;
		struct historyLine* historyView;
		char comm[COMMAND_SIZE+1];
		int commLen;
		int cursorPos;
	} console;
	struct frame_* frame;
	sem_t frameAccess;
};
extern struct gamestate_ gamestate;//in Main.c
extern void appendHistory(char* msg);

typedef struct objDef{//An AVL tree (because I hate implementing RB trees...)
	struct objDef* (child[2]);
	struct objDef* parent;
	char* name;
	int height;
	int id;
	int modelId;
	enum{WAITING, WAITINGVERSION, DONE} pending;//do we have the data for it yet?
	int age;//For auto ageing
	float color[4];
	char predictionMode;
	char revision;
} objDef;
extern objDef* objDefRoot;
extern void objDefInit();
extern objDef* objDefGet(int id);
extern void objDefAgeAll();
extern void objDefResetAge(int id);
extern void objDefDeleteOld(int age);
extern objDef* objDefInsert(int id);
extern void objDefDelete(objDef* target);
extern void objDefPrintTree();
#endif
