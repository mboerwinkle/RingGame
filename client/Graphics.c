#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <GLFW/glfw3.h>
#include <pthread.h>
#include <math.h>
#include <assert.h>
#include "Graphics.h"
#include "LoadData.h"
#include "Gamestate.h"
#include "Delay.h"
#include "ExternLinAlg.h"

#define FOV 90 //PI/2
extern int errno;

pthread_t graphicsthread;
mb_itq graphicsitq;

char* frame;//This is the network data for what to draw.
sem_t frameAccess;
float textcolor[3] = {0.827,0.827,0.827};

struct shaderProg{
	GLuint frag_shader;
	GLuint vert_shader;
	GLuint program;
	GLuint u_lens, u_cam, u_camTrs, u_modRot, u_color, u_offset, u_aspect, u_scale, a_loc, a_norm;
};

GLFWwindow* window;
GLfloat cam_mat[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};//u_cam (camera pointing direction)
GLfloat cam_trs[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};//translate to camera location
GLfloat mod_rot[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};//rotate point based on model rotation
GLfloat mod_scl[16] = {-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};//Scale model
GLfloat cam_lens[16];//Camera lens (clipping, fov, etc)
GLfloat cam_lens_ortho[16];
struct shaderProg solidShader, starShader, hudShader;

void cerr(char* msg){
	int err = glGetError();
	while(err != 0){
		printf("Error (%s): %d\n", msg, err);
		err = glGetError();
	}
}

int cloc[3];//Camera location

int XRES = 800;
int YRES = 800;

static void error_callback(int error, const char* description){
	fprintf(stderr, "Error: %s\n", description);
}
char *glMsgBuf = NULL;
void printGLProgErrors(GLuint prog){
	GLint ret = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ret);
	printf("Link status %d ", ret);
	glGetProgramiv(prog, GL_ATTACHED_SHADERS, &ret);
	printf("Attached Shaders: %d ", ret);
	
	if(glMsgBuf == NULL) glMsgBuf = calloc(3000, sizeof(char));
	glMsgBuf[0] = 0;
	int len;
	glGetProgramInfoLog(prog, 3000, &len, glMsgBuf);
	printf("Len: %d\n", len);
	glMsgBuf[len] = 0;
	printf("GL Info Log: %s\n", glMsgBuf);
}
void printGLShaderErrors(GLuint shader){
	if(glMsgBuf == NULL) glMsgBuf = calloc(3000, sizeof(char));
	glMsgBuf[0] = 0;
	int len;
	glGetShaderInfoLog(shader, 3000, &len, glMsgBuf);
	printf("Len: %d\n", len);
	glMsgBuf[len] = 0;
	printf("GL Info Log: %s\n", glMsgBuf);
}
void drawStars(){
	glDisable(GL_DEPTH_TEST);
	glUseProgram(starShader.program);
	cerr("After use program (stars)");
	glBindBuffer(GL_ARRAY_BUFFER, star_buffer);
	glVertexAttribPointer(starShader.a_loc, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*) 0);
	glUniformMatrix4fv(starShader.u_lens, 1, GL_FALSE, cam_lens);
	glUniformMatrix4fv(starShader.u_cam, 1, GL_FALSE, cam_mat);
	glUniform3f(solidShader.u_offset, cloc[0], cloc[1], cloc[2]);
	glDrawArrays(GL_POINTS, 0, starCount);
}
char* readFileContents(char* path){
	FILE* fp = fopen(path, "r");
	if(!fp){
		printf("Failed to open file: %s\n", path);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	rewind(fp);  /* same as rewind(f); */
	char *ret = malloc(fsize + 1);
	fread(ret, 1, fsize, fp);
	fclose(fp);
	ret[fsize] = 0;
	return ret;
}
struct shaderProg createShader(char* vertPath, char* fragPath){
	struct shaderProg ret;
	ret.vert_shader = glCreateShader(GL_VERTEX_SHADER);
	GLuint vs = ret.vert_shader;
	char* verttext = readFileContents(vertPath);
	glShaderSource(vs, 1, (const char**)&verttext, NULL);
	free(verttext);
	glCompileShader(vs);
	printGLShaderErrors(vs);

	ret.frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint fs = ret.frag_shader;
	char* fragtext = readFileContents(fragPath);
	glShaderSource(fs, 1, (const char**)&fragtext, NULL);
	free(fragtext);
	glCompileShader(fs);
	printGLShaderErrors(fs);

	ret.program = glCreateProgram();
	GLuint p = ret.program;
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);
	cerr("Post link");

	ret.u_lens = glGetUniformLocation(p, "u_lens");
	ret.u_cam = glGetUniformLocation(p, "u_cam");
	ret.u_camTrs = glGetUniformLocation(p, "u_camTrs");
	ret.u_modRot = glGetUniformLocation(p, "u_modRot");
	ret.u_color = glGetUniformLocation(p, "u_baseColor");
	ret.u_offset = glGetUniformLocation(p, "u_offset");
	ret.u_aspect = glGetUniformLocation(p, "u_aspect");
	ret.u_scale = glGetUniformLocation(p, "u_scale");
	ret.a_loc = glGetAttribLocation(p, "a_loc");
	ret.a_norm = glGetAttribLocation(p, "a_norm");

	if(ret.a_loc != -1) glEnableVertexAttribArray(ret.a_loc);
	if(ret.a_norm != -1) glEnableVertexAttribArray(ret.a_norm);
	printGLProgErrors(p);
	return ret;
}

float teamcolors[6] = {0.0, 1.0, 0.941, 1.0, 0.0, 0.941};
void drawFrame(){
	char* buf = frame;
	buf += 4;
	int loc[3];
	float rot[4];
	int teamCount;
	memcpy(&teamCount, buf, 4);
	buf+=4;
	int* teamScores = (int*)buf;
	buf+=4*teamCount;//get past the scores.
	int objectCount = *(int*)buf;
	buf+=4;
	//printf("Got Frame: %d %d %d (%f %f %f %f) Objects: %d\n", loc[0], loc[1], loc[2], rot[0], rot[1], rot[2], rot[3], objectCount);
	for(int idx = 0; idx < objectCount; idx++){
		if(*(int*)(buf+idx*33) == gamestate.myShipId){
			objDef* mydef = objDefGet(gamestate.myShipId);
			if(!mydef || mydef->pending == WAITING) break;
			memcpy(cloc, buf+idx*33+5, 12);
			memcpy(rot, buf+idx*33+17, 16);
			float up[3] = {0, 0, 1};
			float front[3] = {1, 0, 0};
			float eye[3] = {0, 0, 0};//eye stays at 0 since we use a separate method for offsets
			rotateVec(up, rot, up);
			rotateVec(front, rot, front);
			float mdiameter = models[mydef->modelId].diameter;
			for(int dim = 0; dim < 3; dim++){
				cloc[dim] += (-1.2 * front[dim] +  0.5 * up[dim]) * mdiameter;
			}
			mat4x4idenf(cam_mat);
			glhLookAtf2(cam_mat, eye, front, up);
			break;
		}
	}
	drawStars();
	//printf("%d objects\n", objectCount);
	for(int oidx = 0; oidx < objectCount; oidx++){
		int uid = *(int*)buf;
		buf+=4;
		char revision = *(char*)buf;
		buf+=1;
		memcpy(loc, buf, 12);
		buf+=12;
		memcpy(rot, buf, 16);
		buf+=16;
		//printf("Model: %d (%d %d %d) (%f %f %f %f)\n", mid, loc[0], loc[1], loc[2], rot[0], rot[1], rot[2], rot[3]);
		objDef* o = objDefGet(uid);
		if(o){
			o->age = 0;
			if(o->pending != WAITING){
				drawModel(o->modelId, o->color, loc, rot, o->name);
				if(o->revision != revision && o->pending == DONE){
					o->pending = WAITINGVERSION;
					int* msg = malloc(sizeof(int));
					*msg = uid;
					mb_itqAdd(&graphicsitq, msg);
				}
			}
		}else{
			objDefInsert(uid);
			int* msg = malloc(sizeof(int));
			*msg = uid;
			mb_itqAdd(&graphicsitq, msg);
		}
	}
	for(int tIdx = 0; tIdx < teamCount; tIdx++){
		char scoreMsg[80];
		sprintf(scoreMsg, "Team %d: %d", tIdx, teamScores[tIdx]);
		drawHudText(scoreMsg, &myfont, 0, myfont.invaspect*1.5*tIdx*0.02, 0.02, &(teamcolors[3*tIdx]));
	}
}
void drawConsole(){
	glDisable(GL_DEPTH_TEST);
	glUseProgram(hudShader.program);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	float consoleback[12] = {0,0,0,1,1,0,1,0,0,1,1,1};
	glVertexAttribPointer(hudShader.a_loc, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)(consoleback));
	glUniformMatrix4fv(hudShader.u_lens, 1, GL_FALSE, cam_lens_ortho);
	glUniform1f(hudShader.u_scale, 1.0);
	glUniform1f(hudShader.u_aspect, 0.5);
	glUniform2f(hudShader.u_offset, 0,0);
	glUniform3f(hudShader.u_color, 0.318,0.322,0.463);
	glDrawArrays(GL_TRIANGLES, 0, 6);//background
	glUniform1f(hudShader.u_scale, 0.01);
	glUniform1f(hudShader.u_aspect, 1.5);
	glUniform2f(hudShader.u_offset, (float)gamestate.console.cursorPos/80.0, 0.5-1.0/56.0);
	glUniform3f(hudShader.u_color, 0.259,0.749,1.000);
	glDrawArrays(GL_TRIANGLES, 0, 6);//background
	drawHudText(gamestate.console.comm, &myfont, 0.0, 0.5-1.0/56.0, 0.0125*0.8, textcolor);//draw current command
	for(int idx = 0; idx < CONS_ROW; idx++){
		drawHudText(&(gamestate.console.history[(CONS_COL+1)*idx]), &myfont, 0.0, 0.5-((idx+2)/56.0), 0.0125*0.8, textcolor);
	}
}
void* graphicsLoop(void* none){
	glfwMakeContextCurrent(window);
	while(gamestate.running){
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		delay(60);
		sem_wait(&frameAccess);
		if(frame){
			drawFrame();
		}
		if(gamestate.screen == CONS){
			drawConsole();
		}
		sem_post(&frameAccess);
		glfwSwapBuffers(window);
	}
	return NULL;
}

void initGraphics(){
	mb_itqInit(&graphicsitq);
	frame = NULL;
	sem_init(&frameAccess, 0, 1);
	glfwSetErrorCallback(error_callback);
	/* Initialize the library */
	if (!glfwInit()){
		printf("GLFW init failed\n");
		exit(1);
	}
	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	window = glfwCreateWindow(XRES, YRES, "Ring Capture", NULL, NULL);
	if (!window){
		glfwTerminate();
		exit(1);
	}
	/* Make the window's context current */
	glfwMakeContextCurrent(window);
	printf("GL Version: %s\n", glGetString(GL_VERSION));
	glfwGetFramebufferSize(window, &XRES, &YRES);
	printf("FBSize %d %d\n", XRES, YRES);
	glViewport(0, 0, XRES, YRES);
	gluPerspective(cam_lens, 1.5, XRES/(float)YRES, 100, 200000);//vfov was 1.22
	glOrthoEquiv(cam_lens_ortho, 0, 1, 1, 0, -1, 1);
	//glDepthFunc(GL_LESS);
	//glClearDepth(1.0);
	//glEnable(GL_CULL_FACE);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	solidShader = createShader("gl/solid.vert", "gl/color.frag");
	starShader = createShader("gl/star.vert", "gl/white.frag");
	hudShader = createShader("gl/hud.vert", "gl/color.frag");
	cerr("After enableVertexAttrib");
	puts("Done initializing graphics");
}
void startGraphicsLoop(){
	pthread_create(&graphicsthread, NULL, &graphicsLoop, NULL);
}
//Transforms world space coordinates l[3] to screen space coordinates r[2]
//Should this be done with feedback shaders?
void getScreenFromWorld(int* l, float* r){
	float relLoc[4] = {l[0]-cloc[0], l[1]-cloc[1], l[2]-cloc[2], 1.0};
	float camSpaceLoc[4];
	float lensSpaceLoc[4];
	mat4x4By4x1Multf(camSpaceLoc, cam_mat, relLoc);
	mat4x4By4x1Multf(lensSpaceLoc, cam_lens, camSpaceLoc);
	r[0] = lensSpaceLoc[0]/lensSpaceLoc[3];
	r[1] = lensSpaceLoc[1]/lensSpaceLoc[3];
	r[2] = lensSpaceLoc[2];
}
void drawModel(int idx, float* color, int* loc, float* rot, char* name){
	glEnable(GL_DEPTH_TEST);
	glUseProgram(solidShader.program);
	cerr("After use program (solid)");
	struct model *m = &(models[idx]);
	//Set u_camTrs to relative position
	int relLoc[3] = {loc[0]-cloc[0], loc[1]-cloc[1], loc[2]-cloc[2]};
	mat4x4idenf(cam_trs);
	mat4x4Transf(cam_trs, relLoc[0], relLoc[1], relLoc[2]);
	float revModRot[4] = {rot[0], -rot[1], -rot[2], -rot[3]};
	mat4x4fromQuat(mod_rot, revModRot);
	//mat4x4Multf(mod_rot, mod_rot, mod_scl);//Scale the initial model prior to rotation
	glBindBuffer(GL_ARRAY_BUFFER, m->vertex_buffer);
	glVertexAttribPointer(solidShader.a_loc, 3, GL_FLOAT, GL_FALSE, sizeof(struct bPoint), (void*) 0);
	cerr("After AttribPtr");
	glBindBuffer(GL_ARRAY_BUFFER, m->norm_buffer);
	glVertexAttribPointer(solidShader.a_norm, 3, GL_FLOAT, GL_FALSE, sizeof(struct bPoint), (void*) 0);
	glUniformMatrix4fv(solidShader.u_lens, 1, GL_FALSE, cam_lens);
	glUniformMatrix4fv(solidShader.u_cam, 1, GL_FALSE, cam_mat);
	glUniformMatrix4fv(solidShader.u_camTrs, 1, GL_FALSE, cam_trs);
	glUniformMatrix4fv(solidShader.u_modRot, 1, GL_FALSE, mod_rot);
	glUniform3f(solidShader.u_color, color[0], color[1], color[2]);
	glDrawArrays(GL_TRIANGLES, 0, m->facetCount*3);
	if(name && name[0] != 0){//model has a name
		float r[3];
		getScreenFromWorld(loc, r);
		if(r[2] > 0) drawHudText(name, &myfont, 0.5*r[0]+0.5, -0.5*r[1]+0.5, 0.01, textcolor);
	}
}
void drawHudText(char* str, struct font* f, double x, double y, double scale, float* color){
	glDisable(GL_DEPTH_TEST);
	glUseProgram(hudShader.program);
	cerr("After use program (hud)");
	glBindBuffer(GL_ARRAY_BUFFER, f->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, f->ref_buffer);
	
	glUniformMatrix4fv(hudShader.u_lens, 1, GL_FALSE, cam_lens_ortho);
	glUniform1f(hudShader.u_scale, (float)scale);
	glUniform1f(hudShader.u_aspect, (float)f->invaspect);
	glUniform3f(hudShader.u_color, color[0], color[1], color[2]);

	glVertexAttribPointer(hudShader.a_loc, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*) 0);
	for(int idx = 0; idx < strlen(str); idx++){
		glUniform2f(hudShader.u_offset, x+(1.0+f->spacing)*scale*idx, y);
		int letter = str[idx];
		letter -= 33;
		if(letter < 0 || letter >= 94) continue;
		glDrawElements(GL_TRIANGLES, f->letterLen[letter], GL_UNSIGNED_SHORT, (void*) (sizeof(short)*f->letterStart[letter]));
	}
}

void shutdownGraphics(){
	int joinRet = pthread_join(graphicsthread, NULL);
	if(joinRet){
		printf("Graphics thread join error: %d\n", joinRet);
	}else{
		printf("Graphics thread joined\n");
	}
	glfwTerminate();
}
