#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "kbsp.h"
#include "../include/log_message.h"

Bsp_Data LoadBsp(char *path, bool print_output) {
	Bsp_Data data = (Bsp_Data) {0};

	FILE *pF = fopen(path, "rb");

	if(!pF) {
		MessageError("ERROR: Could not load file ", path);
		return data;
	}

	Bsp_Header header = {0};
	fread(&header, sizeof(header), 1, pF);

	if(header.version != BSP_VERSION) {
		MessageError("ERROR: BSP version mismatch", NULL);
		return data;
	}

	printf("%d\n", header.version);

	// ---------------------------------------------------------------------------------------
	// Planes
	Bsp_Lump planes_lump = header.lumps[LUMP_PLANES];
	if(planes_lump.file_size < sizeof(Bsp_Lump)) {
		MessageError("ERROR: Lump size mismatch", NULL);
		return data;
	}

	fseek(pF, planes_lump.file_offset, SEEK_SET);
	i32 plane_count = planes_lump.file_size / sizeof(Bsp_Plane);  
	Bsp_Plane planes[plane_count];
	fread(&planes, sizeof(planes), 1, pF);
	if(print_output) {
		for(i32 i = 0; i < plane_count; i++) {
			Bsp_Plane *p = &planes[i];
			printf("{ %f, %f, %f }\n", p->normal[0], p->normal[1], p->normal[2]);
		}
	}

	data.num_planes = plane_count;
	data.planes = malloc(sizeof(Bsp_Plane) * data.num_planes);
	memcpy(data.planes, planes, sizeof(planes));

	// ---------------------------------------------------------------------------------------
	// Nodes
	Bsp_Lump nodes_lump = header.lumps[LUMP_NODES];
	if(nodes_lump.file_size < sizeof(Bsp_Lump)) {
		MessageError("ERROR: Lump size mismatch", NULL);
		return data;
	}

	fseek(pF, nodes_lump.file_offset, SEEK_SET);
	i32 node_count = nodes_lump.file_size / sizeof(Bsp_Node);
	Bsp_Node nodes[node_count];
	fread(&nodes, sizeof(nodes), 1, pF);

	data.num_nodes = node_count;
	data.nodes = malloc(sizeof(Bsp_Node) * data.num_nodes);
	memcpy(data.nodes, nodes, sizeof(nodes));

	// ---------------------------------------------------------------------------------------
	// Clip nodes
	Bsp_Lump clipnodes_lump = header.lumps[LUMP_CLIPNODES];
	if(clipnodes_lump.file_size < sizeof(Bsp_Lump)) {
		MessageError("ERROR: Lump size mismatch", NULL);
		return data;
	}

	fseek(pF, clipnodes_lump.file_offset, SEEK_SET);
	i32 clipnode_count = clipnodes_lump.file_size / sizeof(Bsp_ClipNode);
	Bsp_ClipNode clipnodes[clipnode_count];
	fread(&clipnodes, sizeof(clipnodes), 1, pF);
	if(print_output) {
		for(i32 i = 0; i < clipnode_count; i++) {
			Bsp_ClipNode *node = &clipnodes[i];
			printf("planenum: %d\n", node->planenum);
			printf("front: %d\n", node->children[0]);
			printf("back: %d\n", node->children[1]);
		}
	}

	data.num_clipnodes = clipnode_count;
	data.clipnodes = malloc(sizeof(Bsp_ClipNode) * data.num_clipnodes);
	memcpy(data.clipnodes, clipnodes, sizeof(clipnodes));

	// ---------------------------------------------------------------------------------------
	// Models
	Bsp_Lump models_lump = header.lumps[LUMP_MODELS];
	if(models_lump.file_size < sizeof(Bsp_Lump)) {
		MessageError("ERROR: Lump size mismatch", NULL);
		return data;
	}

	fseek(pF, models_lump.file_offset, SEEK_SET);
	i32 model_count = models_lump.file_size / sizeof(Bsp_Model);
	Bsp_Model models[model_count];
	fread(&models, sizeof(models), 1, pF);
	
	data.num_models = model_count;
	data.models = malloc(sizeof(Bsp_Model) * data.num_models);
	memcpy(data.models, models, sizeof(models));

	for(int i = 0; i < 4; i++) {
		printf("head: %d\n", data.models[0].head_nodes[i]);
	}

	fclose(pF);
	
	return data;
}

Bsp_Hull Bsp_BuildHull(Bsp_Data *data, int hull_index) {
	Bsp_Hull hull = (Bsp_Hull) {0};

	/*
	int nodes_per_hull = data->num_clipnodes / 4;
	hull.nodes = data->clipnodes;
	hull.first_node = hull_index * nodes_per_hull;
	hull.last_node = (hull_index == 3) ? data->num_clipnodes - 1 : hull.first_node + nodes_per_hull - 1;
	*/

	hull.nodes = data->clipnodes;
	hull.first_node = data->models[0].head_nodes[hull_index];
	hull.last_node = data->num_clipnodes - 1;
	//if(hull_index <= 3) hull.last_node = data->models[0].head_nodes[hull_index + 1] - 1;

	hull.nodes = data->clipnodes;
	hull.planes = data->planes;

	//hull.nodes = calloc(data->num_clipnodes, sizeof(Bsp_ClipNode));	
	//memcpy(hull.nodes, data->clipnodes, sizeof(Bsp_ClipNode) * data->num_clipnodes);

	//hull.num_planes = data->num_planes;
	//hull.planes = malloc(sizeof(Bsp_Plane) * hull.num_planes);
	//memcpy(hull.planes, data->planes, sizeof(Bsp_Plane) * hull.num_planes);

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
/*
bool Bsp_RecursiveTraceEx(Bsp_Hull *hull, int node_num, float p1_frac, float p2_frac, Vector3 p1, Vector3 p2, Bsp_TraceData *trace) {
	Bsp_ClipNode *node;
	Bsp_Plane *plane;
	float t1, t2;
	float fraction;
	
	// Handle leaves	
	if(node_num < 0) {
		if(node_num == CONTENTS_SOLID) {
			trace->point = p1;
			return true;
		}
		return false;
	}

	node = &hull->nodes[node_num];
	plane = &hull->planes[node->planenum];

	t1 = Vector3DotProduct( (Vector3) { plane->normal[0], plane->normal[1], plane->normal[2] }, p1 ) - plane->dist;	
	t2 = Vector3DotProduct( (Vector3) { plane->normal[0], plane->normal[1], plane->normal[2] }, p2 ) - plane->dist;	

	// Handle cases where the line falls entirely within a single child
	if(t1 >= 0 && t2 >= 0)
		return Bsp_RecursiveTrace(hull, node->children[0], p1, p2, &trace->point);

	if(t1 < 0 && t2 < 0)
		return Bsp_RecursiveTrace(hull, node->children[1], p1, p2, &trace->point);

	// Find point of intersection with split plane
	fraction = t1 / (t1 - t2);
	fraction = Clamp(fraction, 0.0f, 1.0f);

	float3 m = {0};
	float3 a = Vector3ToFloatV(p1);
	float3 b = Vector3ToFloatV(p2);

	for(short i = 0; i < 3; i++)
		m.v[i] = a.v[i] + fraction*(b.v[i] - a.v[i]);

	Vector3 mid = (Vector3) { m.v[0], m.v[1], m.v[2] };

	short side = (t1 >= 0) ? 0 : 1;
	if(Bsp_RecursiveTrace(hull, node->children[side], p1, mid, &trace->point))
		return true;

	return Bsp_RecursiveTrace(hull, node->children[1 - side], mid, p2, &trace->point);
}
*/

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
	t1 = Vector3DotProduct(norm, p1) - plane->dist;
	t2 = Vector3DotProduct(norm, p2) - plane->dist;

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

	mid_frac = p1_frac + (p1_frac - p2_frac) * frac; 

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

