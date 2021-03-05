#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLFW/glfw3.h>
#include <utf8proc.h>

#include "Input.h"
#include "Graphics.h"
#include "Network.h"
#include "Gamestate.h"
#include "assert.h"


int utf8_isFirstByte(char b){
	//check for all Byte1 patterns (includes null byte)
	return (b>>7 == 0 || b>>5 == 0b110 || b>>4 == 0b1110 || b>>3 == 0b11110);
}

struct controlState control = {.s=0};
struct controlState lastControl = {.s=0};
char controlChanged;

void char_callback(GLFWwindow* window, unsigned int c){
	int cpWidth = utf8proc_charwidth(c);
	if(gamestate.screen != CONS || c == '`' || c == '~' || cpWidth < 1 || !utf8proc_codepoint_valid(c)) return;
	//do we have space for the next character? remember console.comm is size[COMMAND_SIZE+1]
	if(gamestate.console.commLen+cpWidth <= COMMAND_SIZE){
		memmove(gamestate.console.comm + gamestate.console.cursorPos + cpWidth,
			gamestate.console.comm + gamestate.console.cursorPos,
			COMMAND_SIZE - gamestate.console.cursorPos);
		#ifndef NDEBUG //suppress unused variable warning for assert below
		int writtenlen =
		#endif
			utf8proc_encode_char(c, (utf8proc_uint8_t*) (gamestate.console.comm + gamestate.console.cursorPos));
		assert(writtenlen == cpWidth);
		gamestate.console.commLen += cpWidth;
		gamestate.console.cursorPos += cpWidth;
	}
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT){
		if(gamestate.screen == NONE){
			if(key == GLFW_KEY_ESCAPE){
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}else if(key == GLFW_KEY_GRAVE_ACCENT || key == GLFW_KEY_ENTER){
				gamestate.screen = CONS;
			}
		}else if(gamestate.screen == CONS){
			#define C (gamestate.console)
			if(key == GLFW_KEY_GRAVE_ACCENT || key == GLFW_KEY_ESCAPE){
				gamestate.screen = NONE;
			}else if(key == GLFW_KEY_LEFT){
				if(C.cursorPos != 0){
					int cpWidth = 1;
					while(!utf8_isFirstByte(C.comm[C.cursorPos - cpWidth])){
						cpWidth++;
					}
					C.cursorPos -= cpWidth;
				}
			}else if(key == GLFW_KEY_RIGHT){
				if(C.comm[C.cursorPos] != 0){
					int cpWidth = 1;
					while(!utf8_isFirstByte(C.comm[C.cursorPos + cpWidth])){
						cpWidth++;
					}
					C.cursorPos += cpWidth;
				}
			}else if(key == GLFW_KEY_BACKSPACE && C.cursorPos != 0){
				int cpWidth = 1;
				while(!utf8_isFirstByte(C.comm[C.cursorPos - cpWidth])){
					cpWidth++;
				}
				memmove(C.comm + C.cursorPos - cpWidth, C.comm + C.cursorPos, COMMAND_SIZE - C.cursorPos);
				C.cursorPos -= cpWidth;
				C.commLen -= cpWidth;
			}else if(key == GLFW_KEY_ENTER && C.commLen != 0){
				printf("%s\n", C.comm);
				char netmsg[10+C.commLen];
				sprintf(netmsg+4, "COMM%s", C.comm);
				*(int32_t*)netmsg = htonI32(strlen(netmsg+4));
				appendHistory(C.comm);
				sendMessage(netmsg, 4+strlen(netmsg+4), 0);
				C.comm[0] = 0;
				C.commLen = 0;
				C.cursorPos = 0;
			}
			C.comm[C.commLen] = 0;
			return;
			#undef C
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

