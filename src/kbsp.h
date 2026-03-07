#include "../include/num_redefs.h"
#include "raylib.h"
#include "raymath.h"

#ifndef KBSP_H_
#define KBSP_H_

#define BSP_VERSION 29
#define BSP_LUMPS 	15

typedef struct {
    int16_t min;
    int16_t max;
} Bsp_Box32;

enum LUMP_TYPES {
	LUMP_ENTS			= 0,
	LUMP_PLANES 		= 1,
	LUMP_MIPTEX			= 2,
	LUMP_VERTICES		= 3,
	LUMP_VIS			= 4,
	LUMP_NODES			= 5,
	LUMP_TEXINFO		= 6,
	LUMP_FACES			= 7,
	LUMP_LIGHTMAPS		= 8,
	LUMP_CLIPNODES 		= 9,
	LUMP_LEAVES			= 10,
	LUMP_LFACES			= 11,
	LUMP_EDGES			= 12,
	LUMP_L_EDGES		= 13,
	LUMP_MODELS			= 14,
};

typedef struct {
	i32 file_offset;
	i32 file_size;

} Bsp_Lump;

typedef struct {
	i32 version;
	Bsp_Lump lumps[BSP_LUMPS];

} Bsp_Header;

typedef struct {
	float normal[3];
	float dist;
	i32 type;

} Bsp_Plane;

typedef struct {
	i32 *offset;
	i32 numtex;

} Bsp_Mipheader;

typedef struct {
	char name[16];

	u32 width;
	u32 height;

	u32 offset1;
	u32 offset2;
	u32 offset4;
	u32 offset8;

} Bsp_Miptex;

typedef struct {
	u32 planenum;
	i16 children[2];

} Bsp_ClipNode;

typedef struct {
	i32 planenum;
	i16 children[2];
	i16 mins[3];
	i16 maxs[3];
	u16 first_face;
	u16 num_faces;

} Bsp_Node;

typedef struct {
	float mins[3], maxs[3];
	float origin[3];

	i32 head_nodes[4];
	i32 num_leafs;
	i32 face_id;
	i32 faces;

} Bsp_Model;

typedef struct {
	Bsp_Plane *planes;
	Bsp_Miptex *miptex;
	Vector3 *verts;
	// Vis
	Bsp_Node *nodes;
	// Texinfo
	// Faces
	// Lightmaps
	Bsp_ClipNode *clipnodes;
	// Leaves
	// L_faces
	// Edges
	// L_edges
	Bsp_Model *models;

	u32 num_planes;
	u32 num_miptex;
	u32 num_verts;
	u32 num_vis;
	u32 num_nodes;
	u32 num_clipnodes;
	u32 num_models;

} Bsp_Data;

Bsp_Data LoadBsp(char *path, bool print_output);
void UnloadBsp(Bsp_Data *data);

typedef struct {
	Bsp_Plane *planes;
	Bsp_ClipNode *nodes;

	int first_node;
	int last_node;

	int num_planes;

} Bsp_Hull;

Bsp_Hull Bsp_BuildHull(Bsp_Data *data, int hull_index);

#define CONTENTS_EMPTY -1
#define CONTENTS_SOLID -2
int Bsp_PointContents(Bsp_Hull *hull, int num, Vector3 point);

bool Bsp_RecursiveTrace(Bsp_Hull *hull, int node_num, Vector3 point_A, Vector3 point_B, Vector3 *interesection);

typedef struct {
	Bsp_Plane plane;

	Vector3 point;
	Vector3 normal;

	float distance;
	float fraction;

	bool start_solid;
	bool all_solid;
	bool in_open;
	bool in_water;

	bool hit;

} Bsp_TraceData;

Bsp_TraceData Bsp_TraceDataEmpty();

bool Bsp_RecursiveTraceEx(Bsp_Hull *hull, int node_num, float p1_frac, float p2_frac, Vector3 p1, Vector3 p2, Bsp_TraceData *trace);

#endif
