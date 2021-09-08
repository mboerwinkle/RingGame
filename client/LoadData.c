#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <GLFW/glfw3.h>
#include "LoadData.h"
#include "loadJsonConfig.h"
#include "ExternLinAlg.h"
int modelCount;
struct model *models;
struct font myfont;
int starCount;
float* stars;
GLuint star_buffer;
double maxd3(double a, double b, double c);
struct model loadModel(FILE* input, double size);

void populatebPts(struct model* i){//This function populates teh model 'Points' attribute. This is what will be passed to gl.
	i->points = calloc(i->facetCount*3 ,sizeof(struct bPoint));
	for(int fidx = 0; fidx < i->facetCount; fidx++){
		struct facet f = i->facets[fidx];
		for(int tp = 0; tp < 3; tp++){
			float* d = i->points[fidx*3+tp].d;
			d[0] = f.c[tp*3+0];
			d[1] = f.c[tp*3+1];
			d[2] = f.c[tp*3+2];
		}
	}
}

void populateNorm(struct model* i){//This function populates the facet normals for gl FIXME this should actually only do one normal per facet, then pass index to each point
	i->norm = calloc(i->facetCount*3 ,sizeof(struct bPoint));
	for(int fidx = 0; fidx < i->facetCount; fidx++){
		struct facet f = i->facets[fidx];
		float v1[3] = {f.c[0]-f.c[3], f.c[1]-f.c[4], f.c[2]-f.c[5]};
		float v2[3] = {f.c[0]-f.c[6], f.c[1]-f.c[7], f.c[2]-f.c[8]};
		float n[3];
		cross(n, v1, v2);
		norm3f(n);
		//printf("Normal: %f %f %f\n", n[0], n[1], n[2]);
		for(int tp = 0; tp < 3; tp++){
			float* d = i->norm[fidx*3+tp].d;
			d[0] = n[0];//FIXME memcpy
			d[1] = n[1];
			d[2] = n[2];
		}
	}
}

void loadData(){
	if(sizeof(float) != 4){
		printf("FATAL: float is not 4 byte\n");
		return;
	}
	FILE* mfp = fopen("assets/manifest3.json", "r");
	if(mfp == NULL){
		printf("Could not open data manifest\n");
		return;
	}
	printf("Loading models\n");
	jsonValue* manifest = jsonLoad(mfp);
	int modelCount = jsonGetLen(jsonGetObj(*manifest, "models"));
	models = calloc(modelCount, sizeof(struct model));
	for(int midx = 0; midx < modelCount; midx++){
		char path[100];
		sprintf(path, "assets/stl/%s.stl", jsonGetString(jsonGetObj(jsonGetArr(jsonGetObj(*manifest, "models"), midx), "name")));
		FILE* fp = fopen(path, "r");
		if(fp == NULL){
			printf("Failed to open %s\n", path);
			continue;
		}
		models[midx] = loadModel(fp, jsonGetDouble(jsonGetObj(jsonGetArr(jsonGetObj(*manifest, "models"), midx), "diameter")));
		printf("Loaded model %d (%d facets)\n", midx, models[midx].facetCount);
		populatebPts(&(models[midx]));
		populateNorm(&(models[midx]));
		
		glGenBuffers(1, &(models[midx].vertex_buffer));
		glBindBuffer(GL_ARRAY_BUFFER, models[midx].vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(struct bPoint)*models[midx].facetCount*3, models[midx].points, GL_STATIC_DRAW);

		glGenBuffers(1, &(models[midx].norm_buffer));
		glBindBuffer(GL_ARRAY_BUFFER, models[midx].norm_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(struct bPoint)*models[midx].facetCount*3, models[midx].norm, GL_STATIC_DRAW);

	}
	fclose(mfp);
	jsonFree(*manifest);
	free(manifest);
	//Done loading models. Load font.
	FILE* fontfp = fopen("assets/font.json", "r");
	if(fontfp == NULL){
		printf("Could not open font file\n");
		return;
	}
	
	printf("Loading font\n");
	jsonValue* fontjson = jsonLoad(fontfp);
	myfont.invaspect = jsonGetDouble(jsonGetObj(*fontjson, "invaspect"));
	myfont.spacing = jsonGetDouble(jsonGetObj(*fontjson, "spacing"));
	jsonValue pointsjson = jsonGetObj(*fontjson, "points");
	jsonValue formjson = jsonGetObj(*fontjson, "form");
	jsonValue letterstartjson = jsonGetObj(*fontjson, "letter");
	int pointsLen = jsonGetLen(pointsjson);
	int formLen = jsonGetLen(formjson);
	float* points = calloc(pointsLen, sizeof(float));
	short* form = calloc(formLen, sizeof(short));

	for(int idx = 0; idx < pointsLen; idx++){
		points[idx] = (float)jsonGetDouble(jsonGetArr(pointsjson, idx));
	}
	for(int idx = 0; idx < formLen; idx++){
		form[idx] = jsonGetInt(jsonGetArr(formjson, idx));
	}
	for(int idx = 0; idx < 94; idx++){
		myfont.letterStart[idx] = jsonGetInt(jsonGetArr(letterstartjson, idx));
		if(idx != 0){
			myfont.letterLen[idx-1] = (myfont.letterStart[idx]-myfont.letterStart[idx-1]);
		}
	}
	myfont.letterLen[93] = (formLen-myfont.letterStart[93]);
	
	glGenBuffers(1, &(myfont.vertex_buffer));
	glBindBuffer(GL_ARRAY_BUFFER, myfont.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*pointsLen, points, GL_STATIC_DRAW);

	glGenBuffers(1, &(myfont.ref_buffer));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, myfont.ref_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(short)*formLen, form, GL_STATIC_READ);
	
	free(form);
	free(points);
	fclose(fontfp);
	jsonFree(*fontjson);
	free(fontjson);
	FILE* starsfp = fopen("assets/stars.json", "r");
	jsonValue* starsJson = jsonLoad(starsfp);
	starCount = jsonGetLen(*starsJson);
	stars = malloc(sizeof(float)*3*starCount);
	printf("%d stars (%.1lf kb)\n", starCount, (double)(starCount*sizeof(float)*3)/1000.0);
	for(int sIdx = 0; sIdx < starCount; sIdx++){
		jsonValue star = jsonGetArr(*starsJson, sIdx);
		for(int dim = 0; dim < 3; dim++){
			jsonValue l = jsonGetArr(star, dim);
			stars[3*sIdx+dim] = (float)jsonGetInt(l);
		}
		//printf("Star: %d %d %d\n", stars[3*sIdx], stars[3*sIdx+1], stars[3*sIdx+2]);
	}
	fclose(starsfp);
	jsonFree(*starsJson);
	free(starsJson);
	glGenBuffers(1, &star_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, star_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*3*starCount, stars, GL_STATIC_DRAW);
}

struct model loadModel(FILE* input, double size){
	struct model ret = {.diameter = size};
	//skip header
	fseek(input, 80, SEEK_SET);
	//read triangle count
	fread(&(ret.facetCount), sizeof(uint32_t), 1, input);
	//printf("%d facets\n", ret.facetCount);
	//allocate space for the triangles
	ret.facets = calloc(ret.facetCount, sizeof(struct facet));
	//read in all the triangles
	fread(ret.facets, sizeof(struct facet), ret.facetCount, input);
	fclose(input);
	double min[3] = {0,0,0};
	double max[3] = {0,0,0};
	for(int fidx = 0; fidx < ret.facetCount; fidx++){
		for(int pidx = 0; pidx < 3; pidx++){
			for(int dim = 0; dim < 3; dim++){
				double v = ret.facets[fidx].c[pidx*3+dim];
				if(v < min[dim]) min[dim] = v;
				if(v > max[dim]) max[dim] = v;
			}
		}
	}
	double maxdiff = 0;
	for(int dim = 0; dim < 3; dim++){
		if(max[dim]-min[dim] > maxdiff) maxdiff = max[dim]-min[dim];
	}
	printf("maxdiff: %lf. Target Size: %lf\n", maxdiff, size);
	for(int fidx = 0; fidx < ret.facetCount; fidx++){
		for(int pidx = 0; pidx < 3; pidx++){
			for(int dim = 0; dim < 3; dim++){
				ret.facets[fidx].c[pidx*3+dim] *= (size/maxdiff);
			}
		}
	}
	return ret;
}

double maxd3(double a, double b, double c){
	if(a > b){
		if(a > c){
			return a;
		}
	}else{
		if(b > c){
			return b;
		}
	}
	return c;
}
