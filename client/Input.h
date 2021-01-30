#ifndef INPUT_H
#define INPUT_H
#include <GLFW/glfw3.h>
struct controlState {//This is the control representation
	unsigned int s; //Bitmap [..., up, down, left, right, space]
};

extern int handleInput();
extern void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
extern struct controlState control;
extern char controlChanged;
#endif
