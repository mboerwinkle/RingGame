#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLFW/glfw3.h>

#include "Input.h"
#include "Graphics.h"
#include "Network.h"
#include "Gamestate.h"
#include "assert.h"

struct controlState control = {.s=0};
struct controlState lastControl = {.s=0};
char controlChanged;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS){
		if(gamestate.screen == NONE){
			if(key == GLFW_KEY_ESCAPE){
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}else if(key == GLFW_KEY_GRAVE_ACCENT || key == GLFW_KEY_ENTER){
				gamestate.screen = CONS;
			}
		}else if(gamestate.screen == CONS){
			if(key == GLFW_KEY_GRAVE_ACCENT || key == GLFW_KEY_ESCAPE){
				gamestate.screen = NONE;
			}else if(key >= ' ' && key <= '~' && gamestate.console.commLen < COMMAND_SIZE){//improper shift support
				memmove(&(gamestate.console.comm[gamestate.console.cursorPos+1]), &(gamestate.console.comm[gamestate.console.cursorPos]), (COMMAND_SIZE)-(gamestate.console.cursorPos));
				gamestate.console.comm[gamestate.console.cursorPos] = key;
				gamestate.console.commLen++;
				gamestate.console.cursorPos++;
			}else if(key == GLFW_KEY_LEFT){
				gamestate.console.cursorPos--;
				if(gamestate.console.cursorPos < 0){
					gamestate.console.cursorPos = 0;
				}
			}else if(key == GLFW_KEY_RIGHT){
				gamestate.console.cursorPos++;
				if(gamestate.console.cursorPos > gamestate.console.commLen){
					gamestate.console.cursorPos = gamestate.console.commLen;
				}
			}else if(key == GLFW_KEY_BACKSPACE && gamestate.console.cursorPos != 0){
				gamestate.console.cursorPos--;
				gamestate.console.commLen--;
				memmove(&(gamestate.console.comm[gamestate.console.cursorPos]), &(gamestate.console.comm[gamestate.console.cursorPos+1]), (COMMAND_SIZE)-(gamestate.console.cursorPos));
			}else if(key == GLFW_KEY_ENTER && gamestate.console.commLen != 0){
				printf("%s\n", gamestate.console.comm);
				char netmsg[10+gamestate.console.commLen];
				sprintf(netmsg, "COMM %s#", gamestate.console.comm);
				appendHistory(gamestate.console.comm);
				sendMessage(netmsg, strlen(netmsg), 0);
				gamestate.console.comm[0] = 0;
				gamestate.console.commLen = 0;
				gamestate.console.cursorPos = 0;
			}
			gamestate.console.comm[gamestate.console.commLen] = 0;
			return;
		}
	}
	
	int place = 0;
	switch(key){
		case GLFW_KEY_W://forward
			place = 1<<8;
			break;
		case GLFW_KEY_A://roll left
			place = 1<<7;
			break;
		case GLFW_KEY_S://reverse
			place = 1<<6;
			break;
		case GLFW_KEY_D://roll right
			place = 1<<5;
			break;
		case GLFW_KEY_UP://pitch up
			place = 1<<4;
			break;
		case GLFW_KEY_DOWN://pitch down
			place = 1<<3;
			break;
		case GLFW_KEY_LEFT://yaw left
			place = 1<<2;
			break;
		case GLFW_KEY_RIGHT://yaw right
			place = 1<<1;
			break;
		case GLFW_KEY_SPACE:
			place = 1;
	}
	if(place){
		if(action == GLFW_PRESS){
			control.s |= place;
		}else if(action == GLFW_RELEASE){
			control.s &= ~place;
		}
	}
}

int handleInput(){
	glfwPollEvents();
	if(glfwWindowShouldClose(window)) return 1;
	if(!memcmp(&control, &lastControl, sizeof(struct controlState)) == 0){
		controlChanged = 1;
	}
	lastControl = control;
	return 0;
}

