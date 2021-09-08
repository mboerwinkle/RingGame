#ifndef INPUT_H
#define INPUT_H
#include <GLFW/glfw3.h>
//what controls in controlState.s do we not want to resend? (like events)
#define CS_SENDONCEMAP 0x1

struct controlState {//This is the control representation
	int32_t s; //Bitmap [..., up, down, left, right, space]
	double mousex;
	double mousey;
};

extern int handleInput();
extern void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
extern void char_callback(GLFWwindow* window, unsigned int c);
extern void cursorpos_callback(GLFWwindow* window, double xpos, double ypos);
extern void updateRotations(float framerate);
extern struct controlState control;
extern struct controlState lastControl;
#endif
