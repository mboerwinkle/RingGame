#ifndef GRAPHICS_H
#define GRAPHICS_H
#include <GLFW/glfw3.h>
#include <pthread.h>
#include "MB_ITQ.h"

struct font{
	float invaspect;
	float spacing;
	GLuint vertex_buffer;
	GLuint ref_buffer;
	short letterStart[94];
	short letterLen[94];
};

extern mb_itq graphicsitq;

extern GLFWwindow* window;
extern void setCameraLoc(int* l, float* r);
extern void initGraphics();
extern void startGraphicsLoop();
extern void shutdownGraphics();
extern void draw();
extern void drawStars();
extern void drawModel(int idx, float* color, int* loc, float* rot, char* name);
extern void drawLine(int32_t* loc, int32_t* offset, float* color);
extern void drawHudText(char* str, struct font* f, double x, double y, double scale, float* color, int length);
extern void drawCrosshairs();
#endif
