#ifndef GAMESTATE_H
#define GAMESTATE_H
#define CONS_COL 80
#define CONS_ROW 27
struct gamestate_{
	int running;
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
#endif
