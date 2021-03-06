#ifndef INPUT_H
#define INPUT_H
#include <GLFW/glfw3.h>
struct controlState {//This is the control representation
	int32_t s; //Bitmap [..., up, down, left, right, space]
};

extern int handleInput();
extern void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
extern void char_callback(GLFWwindow* window, unsigned int c);
extern void updateRotations(float framerate);
extern struct controlState control;
extern char controlChanged;
#endif
