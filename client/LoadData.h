#ifndef LOADDATA_H
#define LOADDATA_H

#include <inttypes.h>
#include "Graphics.h"
struct facet{
	float n[3];
	float c[9];
	uint16_t attr;
}__attribute__((packed));
struct bPoint{
	float d[3];
};
struct model{
	//int id;
	uint32_t facetCount;
	struct facet* facets;//This mirrors exactly the stl file
	struct bPoint* points;//This will become the buffer that will be passed to GL.
	struct bPoint* norm;//This is the buffer of normals passed to GL.
	GLuint vertex_buffer;//This is the actual vertex buffer
	GLuint norm_buffer;
	double diameter;
};
extern int modelCount;
extern struct model *models;


extern struct font myfont;

extern int starCount;
extern float* stars;
extern GLuint star_buffer;



extern void loadData();

#endif
