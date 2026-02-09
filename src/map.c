#include <string.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include "geo.h"

int point_total = 0;
Model *brush_point_meshes;

Plane BuildPlane(Vector3 v0, Vector3 v1, Vector3 v2) {
	Vector3 edge_0 = Vector3Subtract(v1, v0);
	Vector3 edge_1 = Vector3Subtract(v2, v0);

	Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge_0, edge_1));
	float distance = -Vector3DotProduct(normal, v0);
	
	return (Plane) { .normal = normal, .d = distance };
}

Vector3 BrushCenter(Brush *brush) {
	Vector3 center = Vector3Zero();
	for(u8 i = 0; i < brush->plane_count; i++) 
		center = Vector3Add(center, Vector3Scale(brush->planes[i].normal, brush->planes[i].d));	

	return center;
}

short ThreePlaneIntersect(Plane a, Plane b, Plane c, Vector3 *v) {
	float denom = Vector3DotProduct(a.normal, Vector3CrossProduct(b.normal, c.normal));
	if(fabsf(denom) < EPSILON) return 0;

	Vector3 tA = Vector3Scale(Vector3CrossProduct(b.normal, c.normal), a.d);
	Vector3 tB = Vector3Scale(Vector3CrossProduct(c.normal, a.normal), b.d);
	Vector3 tC = Vector3Scale(Vector3CrossProduct(a.normal, b.normal), c.d);

	*v = Vector3Scale(Vector3Add(Vector3Add(tA, tB), tC), 1.0f / denom);
	return 1;
} 

void BrushGetVertices(Brush *brush) {
	u8 count = 0;
	Vector3 vertices[18];

	for(u8 i = 0; i < brush->plane_count; i++) {
		Plane a = brush->planes[i];
		for(u8 j = i+1; j < brush->plane_count; j++) {
			Plane b = brush->planes[j];
			for(u8 k = j+1; k < brush->plane_count; k++) {
				Plane c = brush->planes[k];

				Vector3 v;
				if(ThreePlaneIntersect(a, b, c, &v)) {
					i8 in = true;
					for(u8 p = 0; p < brush->plane_count; p++) {
						Plane plane = brush->planes[p];
						if(Vector3DotProduct(plane.normal, v) - plane.d > EPSILON) {
							in = 0;
							break;
						}
					}
					if(in) {
						vertices[count++] = v;
					}
				}
			}
		}
	}

	brush->vert_count = count;
	memcpy(brush->verts, vertices, sizeof(Vector3) * count);
}

#define PARSE_NONE -1
#define PARSE_BRUSH 0
#define PARSE_ENT 	1
void LoadMapFile(BrushPool *brush_pool, char *path, Model *map_model) {
	FILE *pF = fopen(path, "r");

	if(!pF) {
		printf("ERROR: No file[%s]\n", path);
		return;
	}

	brush_pool->count = 0;
	brush_pool->brushes = calloc(4096, sizeof(Brush));
	for(int i = 0; i < 4096; i++) {
		Brush *brush = &brush_pool->brushes[i];
		brush->bounds = EmptyBox();
	}

	int curr_brush = 0;
	int curr_ent = 0, ent_count = 0;

	short parse_mode = -1;

	char line[256];
	while(fgets(line, sizeof(line), pF)) {
		Brush *brush = &brush_pool->brushes[curr_brush];

		// Set mode & id of brush or entity 
		if(line[0] == '/') {
			if(line[3] == 'b') {
				parse_mode = PARSE_BRUSH;
				char *tok; 
				tok = strtok(line, " ");
				while(tok != NULL) {
					sscanf(tok, "%d", &curr_brush);
					tok = strtok(NULL, " ");
				}
				brush_pool->count++;

			} else if(line[3] == 'e') {
				parse_mode = PARSE_ENT;
				char *tok = strtok(line, " ");
				while(tok != NULL) {
					sscanf(tok, "%d", &curr_ent);
					tok = strtok(NULL, " ");
				}
				ent_count++;
			}
		}

		if(parse_mode == PARSE_NONE)
			continue;

		if(parse_mode == PARSE_BRUSH && line[0] == '(') {

			char *points_str = line;
			char *last_par = strrchr(line, ')') + 1;
			*last_par = '\0';
			
			Vector3 points[3] = {0};
			sscanf(
				points_str, 
				"( %f %f %f ) ( %f %f %f ) ( %f %f %f )", 
				&points[0].x, &points[0].z, &points[0].y,
				&points[1].x, &points[1].z, &points[1].y,
				&points[2].x, &points[2].z, &points[2].y
			);

			for(short i = 0; i < 3; i++) {
				points[i].z *= -1.0f; 
				Vector3 n = Vector3Negate(points[i]);
			}

			Plane plane = BuildPlane(points[0], points[1], points[2]);
			brush->planes[brush->plane_count++] = plane;
		}

		if(parse_mode == PARSE_ENT) {
			// * TODO
			// *
		}
	}

	fclose(pF);

	brush_point_meshes = malloc(sizeof(Model) * 1);
	Mesh mesh = GenMeshSphere(1, 8, 12);
	Model model = LoadModelFromMesh(mesh);
	brush_point_meshes[0] = model;

	for(u16 i = 0; i < brush_pool->count; i++) {
		Brush *brush = &brush_pool->brushes[i];
		// Change coordinates to x, y, z 
		// instead of: 
		// -x, -z, -y
		// 1.
		// Adjust planes 
		for(u16 j = 0; j < brush->plane_count; j++) {
			brush->planes[j].normal = Vector3Negate(brush->planes[j].normal);
		}
		// 2.
		// Build vertices, AABBs
		BrushGetVertices(brush);
		for(u16 j = 0; j < brush->vert_count; j++) {
			brush->bounds.min = Vector3Min(brush->bounds.min, brush->verts[j]);
			brush->bounds.max = Vector3Max(brush->bounds.max, brush->verts[j]);
		}
	}
}

// Expand brushes to use as fitting collision volumes 
BrushPool ExpandBrushes(BrushPool *brush_pool, Vector3 aabb_extents) {
	BrushPool exp = (BrushPool) {0};
	exp.count = brush_pool->count;
	exp.brushes = calloc(brush_pool->count, sizeof(Brush));

	// 1. Extend plane by it's normal
	Vector3 half_extents = Vector3Scale(aabb_extents, 0.5f);
	for(u16 i = 0; i < brush_pool->count; i++) {
		exp.brushes[i] = (Brush) { .plane_count = brush_pool->brushes[i].plane_count, .vert_count = brush_pool->brushes[i].vert_count };
		Brush *brush = &exp.brushes[i];

		memcpy(brush->planes, brush_pool->brushes[i].planes, sizeof(Plane) * brush->plane_count);
		memcpy(brush->verts, brush_pool->brushes[i].verts, sizeof(Vector3) * brush->vert_count);

		for(u8 j = 0; j < brush->plane_count; j++) {
			Plane *plane = &brush->planes[j];

			float diff = MinkowskiDiff(plane->normal, half_extents);
			plane->d += diff;
		}

		// 2. Rebuild vertices, AABBs
		BrushGetVertices(brush);
		for(u16 j = 0; j < brush->vert_count; j++) {
			brush->bounds.min = Vector3Min(brush->bounds.min, brush->verts[j]);
			brush->bounds.max = Vector3Max(brush->bounds.max, brush->verts[j]);
		}
	}	
	
	return exp;
}

Tri *TrisFromBrushPool(BrushPool *brush_pool, u16 *count) {
	for(u8 i = 0; i < brush_pool->count; i++) {
		
	}

	return NULL;
}

void BrushTestView(BrushPool *brush_pool, Color color) {
	for(u16 i = 0; i < brush_pool->count; i++) {
		Brush *brush = &brush_pool->brushes[i];

		//DrawBoundingBox(brush->bounds, SKYBLUE);
		
		for(short j = 0; j < brush->vert_count; j++) {
			//DrawSphere( (Vector3) { brush->verts[j].x, brush->verts[j].y, brush->verts[j].z }, 5, SKYBLUE);
			//Vector3 v = Vector3Negate(brush->verts[j]);
			Vector3 v = brush->verts[j];
			DrawModel(brush_point_meshes[0], v, 3, color);
		}
	}

	/*
	for(int i = 0; i < point_total; i++) {
		//DrawModel(brush_point_meshes[i], Vector3Zero(), 10, SKYBLUE);
		//DrawSphere(, float radius, Color color)
	}
	*/
}

