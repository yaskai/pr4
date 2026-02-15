#include <math.h>
#include <string.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include "geo.h"
#include "../include/sort.h"
#include "../include/log_message.h"

#define PLANE_EPS 0.001f

Plane BuildPlane(Vector3 v0, Vector3 v1, Vector3 v2) {
	Vector3 edge_0 = Vector3Subtract(v1, v0);
	Vector3 edge_1 = Vector3Subtract(v2, v0);

	Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge_0, edge_1));
	float distance = -Vector3DotProduct(normal, v0);
	
	return (Plane) { .normal = normal, .d = distance };
}

Vector3 BrushCenter(Brush *brush) {
	float3 min = Vector3ToFloatV(Vector3Scale(Vector3One(),  FLT_MAX));
	float3 max = Vector3ToFloatV(Vector3Scale(Vector3One(), -FLT_MAX));
	
	for(u16 i = 0; i < brush->vert_count; i++) {
		float3 point = Vector3ToFloatV(brush->verts[i]);

		for(short a = 0; a < 3; a++) {
			if(point.v[a] < min.v[a])	
				min.v[a] = point.v[a];

			if(point.v[a] > max.v[a])	
				max.v[a] = point.v[a];
		}
	}

	float3 out = {0};
	for(short a = 0; a < 3; a++)
		out.v[a] = (min.v[a] + max.v[a]) * 0.5f;

	return (Vector3) {
		.x = out.v[0],
		.y = out.v[1],
		.z = out.v[2]
	};
}

short ThreePlaneIntersect(Plane a, Plane b, Plane c, Vector3 *v) {
	float denom = Vector3DotProduct(a.normal, Vector3CrossProduct(b.normal, c.normal));
	if(fabsf(denom) < PLANE_EPS) return 0;

	Vector3 tA = Vector3Scale(Vector3CrossProduct(b.normal, c.normal), a.d);
	Vector3 tB = Vector3Scale(Vector3CrossProduct(c.normal, a.normal), b.d);
	Vector3 tC = Vector3Scale(Vector3CrossProduct(a.normal, b.normal), c.d);

	*v = Vector3Scale(Vector3Add(Vector3Add(tA, tB), tC), 1.0f / denom);
	return 1;
} 

Vector3 *CollectTriplets(Brush *brush, u8 *t_count) {
	u8 count = 0;
	Vector3 *vertices = calloc(64, sizeof(Vector3));

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
						if(Vector3DotProduct(plane.normal, v) - plane.d > 0) {
							in = 0;
							break;
						}
					}
					if(in) {
						bool unique = true;
						for(u8 t = 0; t < count; t++) {
							if(Vector3Distance(vertices[t], v) < 0.01f) {
								unique = false;
								break;
							}
						}
						if(unique)
							vertices[count++] = v;
					}
				}
			}
		}
	}

	*t_count = count;
	return vertices;
};

void BrushGetVertices(Brush *brush) {
	// Collect triple-plane intersections
	u8 count = 0;
	Vector3 *vertices = CollectTriplets(brush, &count);

	// Collect edge intersections
	for(u8 i = 0; i < brush->plane_count; i++) {
		for(u8 j = i +1; j < brush->plane_count; j++) {
			Plane *pl_A = &brush->planes[i];
			Plane *pl_B = &brush->planes[j];

			Vector3 edge_dir = Vector3CrossProduct(pl_A->normal, pl_B->normal);
			if(Vector3Length(edge_dir) < 0.001f) continue;

			for(u8 k = 0; k < brush->plane_count; k++) {
				if(k == i || k == j) continue;

				Vector3 v = (Vector3) {0};
				if(ThreePlaneIntersect(*pl_A, *pl_B, brush->planes[k], &v)) {
					i8 in = true;
					for(u8 p = 0; p < brush->plane_count; p++) {
						if(Vector3DotProduct(brush->planes[p].normal, v) - brush->planes[p].d > PLANE_EPS) {
							in = false;
							break;
						}
					}

					if(in) {
						bool unique = true;
						for(u8 t = 0; t < count; t++) {
							if(Vector3Distance(vertices[t], v) < 0.1f) {
								unique = false;
								break;
							}
						}
						if(unique) 
							vertices[count++] = v;
					}
				}
			}
		}
	}

	for(u8 i = 0; i < count; i++) {
		Vector3 v = vertices[i];
		
		brush->bounds.min = Vector3Min(brush->bounds.min, v);
		brush->bounds.max = Vector3Max(brush->bounds.max, v);
	}

	brush->center = BoxCenter(brush->bounds);

	brush->vert_count = count;
	memcpy(brush->verts, vertices, sizeof(Vector3) * count);

	free(vertices);
}

#define PARSE_NONE -1
#define PARSE_BRUSH 0
#define PARSE_ENT 	1
void LoadMapFile(BrushPool *brush_pool, char *path, Model *map_model, SpawnList *spawn_list) {
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
	int curr_ent = 0;

	short parse_mode = -1;

	char line[256];
	while(fgets(line, sizeof(line), pF)) {
		Brush *brush = &brush_pool->brushes[curr_brush];
		EntSpawn *curr_entspawn = &spawn_list->arr[curr_ent];

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
				spawn_list->count++;
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
			if(line[0] == '{' || line[0] == '}' || line[0] == '/') continue;
			// * TODO
			// *
			char *tok = strtok(line, "\"");
			printf("%s, ", tok);

			if(strcmp(tok, "origin") == 0) {
				char *end = line + sizeof(tok) + 2;
				tok = strtok(end, "\"");
				printf("%s\n", tok);
				sscanf(tok, "%f %f %f", &curr_entspawn->position.x, &curr_entspawn->position.z, &curr_entspawn->position.y);
				curr_entspawn->position.z *= -1;
			}

			if(strcmp(tok, "enum_id") == 0) {
				char *end = line + sizeof(tok) + 2;
				tok = strtok(end, "\"");
				printf("%s\n", tok);
				sscanf(tok, "%d", &curr_entspawn->ent_type);
			}

			if(strcmp(tok, "angle") == 0) {
				char *end = line + 8;
				tok = strtok(end, "\"");
				printf("%s\n", tok);
				sscanf(tok, "%d", &curr_entspawn->angle);
			}
		}
	}

	fclose(pF);

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

	if(GetLogState()) {
		Message("--------------- [ ENTITIES ] -----------------", ANSI_GREEN);
		for(int i = 0; i < spawn_list->count; i++) {
			printf("-----------------------\n");
			printf("type: %d\n", spawn_list->arr[i].ent_type);
			printf("pos: { %f %f %f }\n", spawn_list->arr[i].position.x, spawn_list->arr[i].position.y, spawn_list->arr[i].position.z);
			printf("angle: %d\n", spawn_list->arr[i].angle);
		}
		Message("----------------------------------------------", ANSI_GREEN);
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

Tri *BrushToTris(Brush *brush, u16 *count, u16 brush_id) {
	Tri *tris = calloc(32, sizeof(Tri));
	u16 tri_count = 0;

	for(u8 i = 0; i < brush->plane_count; i++) {
		FaceVert face_verts[64] = {0};
		u8 fv_count = 0;

	 	Plane *plane = &brush->planes[i];

		for(u8 j = 0; j < brush->vert_count; j++) {
			Vector3 v = brush->verts[j];

			bool in = (fabsf(Vector3DotProduct(plane->normal, v) - plane->d) <= 0.01f);
			if(!in) continue;

			face_verts[fv_count++].p = v;
		}

		if(fv_count < 3) continue;

		Vector3 center = Vector3Zero();
		for(u8 j = 0; j < fv_count; j++) 
			center = Vector3Add(center, face_verts[j].p);
		center = Vector3Scale(center, 1.0f / fv_count);		

		Vector3 u = Vector3Normalize(
			(fabsf(plane->normal.x) > 0.9f) ? 
			Vector3CrossProduct(plane->normal, UP) :
			Vector3CrossProduct(plane->normal, (Vector3) { 1, 0, 0 } )
		);
		Vector3 v = Vector3CrossProduct(plane->normal, u); 	

		for(u8 j = 0; j < fv_count; j++) {
			Vector3 d = Vector3Subtract(face_verts[j].p, center);
			float x = Vector3DotProduct(d, u);
			float y = Vector3DotProduct(d, v);
			face_verts[j].a = atan2f(y, x);
		}

		for(u8 a = 0; a < fv_count; a++) {
			for(u8 b = a+1; b < fv_count; b++) {
				if(face_verts[a].a > face_verts[b].a) {
					FaceVert temp = face_verts[b];
					face_verts[b] = face_verts[a];
					face_verts[a] = temp;
				}
			}
		}

		for(u8 j = 1; j < fv_count - 1; j++) {
			tris[tri_count++] = (Tri) {
				.vertices[0] = face_verts[0].p,
				.vertices[1] = face_verts[j].p,
				.vertices[2] = face_verts[j+1].p,
				.normal = plane->normal,
				.hull_id = brush_id
			};
		}
	}
	
	*count = tri_count;

	/*
	for(short i = 0; i < tri_count; i++) {
		printf("\ntri[%d]\n", i);

		for(short j = 0; j < 3; j++) {
			float x = tris[i].vertices[j].x, y = tris[i].vertices[j].y, z = tris[i].vertices[j].z;
			//printf("vtx[%d]: { %f, %f, %f } \n", j, x, y, z);
		}

		float nx = tris[i].normal.x, ny = tris[i].normal.y, nz = tris[i].normal.z;
		//printf("normal: { %f, %f, %f }\n", nx, ny, nz);
	}
	*/

 	if(tri_count)
		tris = realloc(tris, sizeof(Tri) * tri_count);

	return tris;
}

Tri *TrisFromBrushPool(BrushPool *brush_pool, u16 *count) {
	u16 tri_count = 0;
	u16 tri_cap = 1024;
	Tri *tris = calloc(tri_cap, sizeof(Tri));

	for(u16 i = 0; i < brush_pool->count; i++) {
		Brush *brush = &brush_pool->brushes[i];

		u16 temp_count = 0;
		Tri *brush_tris = BrushToTris(brush, &temp_count, i);

		if(tri_count + temp_count > tri_cap) {
			tri_cap = (tri_cap << 1);
			tris = realloc(tris, sizeof(Tri) * tri_cap);
		}

		memcpy(tris + tri_count, brush_tris, sizeof(Tri) * temp_count);
		tri_count += temp_count;

		free(brush_tris);
	}

	*count = tri_count;
	tri_cap = tri_count;
	if(tri_count) tris = realloc(tris, sizeof(Tri) * tri_cap);
	return tris;
}

void BrushTestView(BrushPool *brush_pool, Color color) {
	for(u16 i = 0; i < brush_pool->count; i++) {
		Brush *brush = &brush_pool->brushes[i];

		//DrawBoundingBox(brush->bounds, SKYBLUE);
		
		for(short j = 0; j < brush->vert_count; j++) {
			//DrawSphere( (Vector3) { brush->verts[j].x, brush->verts[j].y, brush->verts[j].z }, 5, SKYBLUE);
			//Vector3 v = Vector3Negate(brush->verts[j]);
			Vector3 v = brush->verts[j];
		}
	}
}

// * NOTE:
// The plane math for expanding the hulls is completely unsusable and fucked for use in in-game tracing. 
// However, the tris do build correctly it's just the end planes (terrible coordinate coversion from id format /:)
// A hacky solution could be to reconstruct the planes *post* tri construction and remove the duplicates...  2 tris = 1 plane
MapSection BuildMapSect(char *path, SpawnList *spawn_list) {
	MessageDiag("Constructing map section", path, ANSI_BLUE);

	MapSection sect = (MapSection) {0};

	if(!DirectoryExists(path)) {
		MessageError("Missing directory", path);
		return sect;
	}

	FilePathList path_list = LoadDirectoryFiles(path);

	// 1. Load 3d model, rendering
	Message("Loading model...", ANSI_BLUE);
	short model_id = -1;
	for(short i = 0; i < path_list.count; i++) if(strcmp(GetFileExtension(path_list.paths[i]), ".glb") == 0) model_id = i;

	// No model, exit
	if(model_id == -1) {
		MessageError("Missing model", NULL);
		return sect;
	}
	
	Model model = LoadModel(path_list.paths[model_id]);
	sect.model = model;

	//TriPool tri_pools[3] = {0};
	//tri_pools[0].arr = ModelToTris(model, &tri_pools[0].count, &tri_pools[0].ids);
	//tri_pools[0].arr = ModelToTris(model, &tri_pools[0].count, &tri_pools[0].ids);
	sect._tris[0].arr = ModelToTris(model, &sect._tris[0].count, &sect._tris[0].ids);
	if(GetLogState()) printf("model tri_count: %d\n", sect._tris[0].count);

	// 2. Load .map file, collision, physics, ai logic, etc. 
	Message("Loading map file...", ANSI_BLUE);
	short mpf_id = -1;
	for(short i = 0; i < path_list.count; i++) if(strcmp(GetFileExtension(path_list.paths[i]), ".map") == 0) mpf_id = i;

	// No .map, exit
	if(mpf_id == -1) { 
		MessageError("Missing .map file", NULL);
		return sect;
	}

	*spawn_list = (SpawnList) {
		.count = 0,
		.capacity = 128,
	};
	spawn_list->arr = calloc(spawn_list->capacity, sizeof(EntSpawn));

	BrushPool brush_pools[3] = {0};
	LoadMapFile(&brush_pools[0], path_list.paths[mpf_id], &model, spawn_list);

	// 3. Build expanded geometry for character to world collsions 
	for(short i = 1; i < 3; i++) {
		brush_pools[i].count = brush_pools[0].count;
		brush_pools[i].brushes = calloc(brush_pools[i].count, sizeof(Brush));
		memcpy(brush_pools[i].brushes, brush_pools[0].brushes, sizeof(Brush) * brush_pools[0].count);

		// Expand brush planes/vertices
		BrushPool exp = ExpandBrushes(&brush_pools[i], BODY_VOLUME_MEDIUM);

		// Extract tris
		sect._tris[i].arr = TrisFromBrushPool(&exp, &sect._tris[i].count);
		sect._tris[i].ids = calloc(sect._tris[i].count, sizeof(u16));
		for(u16 j = 0; j < sect._tris[i].count; j++) sect._tris[i].ids[j] = j;
	}

	// 3. Construct BVH trees for each geometry set
	for(short i = 0; i < 2; i++) {
		BvhTree *bvh = &sect.bvh[i];
		bvh->tris = (TriPool) {0};
		bvh->tris.count = sect._tris[i].count;
		bvh->tris.arr = calloc(sect._tris[i].count, sizeof(Tri));
		bvh->tris.ids = calloc(sect._tris[i].count, sizeof(u16));
		for(u16 j = 0; j < sect._tris[i].count; j++) {
			bvh->tris.arr[j] = sect._tris[i].arr[j];
			bvh->tris.ids[j] = sect._tris[i].ids[j];
		}

		BvhConstruct(&sect, &sect.bvh[i], Vector3Zero(), &sect._tris[i]);
		if(GetLogState()) printf("bvh[%d] node count: %d\n", i, sect.bvh->count);
	}
	
	// 4. Copy/convert brushes to hulls
	for(short i = 0; i < 3; i++) {
		BrushPool *bp = &brush_pools[i];

		sect._hulls[i] = (HullPool) {
			.arr = malloc(sizeof(Brush) * bp->count),
			.count = bp->count
		};

		for(u16 j = 0; j < bp->count; j++) {
			Brush *brush = &bp->brushes[j];

			Hull hull = (Hull) {
				.aabb = brush->bounds,
				.center = brush->center,
				.plane_count = brush->plane_count,
				.vert_count = brush->vert_count
			};

			memcpy(hull.planes, brush->planes, sizeof(Plane) * brush->plane_count);
			memcpy(hull.verts, brush->verts, sizeof(Vector3) * brush->vert_count);

			sect._hulls[i].arr[j] = hull;
		}

		free(bp->brushes);
	}

	return sect;
}

