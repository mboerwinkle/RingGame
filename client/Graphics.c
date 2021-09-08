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
GLfloat cam_lens_distant[16];
GLfloat cam_lens_ortho[16];
struct shaderProg solidShader,lineShader, starShader, hudShader;

void cerr(char* msg){
	int err = glGetError();
	while(err != 0){
		printf("Error (%s): %d\n", msg, err);
		err = glGetError();
	}
}

int32_t cloc[3];//Camera location

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
	glEnable(GL_DEPTH_TEST);
	glUseProgram(starShader.program);
	cerr("After use program (stars)");
	glBindBuffer(GL_ARRAY_BUFFER, star_buffer);
	glVertexAttribPointer(starShader.a_loc, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*) 0);
	glUniformMatrix4fv(starShader.u_lens, 1, GL_FALSE, cam_lens_distant);
	glUniformMatrix4fv(starShader.u_cam, 1, GL_FALSE, cam_mat);
	glUniform3f(starShader.u_offset, cloc[0], cloc[1], cloc[2]);
	glDrawArrays(GL_POINTS, 0, starCount);
	glClear(GL_DEPTH_BUFFER_BIT);
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

void requestODef(int32_t uid){
	int32_t* msg = malloc(sizeof(int32_t));
	*msg = uid;
	mb_itqAdd(&graphicsitq, msg);
}
objDef* objDefGetRev(int id, char rev){
	objDef* o = objDefGet(id);
	if(o){
		o->age = 0;
		if(o->pending != WAITING){
			if(rev != o->revision && o->pending == DONE){
				o->pending = WAITINGVERSION;
				requestODef(id);
			}
			return o;
		}
	}else{
		objDefInsert(id);
		requestODef(id);
	}
	return NULL;
}
float teamcolors[8] = {0.0, 1.0, 0.941, 1.0, 1.0, 0.0, 0.941, 1.0};
void drawFrame(struct frame_* frame){
	objDef* mydef = objDefGet(gamestate.myShipId);
	object* me = frame->me;
	float up[3] = {0, 0, 1};
	float front[3] = {1, 0, 0};
	rotateVec(up, gamestate.viewRotation, up);
	rotateVec(front, gamestate.viewRotation, front);
	mat4x4idenf(cam_mat);
	glhLookAtf2(cam_mat, (float[3]){0.0f,0.0f,0.0f}, front, up);//eye stays at 0 since we use a separate method for offsets
	if(mydef && mydef->pending != WAITING && me){
		memcpy(cloc, me->loc, 3 * sizeof(int32_t));
		float mdiameter = models[mydef->odat.modelId].diameter;
		for(int dim = 0; dim < 3; dim++){
			cloc[dim] += (-1.2 * front[dim] + 0.5 * up[dim]) * mdiameter;
		}
	}
	drawStars();
	for(int oidx = 0; oidx < frame->objcount; oidx++){
		object* obj = &(frame->obj[oidx]);
		objDef* odef = objDefGetRev(obj->uid, obj->revision);
		if(odef){
			assert(odef->otype == 'o');
			drawModel(odef->odat.modelId, odef->color, obj->loc, obj->rot, odef->odat.name);
		}
	}
	for(int lidx = 0; lidx < frame->linecount; lidx++){
		lineseg* line = &(frame->line[lidx]);
		objDef* odef = objDefGetRev(line->uid, -1);
		if(odef){
			assert(odef->otype == 'l');
			int32_t* loc = odef->ldat.loc;
			float* vec = odef->ldat.vec;
			int32_t movement = line->movement;
			int32_t floc[3] = {loc[0]+(vec[0]*movement), loc[1]+(vec[1]*movement), loc[2]+(vec[2]*movement)};
			drawLine(floc, odef->ldat.offset, odef->color);
		}
	}
	for(int tIdx = 0; tIdx < frame->teamcount; tIdx++){
		char scoreMsg[80];
		sprintf(scoreMsg, "Team %d: %d", tIdx, frame->teamscores[tIdx]);
		drawHudText(scoreMsg, &myfont, 0, myfont.invaspect*1.5*tIdx*0.02, 0.02, &(teamcolors[4*tIdx]), strlen(scoreMsg));
	}
	drawCrosshairs();
}
void drawConsole(int mode){
	if(mode == 1){
		glDisable(GL_DEPTH_TEST);
		glUseProgram(hudShader.program);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		float consoleback[12] = {0,0,0,1,1,0,1,0,0,1,1,1};
		glVertexAttribPointer(hudShader.a_loc, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)(consoleback));
		glUniformMatrix4fv(hudShader.u_lens, 1, GL_FALSE, cam_lens_ortho);
		glUniform1f(hudShader.u_scale, 1.0);
		glUniform1f(hudShader.u_aspect, 0.5);
		glUniform2f(hudShader.u_offset, 0,0);
		glUniform4f(hudShader.u_color, 0.318,0.322,0.463,0.5);
		glDrawArrays(GL_TRIANGLES, 0, 6);//background
		int cursoroffset = 0;
		if(gamestate.console.cursorPos > CONS_COL-1) cursoroffset = gamestate.console.cursorPos-(CONS_COL-1);
		glUniform1f(hudShader.u_scale, 0.01);
		glUniform1f(hudShader.u_aspect, 1.5);
		glUniform2f(hudShader.u_offset, (float)(gamestate.console.cursorPos-cursoroffset)/80.0, 0.5-1.0/56.0);
		glUniform4f(hudShader.u_color, 0.259,0.749,1.000,1.0);
		glDrawArrays(GL_TRIANGLES, 0, 6);//cursor
		drawHudText(gamestate.console.comm+cursoroffset, &myfont, 0.0, 0.5-1.0/56.0, 0.0125*0.8, textcolor, COMMAND_SIZE);//draw current command
	}

	int hline = 0;
	time_t t = time(NULL)-CHAT_PROM_TIME;
	for(struct historyLine* h = gamestate.console.historyView; h != NULL && hline < CONS_ROW; h = h->prev){
		int len = h->length;
		if(!mode && h->t <= t) break;
		int sublines = 0;
		while(len > CONS_COL){
			sublines++;
			len -= CONS_COL;
		}
		while(sublines >= 0){
			drawHudText(h->text+sublines*CONS_COL, &myfont, 0.0, 0.5-((hline+2)/56.0), 0.0125*0.8, textcolor, CONS_COL);
			sublines--;
			hline++;
		}
	}
}
void* graphicsLoop(void* none){
	glfwMakeContextCurrent(window);
	while(gamestate.running){
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//delay(60);
		sem_wait(&(gamestate.frameAccess));
		if(gamestate.frame){
			drawFrame(gamestate.frame);
		}
		if(gamestate.screen == CONS){
			drawConsole(1);
		}else{
			drawConsole(0);
		}
		sem_post(&(gamestate.frameAccess));
		glfwSwapBuffers(window);
	}
	return NULL;
}

void initGraphics(){
	mb_itqInit(&graphicsitq);
	frame = NULL;
	sem_init(&(gamestate.frameAccess), 0, 1);
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
	/* Enable vsync */
	glfwSwapInterval(1);
	printf("GL Version: %s\n", glGetString(GL_VERSION));
	glfwGetFramebufferSize(window, &XRES, &YRES);
	printf("FBSize %d %d\n", XRES, YRES);
	glViewport(0, 0, XRES, YRES);
	gluPerspective(cam_lens, 1.5, XRES/(float)YRES, 100, 200000);//vfov was 1.22
	gluPerspective(cam_lens_distant, 1.5, XRES/(float)YRES, 1000, 2000000000);
	glOrthoEquiv(cam_lens_ortho, 0, 1, 1, 0, -1, 1);
	//glDepthFunc(GL_LESS);
	//glClearDepth(1.0);
	glEnable(GL_CULL_FACE);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	solidShader = createShader("gl/solid.vert", "gl/color.frag");
	//starShader = createShader("gl/star.vert", "gl/white.frag");
	lineShader = createShader("gl/line.vert", "gl/color.frag");
	starShader = createShader("gl/star.vert", "gl/color.frag");
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
	cerr("After Model AttribPtr");
	glBindBuffer(GL_ARRAY_BUFFER, m->norm_buffer);
	glVertexAttribPointer(solidShader.a_norm, 3, GL_FLOAT, GL_FALSE, sizeof(struct bPoint), (void*) 0);
	glUniformMatrix4fv(solidShader.u_lens, 1, GL_FALSE, cam_lens);
	glUniformMatrix4fv(solidShader.u_cam, 1, GL_FALSE, cam_mat);
	glUniformMatrix4fv(solidShader.u_camTrs, 1, GL_FALSE, cam_trs);
	glUniformMatrix4fv(solidShader.u_modRot, 1, GL_FALSE, mod_rot);
	glUniform4f(solidShader.u_color, color[0], color[1], color[2], color[3]);
	glDrawArrays(GL_TRIANGLES, 0, m->facetCount*3);
	if(name && name[0] != 0){//model has a name
		float r[3];
		getScreenFromWorld(loc, r);
		if(r[2] > 0) drawHudText(name, &myfont, 0.5*r[0]+0.5, -0.5*r[1]+0.5, 0.01, textcolor, strlen(name));
	}
}
void drawLine(int32_t* loc, int32_t* offset, float* color){
	glEnable(GL_DEPTH_TEST);
	glUseProgram(lineShader.program);
	cerr("After use program (line)");
	float l[6] = {loc[0]-cloc[0], loc[1]-cloc[1], loc[2]-cloc[2], loc[0]+offset[0]-cloc[0], loc[1]+offset[1]-cloc[1], loc[2]+offset[2]-cloc[2]};
	glBindBuffer(GL_ARRAY_BUFFER, 0);//FIXME is this needed? //FIXME premake a colorful line-art missile and pass it in array buffer.
	glVertexAttribPointer(lineShader.a_loc, 3, GL_FLOAT, GL_FALSE, 0, l);
	cerr("After Line AttribPtr");
	glUniformMatrix4fv(lineShader.u_lens, 1, GL_FALSE, cam_lens);
	glUniformMatrix4fv(lineShader.u_cam, 1, GL_FALSE, cam_mat);
	glUniform4f(lineShader.u_color, color[0], color[1], color[2], color[3]);
	glDrawArrays(GL_LINES, 0, 2);
	cerr("test");
}
void drawHudText(char* str, struct font* f, double x, double y, double scale, float* color, int len){
	glDisable(GL_DEPTH_TEST);
	glUseProgram(hudShader.program);
	cerr("After use program (hud)");
	glBindBuffer(GL_ARRAY_BUFFER, f->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, f->ref_buffer);
	
	glUniformMatrix4fv(hudShader.u_lens, 1, GL_FALSE, cam_lens_ortho);
	glUniform1f(hudShader.u_scale, (float)scale);
	glUniform1f(hudShader.u_aspect, (float)f->invaspect);
	glUniform4f(hudShader.u_color, color[0], color[1], color[2], color[3]);

	glVertexAttribPointer(hudShader.a_loc, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*) 0);
	for(int idx = 0; idx < len; idx++){
		glUniform2f(hudShader.u_offset, x+(1.0+f->spacing)*scale*idx, y);
		int letter = str[idx];
		if(!letter) return;
		letter -= 33;
		if(letter < 0 || letter >= 94) continue;
		glDrawElements(GL_TRIANGLES, f->letterLen[letter], GL_UNSIGNED_SHORT, (void*) (sizeof(short)*f->letterStart[letter]));
	}
}
void drawCrosshairs() {
	glDisable(GL_DEPTH_TEST);
	glUseProgram(hudShader.program);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	GLfloat crosshairs[8] = {0.47,0.50, 0.53,0.50,   0.50,0.47, 0.50,0.53};
	glVertexAttribPointer(hudShader.a_loc, 2, GL_FLOAT, GL_FALSE, 0, (void*)crosshairs);
	glUniform1f(hudShader.u_scale, 1.0);
	glUniform1f(hudShader.u_aspect, 1.0);
	glUniform2f(hudShader.u_offset, 0,0);
	glUniform4f(hudShader.u_color, 1,0,0,1);
	glDrawArrays(GL_LINES, 0, 4);
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
