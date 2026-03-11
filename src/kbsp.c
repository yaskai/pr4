#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "kbsp.h"
#include "../include/log_message.h"
#include "geo.h"
#include "hash.h"

Material *materials;
Texture2D *textures;
HashMap material_hashmap = (HashMap) {0};
static const u8 qPalette[256][3] = {
	{94, 89, 87},
	{101, 104, 106},
	{111, 114, 116},
	{94, 95, 93},
	{125, 120, 119},
	{83, 83, 82},
	{119, 116, 115},
	{78, 76, 75},
	{97, 98, 101},
	{103, 101, 99},
	{72, 72, 70},
	{83, 81, 79},
	{106, 109, 112},
	{78, 78, 76},
	{114, 111, 109},
	{103, 97, 94},
	{86, 85, 82},
	{87, 86, 84},
	{82, 85, 86},
	{84, 86, 87},
	{114, 112, 112},
	{121, 120, 121},
	{86, 87, 87},
	{88, 87, 86},
	{114, 117, 117},
	{103, 104, 106},
	{104, 106, 108},
	{117, 117, 119},
	{101, 103, 104},
	{103, 104, 104},
	{96, 96, 95},
	{98, 96, 94},
	{96, 96, 98},
	{97, 98, 99},
	{88, 91, 91},
	{90, 91, 91},
	{108, 110, 112},
	{109, 112, 113},
	{111, 109, 109},
	{106, 106, 109},
	{116, 110, 105},
	{125, 119, 112},
	{20, 22, 20},
	{139, 134, 133},
	{125, 128, 130},
	{94, 89, 83},
	{134, 127, 120},
	{37, 36, 35},
	{137, 131, 127},
	{102, 99, 95},
	{80, 78, 78},
	{98, 92, 87},
	{69, 68, 66},
	{69, 69, 68},
	{125, 122, 119},
	{122, 119, 115},
	{70, 73, 73},
	{126, 128, 127},
	{119, 115, 112},
	{105, 101, 101},
	{128, 127, 128},
	{26, 25, 23},
	{31, 32, 30},
	{117, 118, 116},
	{119, 118, 116},
	{42, 44, 43},
	{45, 46, 44},
	{111, 114, 112},
	{22, 22, 20},
	{23, 24, 22},
	{100, 94, 89},
	{101, 96, 90},
	{122, 122, 120},
	{122, 123, 122},
	{92, 88, 83},
	{121, 118, 117},
	{122, 120, 118},
	{103, 101, 102},
	{124, 124, 125},
	{88, 83, 78},
	{106, 108, 107},
	{106, 108, 110},
	{85, 83, 85},
	{115, 116, 119},
	{115, 118, 120},
	{111, 112, 115},
	{62, 62, 61},
	{63, 64, 63},
	{94, 96, 98},
	{94, 98, 99},
	{130, 123, 117},
	{129, 125, 121},
	{73, 75, 74},
	{73, 76, 77},
	{103, 98, 97},
	{129, 129, 131},
	{129, 131, 133},
	{92, 95, 96},
	{94, 95, 96},
	{116, 116, 117},
	{100, 96, 93},
	{100, 97, 95},
	{108, 104, 103},
	{96, 93, 91},
	{96, 93, 93},
	{113, 108, 104},
	{18, 19, 17},
	{20, 20, 18},
	{132, 128, 125},
	{134, 131, 128},
	{133, 131, 131},
	{132, 133, 134},
	{122, 125, 125},
	{125, 125, 125},
	{117, 111, 107},
	{116, 112, 110},
	{122, 116, 111},
	{121, 117, 115},
	{70, 71, 69},
	{70, 72, 71},
	{86, 89, 88},
	{88, 89, 87},
	{38, 38, 37},
	{38, 40, 40},
	{128, 125, 124},
	{130, 126, 124},
	{75, 74, 73},
	{76, 76, 74},
	{106, 106, 104},
	{108, 106, 104},
	{93, 91, 90},
	{92, 91, 93},
	{111, 109, 107},
	{114, 109, 107},
	{96, 100, 102},
	{98, 101, 102},
	{65, 64, 63},
	{66, 66, 64},
	{106, 106, 106},
	{109, 106, 106},
	{82, 85, 84},
	{84, 85, 83},
	{108, 109, 109},
	{109, 111, 109},
	{92, 93, 95},
	{95, 93, 95},
	{75, 76, 77},
	{78, 76, 77},
	{119, 121, 120},
	{119, 121, 122},
	{100, 100, 98},
	{101, 101, 100},
	{105, 99, 95},
	{108, 102, 95},
	{95, 91, 86},
	{95, 91, 88},
	{126, 123, 121},
	{126, 125, 122},
	{80, 80, 81},
	{83, 80, 81},
	{93, 93, 91},
	{93, 93, 93},
	{80, 80, 78},
	{81, 82, 79},
	{78, 79, 78},
	{78, 81, 79},
	{65, 66, 66},
	{67, 67, 66},
	{90, 86, 81},
	{90, 86, 83},
	{105, 103, 100},
	{108, 103, 99},
	{85, 81, 79},
	{86, 83, 79},
	{80, 78, 76},
	{82, 78, 76},
	{90, 89, 86},
	{91, 89, 88},
	{88, 89, 90},
	{90, 89, 90},
	{130, 128, 125},
	{129, 128, 128},
	{105, 101, 99},
	{107, 101, 98},
	{100, 99, 102},
	{100, 101, 103},
	{33, 34, 32},
	{34, 36, 34},
	{85, 83, 82},
	{87, 83, 82},
	{111, 111, 108},
	{112, 112, 110},
	{41, 42, 40},
	{43, 43, 41},
	{114, 114, 111},
	{114, 114, 113},
	{110, 104, 99},
	{113, 106, 100},
	{122, 126, 128},
	{124, 126, 128},
	{113, 115, 115},
	{113, 115, 117},
	{90, 87, 85},
	{90, 88, 88},
	{119, 122, 124},
	{120, 124, 126},
	{67, 69, 67},
	{67, 70, 70},
	{81, 83, 85},
	{83, 83, 85},
	{81, 82, 81},
	{81, 82, 83},
	{86, 87, 90},
	{86, 89, 90},
	{76, 79, 81},
	{78, 79, 81},
	{100, 98, 97},
	{100, 98, 100},
	{118, 119, 119},
	{117, 119, 122},
	{74, 72, 71},
	{74, 74, 71},
	{75, 78, 76},
	{75, 78, 78},
	{96, 98, 96},
	{98, 98, 96},
	{121, 121, 124},
	{121, 123, 125},
	{96, 95, 92},
	{98, 95, 92},
	{90, 91, 94},
	{90, 93, 94},
	{105, 104, 102},
	{105, 104, 104},
	{101, 103, 101},
	{103, 103, 101},
	{91, 91, 88},
	{93, 91, 88},
	{78, 81, 82},
	{78, 82, 84},
	{108, 108, 106},
	{109, 110, 107},
	{96, 100, 100},
	{98, 100, 99},
	{110, 106, 104},
	{111, 108, 104},
	{116, 114, 110},
	{116, 114, 112},
	{40, 40, 38},
	{41, 41, 40},
	{116, 114, 114},
	{117, 116, 114},
	{72, 73, 73},
	{73, 73, 76},
	{111, 110, 112},
	{111, 112, 112},
};

Bsp_Data LoadBsp(char *path, bool print_output) {
	Bsp_Data data = (Bsp_Data) {0};

	FILE *pF = fopen(path, "rb");

	if(!pF) {
		MessageError("ERROR: Could not load file ", path);
		return data;
	}

	// ---------------------------------------------------------------------------------------
	// Header
	Bsp_Header header = {0};
	fread(&header, sizeof(header), 1, pF);

	if(header.version != BSP_VERSION) {
		MessageError("ERROR: BSP version mismatch", NULL);
		return data;
	}

	if(print_output)
		printf("%d\n", header.version);
	// ---------------------------------------------------------------------------------------
	// Entities
	Bsp_Lump ents_lump = header.lumps[LUMP_ENTS];

	// ---------------------------------------------------------------------------------------
	// Planes
	Bsp_Lump planes_lump = header.lumps[LUMP_PLANES];

	fseek(pF, planes_lump.file_offset, SEEK_SET);
	i32 plane_count = planes_lump.file_size / sizeof(Bsp_Plane);  
	Bsp_Plane *planes = malloc(sizeof(Bsp_Plane) * plane_count);
	fread(planes, sizeof(Bsp_Plane) * plane_count, 1, pF);

	data.num_planes = plane_count;
	data.planes = planes;
	// ---------------------------------------------------------------------------------------
	// Miptex
	Bsp_Lump miptex_lump = header.lumps[LUMP_MIPTEX];
	fseek(pF, miptex_lump.file_offset, SEEK_SET);

	i32 miptex_count;
	fread(&miptex_count, sizeof(i32), 1, pF);

	i32 *mip_offsets = malloc(sizeof(i32) * miptex_count);
	fread(mip_offsets, sizeof(i32) * miptex_count, 1, pF);

	data.num_miptex = miptex_count;
	data.miptex = malloc(sizeof(Bsp_Miptex) * miptex_count);

	for(int i = 0; i < miptex_count; i++) {
		fseek(pF, miptex_lump.file_offset + mip_offsets[i], SEEK_SET);
		fread(&data.miptex[i], sizeof(Bsp_Miptex), 1, pF);
	}
	// ---------------------------------------------------------------------------------------
	// Vertices
	Bsp_Lump vert_lump = header.lumps[LUMP_VERTICES];

	fseek(pF, vert_lump.file_offset, SEEK_SET);
	i32 vert_count = vert_lump.file_size / sizeof(Vector3);  
	Vector3 *verts = malloc(sizeof(Vector3) * vert_count);
	fread(verts, sizeof(Vector3) * vert_count, 1, pF);

	data.num_verts = vert_count;
	data.verts = verts;
	// ---------------------------------------------------------------------------------------
	// Vis
	Bsp_Lump vis_lump = header.lumps[LUMP_VIS];
	fseek(pF, vis_lump.file_offset, SEEK_SET);
	data.vis = malloc(vis_lump.file_size);
	fread(data.vis, vis_lump.file_size, 1, pF);
	// ---------------------------------------------------------------------------------------
	// Nodes
	Bsp_Lump nodes_lump = header.lumps[LUMP_NODES];

	fseek(pF, nodes_lump.file_offset, SEEK_SET);
	i32 node_count = nodes_lump.file_size / sizeof(Bsp_Node);
	Bsp_Node *nodes = malloc(sizeof(Bsp_Node) * node_count);
	fread(nodes, sizeof(Bsp_Node) * node_count, 1, pF);

	data.num_nodes = node_count;
	data.nodes = nodes;
	// ---------------------------------------------------------------------------------------
	// Tex info
	Bsp_Lump texinfo_lump = header.lumps[LUMP_TEXINFO];

	fseek(pF, texinfo_lump.file_offset, SEEK_SET);
	i32 texinfo_count = texinfo_lump.file_size / sizeof(Bsp_Surface);
	Bsp_Surface *texinfos = malloc(sizeof(Bsp_Surface) * texinfo_count);
	fread(texinfos, sizeof(Bsp_Surface) * texinfo_count, 1, pF);

	data.num_surfaces = texinfo_count;
	data.surfaces = texinfos;
	// ---------------------------------------------------------------------------------------
	// Faces 
	Bsp_Lump faces_lump = header.lumps[LUMP_FACES];

	fseek(pF, faces_lump.file_offset, SEEK_SET);
	i32 face_count = faces_lump.file_size / sizeof(Bsp_Face);
	
	Bsp_Face *faces = malloc(sizeof(Bsp_Face) * face_count);
	fread(faces, sizeof(Bsp_Face) * face_count, 1, pF);

	data.num_faces = face_count;
	data.faces = faces; 
	// ---------------------------------------------------------------------------------------
	// Lightmaps 
	Bsp_Lump lightmap_lump = header.lumps[LUMP_LIGHTMAPS];
	fseek(pF, lightmap_lump.file_offset, SEEK_SET);
	data.lightmap.num_lightmap = lightmap_lump.file_size;
	data.lightmap.lightmap = malloc(lightmap_lump.file_size);
	fread(data.lightmap.lightmap, lightmap_lump.file_size, 1, pF);
	// ---------------------------------------------------------------------------------------
	// Clip nodes
	Bsp_Lump clipnodes_lump = header.lumps[LUMP_CLIPNODES];

	fseek(pF, clipnodes_lump.file_offset, SEEK_SET);
	i32 clipnode_count = clipnodes_lump.file_size / sizeof(Bsp_ClipNode);
	Bsp_ClipNode *clipnodes = malloc(sizeof(Bsp_ClipNode) * clipnode_count);
	fread(clipnodes, sizeof(Bsp_ClipNode) * clipnode_count, 1, pF);

	data.num_clipnodes = clipnode_count;
	data.clipnodes = clipnodes; 
	// ---------------------------------------------------------------------------------------
	// Leaves 
	Bsp_Lump leaves_lump = header.lumps[LUMP_LEAVES];

	fseek(pF, leaves_lump.file_offset, SEEK_SET);
	i32 leaf_count = leaves_lump.file_size / sizeof(Bsp_Leaf);
	Bsp_Leaf *leaves = malloc(sizeof(Bsp_Leaf) * leaf_count);
	fread(leaves, sizeof(Bsp_Leaf) * leaf_count, 1, pF);

	data.num_leaves = leaf_count;
	data.leaves = leaves;
	// ---------------------------------------------------------------------------------------
	// L_faces 
	Bsp_Lump lfaces_lump = header.lumps[LUMP_LFACES];	
	
	fseek(pF, lfaces_lump.file_offset, SEEK_SET);
	i32 lfaces_count = lfaces_lump.file_size / sizeof(u16);
	u16 *lfaces = malloc(sizeof(u16) * lfaces_count);
	fread(lfaces, sizeof(u16) * lfaces_count, 1, pF);

	data.num_lfaces = lfaces_count;
	data.lfaces = lfaces;
	// ---------------------------------------------------------------------------------------
	// Edges 
	Bsp_Lump edges_lump = header.lumps[LUMP_EDGES];

	fseek(pF, edges_lump.file_offset, SEEK_SET);
	i32 edge_count = edges_lump.file_size / sizeof(Bsp_Edge);
	Bsp_Edge *edges = malloc(sizeof(Bsp_Edge) * edge_count);
	fread(edges, sizeof(Bsp_Edge) * edge_count, 1, pF);

	data.num_edges = edge_count;
	data.edges = edges;
	// ---------------------------------------------------------------------------------------
	// L_edges 
	Bsp_Lump ledges_lump = header.lumps[LUMP_L_EDGES];

	fseek(pF, ledges_lump.file_offset, SEEK_SET);
	i32 ledge_count = ledges_lump.file_size / sizeof(i32);
	i32 *ledges = malloc(sizeof(i32) * ledge_count);
	fread(ledges, sizeof(i32) * ledge_count, 1, pF);

	data.num_ledges = ledge_count;
	data.ledges = ledges;

	// ---------------------------------------------------------------------------------------
	// Models
	Bsp_Lump models_lump = header.lumps[LUMP_MODELS];
	fseek(pF, models_lump.file_offset, SEEK_SET);

	i32 model_count = models_lump.file_size / sizeof(Bsp_Model);
	Bsp_Model *models = malloc(sizeof(Bsp_Model) * model_count);
	fread(models, sizeof(Bsp_Model) * model_count, 1, pF);

	data.num_models = model_count;
	data.models = models;
	// ---------------------------------------------------------------------------------------
	data.miptex_lump_offset = miptex_lump.file_offset;
	data.textures = malloc(sizeof(Texture) * data.num_miptex);

	for(int i = 0; i < data.num_miptex; i++) {
		Bsp_Miptex *mip = &data.miptex[i];

		if(mip->offset1 == 0 || mip->offset1 >= 256) {
			data.textures[i] = (Texture2D) {0};
			continue;
		}

		int px_count = mip->width * mip->height;
		u8 *indexed = malloc(px_count);

		fseek(pF, data.miptex_lump_offset + mip_offsets[i] + mip->offset1, SEEK_SET);
		fread(indexed, px_count, 1, pF);

		u8 *rgba = calloc(px_count * 4, 1);
		for(int j = 0; j < px_count; j++) {
			rgba[j*4+0] = qPalette[indexed[j]][0];   
			rgba[j*4+1] = qPalette[indexed[j]][1];   
			rgba[j*4+2] = qPalette[indexed[j]][2];   
			rgba[j*4+3] = 255;   
		}

		Image img = (Image) {
			.data = rgba,
			.width = mip->width,
			.height = mip->height,
  			.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
			.mipmaps = 1
		};

		data.textures[i] = LoadTextureFromImage(img);
		free(indexed);
		free(rgba);
	}
	free(mip_offsets);

	// Close and return data
	fclose(pF);


	FilePathList mat_list = LoadDirectoryFiles("tools/Disruptor/textures/custom");	

	materials = malloc(sizeof(Material) * mat_list.count); 
	textures = malloc(sizeof(Texture2D) * mat_list.count); 
	for(int i = 0; i < mat_list.count; i++) {
		char path[128] = {0};
		memcpy(path, mat_list.paths[i], strlen(mat_list.paths[i]));

		char *sep = strrchr(path, '/');
		*sep = '\0';

		char *format = sep + 1;
		char *dot = strrchr(format, '.');
		*dot = '\0';

		HashInsert(&material_hashmap, format, i);

		textures[i] = LoadTexture(mat_list.paths[i]);

		materials[i] = LoadMaterialDefault();
		materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = textures[i];
		materials[i].params[0] = 1;
	}

	return data;
}

void UnloadBsp(Bsp_Data *data) {
	if(data->planes)		free(data->planes);
	if(data->miptex)		free(data->miptex);
	if(data->verts)			free(data->verts);
	if(data->vis)			free(data->vis);
	if(data->nodes)			free(data->nodes);
	if(data->clipnodes)		free(data->clipnodes);
	if(data->edges)			free(data->edges);
	if(data->ledges)		free(data->ledges);
	if(data->faces)			free(data->faces);
	if(data->surfaces)		free(data->surfaces);
	if(data->models)		free(data->models);

	if(data->textures) {
		for(int i = 0; i < data->num_miptex; i++)
			UnloadTexture(data->textures[i]);

		free(data->textures);
	}
}

Bsp_Hull Bsp_BuildHull(Bsp_Data *data, int hull_index) {
	Bsp_Hull hull = (Bsp_Hull) {0};

	hull.nodes = data->clipnodes;
	hull.first_node = data->models[0].head_nodes[hull_index];
	hull.last_node = data->num_clipnodes - 1;

	hull.nodes = data->clipnodes;
	hull.planes = data->planes;

	return hull;
}

int Bsp_PointContents(Bsp_Hull *hull, int num, Vector3 point) {
	float d;
	Bsp_ClipNode *node;
	Bsp_Plane *plane;

	while(num >= 0) {
		if(num < hull->first_node || num > hull->last_node) {
			MessageError("Bsp_PointContents()", "bad node number");
			printf("node_num: %d\n", num);
			return 0;
		}

		node = &hull->nodes[num];
		plane = &hull->planes[node->planenum];

		Vector3 normal = (Vector3) { plane->normal[0], plane->normal[1], plane->normal[2] };
		d = Vector3DotProduct(normal, point) - plane->dist;

		if(d < 0)
			num = node->children[1];
		else 
			num = node->children[0];
	}

	return num;
}

bool Bsp_RecursiveTrace(Bsp_Hull *hull, int node_num, Vector3 point_A, Vector3 point_B, Vector3 *intersection) {
	Bsp_ClipNode *node;
	Bsp_Plane *plane;
	float tA, tB;
	float fraction;
	
	// Handle leaves	
	if(node_num < 0) {
		if(node_num == CONTENTS_SOLID) {
			*intersection = point_A;
			return true;
		}
		return false;
	}

	node = &hull->nodes[node_num];
	plane = &hull->planes[node->planenum];

	tA = Vector3DotProduct( (Vector3) { plane->normal[0], plane->normal[1], plane->normal[2] }, point_A ) - plane->dist;	
	tB = Vector3DotProduct( (Vector3) { plane->normal[0], plane->normal[1], plane->normal[2] }, point_B ) - plane->dist;	

	// Handle cases where the line falls entirely within a single child
	if(tA >= 0 && tB >= 0)
		return Bsp_RecursiveTrace(hull, node->children[0], point_A, point_B, intersection);

	if(tA < 0 && tB < 0)
		return Bsp_RecursiveTrace(hull, node->children[1], point_A, point_B, intersection);

	// Find point of intersection with split plane
	fraction = tA / (tA - tB);
	fraction = Clamp(fraction, 0.0f, 1.0f);

	float3 m = {0};
	float3 a = Vector3ToFloatV(point_A);
	float3 b = Vector3ToFloatV(point_B);

	for(short i = 0; i < 3; i++)
		m.v[i] = a.v[i] + fraction*(b.v[i] - a.v[i]);

	Vector3 mid = (Vector3) { m.v[0], m.v[1], m.v[2] };

	short side = (tA >= 0) ? 0 : 1;
	if(Bsp_RecursiveTrace(hull, node->children[side], point_A, mid, intersection))
		return true;

	return Bsp_RecursiveTrace(hull, node->children[1 - side], mid, point_B, intersection);
}

#define	DIST_EPSILON	(0.03125)
Bsp_TraceData Bsp_TraceDataEmpty() {
	Bsp_TraceData data = {0};
	data.all_solid = true;
	data.fraction = 1;
	return data;
}

bool Bsp_RecursiveTraceEx(Bsp_Hull *hull, int node_num, float p1_frac, float p2_frac, Vector3 p1, Vector3 p2, Bsp_TraceData *trace) {
	Bsp_ClipNode *node;
	Bsp_Plane *plane;
	float t1, t2;
	Vector3 mid;
	int side;
	float mid_frac;
	float frac;

	// Check for empty
	if(node_num < 0) {
		if(node_num != CONTENTS_SOLID) {
			trace->all_solid = false;

			if(node_num == CONTENTS_EMPTY)
				trace->in_open = true;
			else 	
				trace->in_water = true;
		} else 
			trace->start_solid = true;

		return true;	// Empty
	}

	if(node_num < hull->first_node || node_num > hull->last_node) {
		MessageError("Bsp_RecursiveTraceEx", "bad node number");
		printf("node_num: %d\n", node_num);
		return true;
	}

	node = &hull->nodes[node_num];
	plane = &hull->planes[node->planenum];

	Vector3 norm = (Vector3) { plane->normal[0], plane->normal[1], plane->normal[2] };
	if(plane->type < 3) {
		float3 p1_f3 = Vector3ToFloatV(p1);
		float3 p2_f3 = Vector3ToFloatV(p2);

		t1 =  p1_f3.v[plane->type] - plane->dist;
		t2 =  p2_f3.v[plane->type] - plane->dist;

		p1 = (Vector3) { p1_f3.v[0], p1_f3.v[1], p1_f3.v[2] };
		p2 = (Vector3) { p2_f3.v[0], p2_f3.v[1], p2_f3.v[2] };

	} else {

		t1 = Vector3DotProduct(norm, p1) - plane->dist;
		t2 = Vector3DotProduct(norm, p2) - plane->dist;
	}

	if(t1 >= 0 && t2 >= 0)
		return Bsp_RecursiveTraceEx(hull, node->children[0], p1_frac, p2_frac, p1, p2, trace);
	if(t1 < 0 && t2 < 0)
		return Bsp_RecursiveTraceEx(hull, node->children[1], p1_frac, p2_frac, p1, p2, trace);

	if(t1 < 0)
		frac = (t1 + DIST_EPSILON) / (t1 - t2);
	else 
		frac = (t1 - DIST_EPSILON) / (t1 - t2);

	if(frac < 0)
		frac = 0;

	if(frac > 1)
		frac = 1;

	mid_frac = p1_frac + (p2_frac - p1_frac) * frac; 

	float3 m = {0};
	float3 p1_f3 = Vector3ToFloatV(p1);
	float3 p2_f3 = Vector3ToFloatV(p2);
	for(short i = 0; i < 3; i++) 
		m.v[i] = p1_f3.v[i] + frac*(p2_f3.v[i] - p1_f3.v[i]);

	mid = (Vector3) { m.v[0], m.v[1], m.v[2] };

	side = (t1 < 0);

	// Move up to node
	if(!Bsp_RecursiveTraceEx(hull, node->children[side], p1_frac, mid_frac, p1, mid, trace))
		return false;

	// Go past node
	if(Bsp_PointContents(hull, node->children[side^1], mid) != CONTENTS_SOLID)
		return Bsp_RecursiveTraceEx(hull, node->children[side^1], mid_frac, p2_frac, mid, p2, trace);

	// Never go out of solid area
	if(trace->all_solid)
		return false;

	if(!side) {
		memcpy(trace->plane.normal, plane->normal, sizeof(float) * 3);
		trace->plane.dist = plane->dist;

	} else {
		for(short i = 0; i < 3; i++)
			trace->plane.normal[i] = -plane->normal[i];
		
		trace->plane.dist = -plane->dist;
	}

	// Shouldn't happen but does sometimes
	while(Bsp_PointContents(hull, node_num, mid) == CONTENTS_SOLID) {
		frac -= 0.1f;

		if(frac < 0) {
			trace->fraction = mid_frac;
			trace->point = mid;
			return false;
		}

		mid_frac = p1_frac + (p2_frac - p1_frac) * frac;

		float3 m = {0};
		float3 p1_f3 = Vector3ToFloatV(p1);
		float3 p2_f3 = Vector3ToFloatV(p2);
		for(short i = 0; i < 3; i++) 
			m.v[i] = p1_f3.v[i] + frac*(p2_f3.v[i] - p1_f3.v[i]);

		mid = (Vector3) { m.v[0], m.v[1], m.v[2] };
	}

	trace->fraction = mid_frac;
	trace->point = mid;

	return false;
}

int Bsp_FindLeaf(Bsp_Data *bsp, Vector3 point) {
	int node_num = bsp->models[0].head_nodes[0];

	while(node_num >= 0) {
		Bsp_Node *node = &bsp->nodes[node_num];
		Bsp_Plane *plane = &bsp->planes[node->planenum];

		Vector3 normal = (Vector3) { plane->normal[0], plane->normal[1], plane->normal[2] };
		float d = Vector3DotProduct(normal, point) - plane->dist;

		node_num = (d >= 0) ? node->children[0] : node->children[1];
	}
	return ~node_num;
}

bool Bsp_LeafVisible(Bsp_Data *bsp, int curr_leaf, int test_leaf) {
	if(curr_leaf == test_leaf)
		return true;

	Bsp_Leaf *leaf = &bsp->leaves[curr_leaf];

	// No vis data, default to drawing
	if(leaf->visofs < 0)
		return true;

	u8 *vis = bsp->vis + leaf->visofs;

	// Decompress and test bitmask
	int leafnum = 1;
	while(leafnum < bsp->num_leaves) {
		if(*vis == 0) {
			// Skip
			vis++;
			leafnum += *vis * 8;
			vis++;
		} else {
			// Test each bit in byte
			for(int bit = 0; bit < 8; bit++) {
				if(leafnum == test_leaf) {
					return (*vis >> bit) & 1;
				}
				leafnum++;
			}
			vis++;
		}
	}

	return false;
}

void Bsp_PrintStructSizes() {
	printf("box: %zu bytes\n", sizeof(Bsp_Box32));
	printf("face: %zu bytes\n", sizeof(Bsp_Face));
	printf("leaf: %zu bytes\n", sizeof(Bsp_Leaf));
	printf("miptex: %zu bytes\n", sizeof(Bsp_Miptex));
}

Model *BspLeafToModels(Bsp_Data *bsp, Bsp_Leaf *leaf, int *out_count) {
    int max_tex_id = 0;
    for (int i = 0; i < leaf->num_faces; i++) {
        int face_id = bsp->lfaces[leaf->first_face + i];
        Bsp_Face *face = &bsp->faces[face_id];
        Bsp_Surface *surf = &bsp->surfaces[face->texinfo];
        if (surf->texture_id > max_tex_id)
            max_tex_id = surf->texture_id;
    }
    int tex_count = max_tex_id + 1;

    int *tri_counts = calloc(tex_count, sizeof(int));

    for (int i = 0; i < leaf->num_faces; i++) {
        int face_id = bsp->lfaces[leaf->first_face + i];
        Bsp_Face *face = &bsp->faces[face_id];
        Bsp_Surface *surf = &bsp->surfaces[face->texinfo];
        tri_counts[surf->texture_id] += face->edge_count - 2;
    }

    int used_count = 0;
    for (int i = 0; i < tex_count; i++)
        if (tri_counts[i] > 0) used_count++;

    *out_count = used_count;
    if (used_count == 0) {
        free(tri_counts);
        return NULL;
    }

    int *tex_to_slot = malloc(tex_count * sizeof(int));
    for (int i = 0; i < tex_count; i++) tex_to_slot[i] = -1;

    Mesh *meshes = calloc(used_count, sizeof(Mesh));
    int  *slot_tex_ids = malloc(used_count * sizeof(int));  // slot -> tex_id

    int slot = 0;
    for (int i = 0; i < tex_count; i++) {
        if (tri_counts[i] == 0) continue;
        tex_to_slot[i] = slot;
        slot_tex_ids[slot] = i;

        meshes[slot].triangleCount = tri_counts[i];
        meshes[slot].vertexCount   = tri_counts[i] * 3;
        meshes[slot].vertices  = MemAlloc(sizeof(float) * meshes[slot].vertexCount * 3);
        meshes[slot].texcoords = MemAlloc(sizeof(float) * meshes[slot].vertexCount * 2);
        meshes[slot].normals   = MemAlloc(sizeof(float) * meshes[slot].vertexCount * 3);
        slot++;
    }

    int *vert_cursors = calloc(used_count, sizeof(int));  // current vert index per slot

    for (int i = 0; i < leaf->num_faces; i++) {
        int face_id = bsp->lfaces[leaf->first_face + i];
        Bsp_Face *face = &bsp->faces[face_id];
        Bsp_Surface *surf = &bsp->surfaces[face->texinfo];
        Bsp_Miptex *miptex = &bsp->miptex[surf->texture_id];

        int s = tex_to_slot[surf->texture_id];
        Mesh *mesh = &meshes[s];
        int *vi = &vert_cursors[s];

        Vector3 face_verts[face->edge_count];
        for (int j = 0; j < face->edge_count; j++) {
            i32 ledge = bsp->ledges[face->first_edge + j];
            face_verts[j] = (ledge >= 0)
                ? bsp->verts[bsp->edges[ ledge].v[0]]
                : bsp->verts[bsp->edges[-ledge].v[1]];
        }

        Bsp_Plane *plane = &bsp->planes[face->plane];
        Vector3 normal = { plane->normal[0], plane->normal[1], plane->normal[2] };
        if (face->side) normal = Vector3Negate(normal);

        for (int j = 1; j < face->edge_count - 1; j++) {
            Vector3 tri[3] = { face_verts[0], face_verts[j], face_verts[j+1] };

            for (int k = 0; k < 3; k++) {
                mesh->vertices[*vi * 3 + 0] = tri[k].x;
                mesh->vertices[*vi * 3 + 1] = tri[k].y;
                mesh->vertices[*vi * 3 + 2] = tri[k].z;

                mesh->normals[*vi * 3 + 0] = normal.x;
                mesh->normals[*vi * 3 + 1] = normal.y;
                mesh->normals[*vi * 3 + 2] = normal.z;

                float u = (Vector3DotProduct(tri[k], surf->vector_s) + surf->dist_s) / miptex->width;
                float v = (Vector3DotProduct(tri[k], surf->vector_t) + surf->dist_t) / miptex->height;

                mesh->texcoords[*vi * 2 + 0] = u;
                mesh->texcoords[*vi * 2 + 1] = v;

                (*vi)++;
            }
        }
    }

    Model *models = malloc(used_count * sizeof(Model));

    for (int i = 0; i < used_count; i++) {
        UploadMesh(&meshes[i], false);
        models[i] = LoadModelFromMesh(meshes[i]);

        int tid = slot_tex_ids[i];
        if (tid < bsp->num_miptex && bsp->textures[tid].id != 0) {
			models[i].materials[0] = materials[HashFetch(&material_hashmap, bsp->miptex[tid].name)];
        }
    }

    free(tri_counts);
    free(tex_to_slot);
    free(slot_tex_ids);
    free(vert_cursors);
    free(meshes);

    return models;
}

