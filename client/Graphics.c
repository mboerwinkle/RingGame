#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <GLFW/glfw3.h>
#include <pthread.h>
#include <math.h>
#include "LoadData.h"
#include "Gamestate.h"
#include "Delay.h"

#define FOV 90 //PI/2
extern int errno;

pthread_t graphicsthread;

char* frame;//This is the network data for what to draw.
sem_t frameAccess;

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
float distv3f(float* v){
	float d = 0;
	for(int idx = 0; idx < 3; idx++){
		d += v[idx]*v[idx];
	}
	return sqrt(d);
}
void norm3f(float* v){
	float d = distv3f(v);
	v[0] /= d;
	v[1] /= d;
	v[2] /= d;
}

void cross(float* res, float* a, float* b){
	res[0]=a[1]*b[2]-a[2]*b[1];
	res[1]=a[2]*b[0]-a[0]*b[2];
	res[2]=a[0]*b[1]-a[1]*b[0];
}

void mat4x4Multf(float* res, float* m1, float* m2){
	for(int x = 0; x < 4; x++){
		for(int y = 0; y < 4; y++){
			float v = 0;
			for(int i = 0; i < 4; i++){
				v += m1[y+4*i]*m2[i+4*x];
			}
			res[y+4*x] = v;
		}
	}
}
void mat4x4fromQuat(float* M, float *rot){
	M[0] = 1-2*rot[2]*rot[2]-2*rot[3]*rot[3];
	M[1] = 2*rot[1]*rot[2]+2*rot[0]*rot[3];
	M[2] = 2*rot[1]*rot[3]-2*rot[0]*rot[2];
	M[3] = 0;
	M[4] = 2*rot[1]*rot[2]-2*rot[0]*rot[3];
	M[5] = 1-2*rot[1]*rot[1]-2*rot[3]*rot[3];
	M[6] = 2*rot[2]*rot[3]+2*rot[0]*rot[1];
	M[7] = 0;
	M[8] = 2*rot[1]*rot[3]+2*rot[0]*rot[2];
	M[9] = 2*rot[2]*rot[3]-2*rot[0]*rot[1];
	M[10] = 1-2*rot[1]*rot[1]-2*rot[2]*rot[2];
	M[11] = 0;
	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;
}
void mat4x4Scalef(float* res, float xs, float ys, float zs){
	res[0] *= xs;
	res[5] *= ys;
	res[9] *= zs;
}
void mat4x4Transf(float* res, float x, float y, float z){
	res[12] += x;
	res[13] += y;
	res[14] += z;
}
void mat4x4idenf(float* res){
	for(int i = 0; i < 16; i++) res[i] = 0.0;
	res[0] = 1.0;
	res[5] = 1.0;
	res[10] = 1.0;
	res[15] = 1.0;
}
void cerr(char* msg){
	int err = glGetError();
	while(err != 0){
		printf("Error (%s): %d\n", msg, err);
		err = glGetError();
	}
}

int cloc[3];//Camera location

void quatMult(float* a, float* b, float* r){
	float ret[4];
	ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3]);
	ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2]);
	ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1]);
	ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0]);
	memcpy(r, ret, 4*sizeof(float));
}

void rotateVec(float* t, float* r, float* ret){
	float p[4] = {0, t[0], t[1], t[2]};
	float revRot[4] = {r[0], -r[1], -r[2], -r[3]};
	quatMult(r, p, p);
	quatMult(p, revRot, p);
	memcpy(ret, &(p[1]), 3*sizeof(float));
}

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
//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
void gluPerspective(float* m, float fovy, float aspect, float zNear, float zFar){
	float f = 1.0/tan(fovy/2.0);
	m[0] = f/aspect;
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;
	m[4] = 0;
	m[5] = f;
	m[6] = 0;
	m[7] = 0;
	m[8] = 0;
	m[9] = 0;
	m[10] = (zFar+zNear)/(zNear-zFar);
	m[11] = -1;
	m[12] = 0;
	m[13] = 0;
	m[14] = (2.0*zFar*zNear)/(zNear-zFar);
	m[15] = 0;
}
//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glOrtho.xml
void glOrthoEquiv(float* m, float left, float right, float bottom, float top, float nearval, float farval){
	m[0] = 2.0/(right-left);
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;
	m[4] = 0;
	m[5] = 2.0/(top-bottom);
	m[6] = 0;
	m[7] = 0;
	m[8] = 0;
	m[9] = 0;
	m[10] = -2.0/(farval-nearval);
	m[11] = 0;
	m[12] = -((right+left)/(right-left));
	m[13] = -((top+bottom)/(top-bottom));
	m[14] = -((farval+nearval)/(farval-nearval));
	m[15] = 1;
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
	float color[4];
	
	memcpy(loc, buf, 12);
	buf+=12;
	memcpy(rot, buf, 16);
	buf+=16;
	int teamCount;
	memcpy(&teamCount, buf, 4);
	buf+=4;
	int* teamScores = (int*)buf;
	buf+=4*teamCount;//get past the scores.
	int objectCount;
	memcpy(&objectCount, buf, 4);
	buf+=4;
	//printf("Got Frame: %d %d %d (%f %f %f %f) Objects: %d\n", loc[0], loc[1], loc[2], rot[0], rot[1], rot[2], rot[3], objectCount);
	setCameraLoc(loc, rot);
	drawStars();
	//printf("%d objects\n", objectCount);
	for(int oidx = 0; oidx < objectCount; oidx++){
		int mid;
		memcpy(&mid, buf, 4);
		buf+=4;
		char name[9];
		memcpy(name, buf, 8);
		name[8] = 0;
		buf+=8;
		memcpy(color, buf, 16);
		buf+=16;
		memcpy(loc, buf, 12);
		buf+=12;
		memcpy(rot, buf, 16);
		buf+=16;
		//printf("Model: %d (%d %d %d) (%f %f %f %f)\n", mid, loc[0], loc[1], loc[2], rot[0], rot[1], rot[2], rot[3]);
		drawModel(mid, color, loc, rot, name);
		/*if(name[0] != 0){
			printf("model name %s\n", name);
		}*/
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
	float textcolor[3] = {0.827,0.827,0.827};
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
	glUniform1f(solidShader.u_scale, 1);
	glUniformMatrix4fv(solidShader.u_lens, 1, GL_FALSE, cam_lens);
	glUniformMatrix4fv(solidShader.u_cam, 1, GL_FALSE, cam_mat);
	glUniformMatrix4fv(solidShader.u_camTrs, 1, GL_FALSE, cam_trs);
	glUniformMatrix4fv(solidShader.u_modRot, 1, GL_FALSE, mod_rot);
	glUniform3f(solidShader.u_color, color[0], color[1], color[2]);
	glDrawArrays(GL_TRIANGLES, 0, m->facetCount*3);
	if(name[0] != 0){//model has a name
		float offsetMat[16];
		float letterRotateMat[16];
		float letterRotateQuat[4] = {-0.5, 0.5, 0.5, -0.5};
		mat4x4fromQuat(letterRotateMat, letterRotateQuat);
		float intermediate[16];
		mat4x4idenf(offsetMat);
		mat4x4Transf(offsetMat, 0, 0, -m->diameter*0.6);
		float finalMat[16];
		float scale = 150;
		glUniform1f(solidShader.u_scale, scale);
		glBindBuffer(GL_ARRAY_BUFFER, myfont.vertex_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, myfont.ref_buffer);
		glVertexAttribPointer(hudShader.a_loc, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*) 0);
		for(int idx = 0; idx < strlen(name); idx++){
			mat4x4Multf(intermediate, mod_rot, offsetMat);
			mat4x4Multf(finalMat, intermediate, letterRotateMat);
			
			glUniformMatrix4fv(solidShader.u_modRot, 1, GL_FALSE, finalMat);
			int letter = name[idx];
			letter -= 33;
			if(letter < 0 || letter >= 94) continue;
			glDrawElements(GL_TRIANGLES, myfont.letterLen[letter], GL_UNSIGNED_SHORT, (void*) (sizeof(short)*myfont.letterStart[letter]));
			mat4x4Transf(offsetMat, 0, (1.0+myfont.spacing)*scale, 0);
		}

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
void glhLookAtf2( float *matrix, float *eyePosition3D, float *center3D, float *upVector3D ){
	float forward[3], side[3], up[3];
	float matrix2[16], resultMatrix[16];
	// --------------------
	forward[0] = center3D[0] - eyePosition3D[0];
	forward[1] = center3D[1] - eyePosition3D[1];
	forward[2] = center3D[2] - eyePosition3D[2];
	norm3f(forward);
	// --------------------
	// Side = forward x up
	cross(side, forward, upVector3D);
	norm3f(side);
	// Recompute up as: up = side x forward
	cross(up, side, forward);
	// --------------------
	matrix2[0] = side[0];
	matrix2[4] = side[1];
	matrix2[8] = side[2];
	matrix2[12] = 0.0;
	// --------------------
	matrix2[1] = up[0];
	matrix2[5] = up[1];
	matrix2[9] = up[2];
	matrix2[13] = 0.0;
	// --------------------
	matrix2[2] = -forward[0];
	matrix2[6] = -forward[1];
	matrix2[10] = -forward[2];
	matrix2[14] = 0.0;
	// --------------------
	matrix2[3] = matrix2[7] = matrix2[11] = 0.0;
	matrix2[15] = 1.0;
	// --------------------
	mat4x4Multf(resultMatrix, matrix, matrix2);
	mat4x4Transf(resultMatrix, -eyePosition3D[0], -eyePosition3D[1], -eyePosition3D[2]);
	// --------------------
	memcpy(matrix, resultMatrix, 16*sizeof(float));
}

void setCameraLoc(int* l, float* r){
	cloc[0] = l[0];
	cloc[1] = l[1];
	cloc[2] = l[2];
	float eye[3] = {0, 0, 0};
	float up[3] = {0, 0, 1};
	float front[3] = {1, 0, 0};
	rotateVec(up, r, up);
	rotateVec(front, r, front);
	mat4x4idenf(cam_mat);
	glhLookAtf2(cam_mat, eye, front, up);
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
