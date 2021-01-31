#ifndef GRAPHICS_H
#define GRAPHICS_H
#include <GLFW/glfw3.h>
#include <pthread.h>
#include <semaphore.h>


struct font{
	float invaspect;
	float spacing;
	GLuint vertex_buffer;
	GLuint ref_buffer;
	short letterStart[94];
	short letterLen[94];
};

extern char* frame;
extern sem_t frameAccess;
extern int frameCount;


extern GLFWwindow* window;
extern void setCameraLoc(int* l, float* r);
extern void initGraphics();
extern void startGraphicsLoop();
extern void shutdownGraphics();
extern void draw();
extern void drawStars();
extern void drawModel(int idx, float* color, int* loc, float* rot, char* name);
extern void drawHudText(char* str, struct font* f, double x, double y, double scale, float* color);
#endif
