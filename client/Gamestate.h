#ifndef GAMESTATE_H
#define GAMESTATE_H
#define CONS_COL 80
#define CONS_ROW 27
struct gamestate_{
	int running;
	int myShipId;
	enum{
		NONE, CONS
	} screen;
	struct{
		char comm[CONS_COL+1];
		int commLen;
		int cursorPos;
		char history[(CONS_COL+1)*CONS_ROW];
	} console;
} ;
extern struct gamestate_ gamestate;//in Main.c
extern void prependHistory(char* msg);

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
