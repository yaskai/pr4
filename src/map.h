#include "raylib.h"
#include "../include/num_redefs.h"
#include "geo.h"

#ifndef MAP_H_
#define MAP_H_

typedef struct {
	BoundingBox bounds;

	Vector3 verts[24];
	Plane planes[18];

	Vector3 center;

	u16 vert_count;
	u16 plane_count;

} Brush;

typedef struct {
	Brush *brushes;
	
	BoundingBox bounds;
	
	u16 count;

} BrushPool;

void LoadMapFile(BrushPool *brush_pool, char *path, Model *map_model);
BrushPool ExpandBrushes(BrushPool *brush_pool, Vector3 aabb_extents);

typedef struct {
	Vector3 p;
	float a;

} FaceVert;

Tri *BrushToTris(Brush *brush, u16 *count, u16 brush_id);
Tri *TrisFromBrushPool(BrushPool *brush_pool, u16 *count);

void BrushTestView(BrushPool *brush_pool, Color color);

MapSection BuildMapSect(char *file_path);

#endif

