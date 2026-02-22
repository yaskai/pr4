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
#include "config.h"

#define PLANE_EPS 0.001f

rMeshCollection rmeshes_collection = {0};

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
			if(line[0] != '"') continue;

			// Get key string
			char *key = strtok(line, "\"");

			// Get value string
			char *end = &line[strlen(key) + 2];
			char *val = strchr(end, '"');
			val = strtok(val, "\"");

			MessageKeyValPair(key, val);

			if(streq(key, "classname")) {
				memcpy(&curr_entspawn->tag, val, strlen(val));
			}

			if(streq(key, "origin")) {
				sscanf(val, "%f %f %f", &curr_entspawn->position.x, &curr_entspawn->position.z, &curr_entspawn->position.y);
				curr_entspawn->position.z *= -1;
			}

			if(streq(key, "enum_id")) {
				sscanf(val, "%d", &curr_entspawn->ent_type);
			}

			if(streq(key, "angle")) {
				sscanf(val, "%d", &curr_entspawn->angle);
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

	/*
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
	*/
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
		Vector3 volume = (i == 1) ? BODY_VOLUME_MEDIUM : BODY_VOLUME_SMALL;
		BrushPool exp = ExpandBrushes(&brush_pools[i], volume);

		// Extract tris
		sect._tris[i].arr = TrisFromBrushPool(&exp, &sect._tris[i].count);
		sect._tris[i].ids = calloc(sect._tris[i].count, sizeof(u16));
		for(u16 j = 0; j < sect._tris[i].count; j++) sect._tris[i].ids[j] = j;
	}

	// 3. Construct BVH trees for each geometry set
	for(short i = 0; i < 3; i++) {
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

	rmeshes_collection.rmeshes = calloc(model.meshCount, sizeof(MapMesh)); 
	BoundingBox model_bounds = GetModelBoundingBox(model);
	Vector3 model_center = BoxCenter(model_bounds);

	for(u16 i = 0; i < model.meshCount; i++) {
		//BoundingBox mesh_bounds = GetMeshBoundingBox(model.meshes[i]);

		rmeshes_collection.rmeshes[rmeshes_collection.count++] = (MapMesh) {
			.mesh_id = i,
			.material_id = model.meshMaterial[1],
			.matrix = model.transform,
			//.position = BoxCenter(mesh_bounds)
			.position = brush_pools[0].brushes[i].center
		};
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

// This function basically just constructs edges between nodes that already exist
void BuildNavGraph(MapSection *sect) {
	NavGraph *navgraph = &sect->base_navgraph;
	navgraph->edge_count = 0;

	BuildNavEdges(navgraph);
	SubdivideNavGraph(sect, navgraph);
}

#define MAX_EDGE_LENGTH (64.0f*64.0f)
void BuildNavEdges(NavGraph *navgraph) {
	navgraph->edge_count = 0;
	navgraph->edge_cap = 128;
	if(navgraph->edges) 
		navgraph->edges = realloc(navgraph->edges, sizeof(NavEdge) * navgraph->edge_cap);
	else {
		printf("edges null\n");
		navgraph->edges = calloc(navgraph->edge_cap, sizeof(NavEdge));
	}

	for(u16 i = 0; i < navgraph->node_count; i++) {
		NavNode *node_A = &navgraph->nodes[i];

		for(u16 j = 0; j < navgraph->node_count; j++) {
			NavNode *node_B = &navgraph->nodes[j];

			if(j == i)
				continue;

			// Using vector subtraction to get distance,
			// doing this in case I want to integrate actual level geometry later 
			Vector3 v = Vector3Subtract(node_A->position, node_B->position);
			float length = Vector3LengthSqr(v);	
		
			// Don't build edges if nodes are too far apart
			if(length > MAX_EDGE_LENGTH)
				continue;

			// All checks passed, create edge
			NavEdge edge = (NavEdge) { .id_A = i, .id_B = j };

			// Resize edge array if needed
			if(navgraph->edge_count >= navgraph->edge_cap) {
				navgraph->edge_cap = navgraph->edge_cap << 1;
				navgraph->edges = realloc(navgraph->edges, sizeof(NavEdge) * navgraph->edge_cap);	
			}

			// Skip creating edge if the reverse of it already exists 
			bool duplicate = false;
			for(u16 k = 0; k < navgraph->edge_count; k++) {
				NavEdge *edge = &navgraph->edges[k];
				//if((edge->node_A == i && edge->node_B == j) || (edge->node_B == i && edge->node_A == j)) {
				if((edge->id_A == node_A->id && edge->id_B == node_B->id) || (edge->id_B == node_A->id && edge->id_A == node_B->id)) {
					duplicate = true;
					break;
				}
			}
			if(duplicate)
				continue;

			// Copy to array	
			navgraph->edges[navgraph->edge_count++] = edge;
		}
	}

	for(u16 i = 0; i < navgraph->edge_count; i++) {
		NavEdge *edge = &navgraph->edges[i];

		NavNode *node_A = &navgraph->nodes[edge->id_A];
		NavNode *node_B = &navgraph->nodes[edge->id_B];

		node_A->edges[node_A->edge_count++] = i;
		node_B->edges[node_B->edge_count++] = i;
	}

	navgraph->edges = realloc(navgraph->edges, sizeof(NavEdge) * navgraph->edge_count);
	navgraph->nodes = realloc(navgraph->nodes, sizeof(NavNode) * navgraph->node_count);
}

void GetConnectedNodes(NavNode *node, u16 connected[MAX_EDGES_PER_NODE], u8 *count, NavGraph *navgraph) {
	for(u16 i = 0; i < node->edge_count; i++) {
		NavEdge *edge = &navgraph->edges[node->edges[i]];

		NavNode *node_A = &navgraph->nodes[edge->id_A];
		NavNode *node_B = &navgraph->nodes[edge->id_B];

		u16 next_node = (node_A->id == node->id) ? node_B->id : node_A->id;
		
		for(u8 i = 0; i < *count; i++) {
			if(connected[i] == next_node)
				return;
		}
		
		connected[(*count)++] = next_node; 
	}	
}

void WalkNavGraph(NavGraph *navgraph, u16 start_node, u16 *walked, u16 *count) {
	NavNode *node = &navgraph->nodes[start_node];	

	walked[(*count)++] = start_node;
	
	u16 next_nodes[MAX_EDGES_PER_NODE];
	u8 next_count = 0;

	GetConnectedNodes(node, next_nodes, &next_count, navgraph);
	
	for(u8 i = 0; i < next_count; i++) {
		bool duplicate = false;
		for(u16 j = 0; j < *count; j++) {
			if(walked[j] == next_nodes[i]) {
				duplicate = true;
			}
		}

		if(duplicate) 
			continue;

		WalkNavGraph(navgraph, next_nodes[i], walked, count);
	}
}

// Split navigation graphs so the spatial separation is reflected in data
// Only having one graph would break pathfinding, 
// graph/edge construction is distance based
void SubdivideNavGraph(MapSection *sect, NavGraph *navgraph) {
	u16 walked_total = 0;
	bool *traveled = calloc(navgraph->node_count, sizeof(bool));

	u16 splits[128];
	u16 split_count = 0;

	u16 next = 0;

	while(walked_total < navgraph->node_count) {
		while(next < navgraph->node_count && traveled[next])
			next++;

		if(next >= navgraph->node_count)
			break;

		u16 walk_count = 0;
		u16 temp[navgraph->node_count];
		WalkNavGraph(navgraph, next, temp, &walk_count);

		for(u16 i = 0; i < walk_count; i++) {
			u16 j = temp[i];
			if(!traveled[j]) {
				traveled[j] = true;
				walked_total++;
			}
		}

		splits[split_count++] = next;

		//for(u16 i = 0; i < walk_count; i++) printf("[%d]: -> %d\n", i, temp[i]);
	}

	/*
	printf("split count: %d\n", split_count);
	for(short i = 0; i < split_count; i++) {
		printf("split[%d] = %d \n", i, splits[i]);
	}
	*/

	for(short i = 0; i < split_count; i++) {
		u16 split_id = splits[i];

		u16 walk_count = 0;
		u16 node_ids[navgraph->node_count];

		WalkNavGraph(navgraph, split_id, node_ids, &walk_count);

		NavGraph graph = (NavGraph) {0};
		graph.node_count = walk_count;
		graph.nodes = calloc(graph.node_count, sizeof(NavNode));
		for(u16 j = 0; j < walk_count; j++) {
			graph.nodes[j] = navgraph->nodes[node_ids[j]];
			graph.nodes[j].id = j;
			graph.nodes[j].edge_count = 0;
		}
		BuildNavEdges(&graph);
		for(u16 j = 0; j < graph.edge_count; j++) {
			NavEdge *edge = &graph.edges[j];

			NavNode *a = &graph.nodes[edge->id_A];
			NavNode *b = &graph.nodes[edge->id_B];

			a->edges[a->edge_count++] = j;
			b->edges[b->edge_count++] = j;
		}

		sect->navgraphs[sect->navgraph_count++] = graph;

		/*
		NavGraph *graph = &sect->navgraphs[sect->navgraph_count++];
		graph->node_count = walk_count;
		graph->nodes = calloc(graph->node_count, sizeof(NavNode));
		for(u16 j = 0; j < walk_count; j++) {
			graph->nodes[j] = navgraph->nodes[node_ids[j]];
			graph->nodes[j].id = j;
		}

		graph->edge_cap = 128;
		graph->edges = calloc(graph->edge_cap, sizeof(NavEdge));
		BuildNavEdges(graph);
		*/
	}

	printf("graph count: %d\n", sect->navgraph_count);

	free(traveled);
}

bool IsNodeInGraph(NavGraph *graph, NavNode *node) {
	/*
	u16 walked[graph->node_count];
	u16 walk_count = 0;
	WalkNavGraph(graph, 0, walked, &walk_count);

	for(u16 i = walk_count; i > 0; i--) {
		if(walked[i] == node->id) {
			if(fabsf(Vector3LengthSqr(Vector3Subtract(node->position, graph->nodes[i].position))) < EPSILON*EPSILON)
				return true;
		}
	}
	*/
	
	if(node->id > graph->node_count)
		return false;

	for(u16 i = 0; i < graph->node_count; i++) {
		float diff = Vector3LengthSqr(Vector3Subtract(node->position, graph->nodes[i].position));	
		diff*=diff;

		if(diff <= EPSILON)
			return true;
	}

	return false;
}

int FindClosestNavNodeInGraph(Vector3 position, NavGraph *graph) {
	int id = -1;

	float closest_dist = FLT_MAX;

	for(u16 i = 0; i < graph->node_count; i++) {
		NavNode *node = &graph->nodes[i];

		float dist = Vector3DistanceSqr(node->position, position);	
		if(dist > closest_dist) 
			continue;

		if(!CheckCollisionSpheres(position, 32, node->position, 32))
			continue;

		closest_dist = dist;
		id = i;

		if(dist < 4*4)
			break;
	}

	return id;
}

void DebugDrawNavGraphs(MapSection *sect, Model model) {
	/*
	NavGraph *navgraph = &sect->base_navgraph;

	for(u16 e = 0; e < navgraph->edge_count; e++) {
		NavEdge *edge = &navgraph->edges[e];

		NavNode *node_A = &navgraph->nodes[edge->id_A];
		NavNode *node_B = &navgraph->nodes[edge->id_B];

		DrawModel(model, node_A->position, 1, BLUE);
		DrawModel(model, node_B->position, 1, BLUE);

		//Color line_color = (e % 2 == 0) ? MAGENTA : GREEN; 
		Color line_color = MAGENTA;
		DrawLine3D(node_A->position, node_B->position, line_color);
	}
	*/

	for(u16 i = 0; i < sect->navgraph_count; i++) {
		NavGraph *navgraph = &sect->navgraphs[i];

		for(u16 e = 0; e < navgraph->edge_count; e++) {
			NavEdge *edge = &navgraph->edges[e];

			NavNode *node_A = &navgraph->nodes[edge->id_A];
			NavNode *node_B = &navgraph->nodes[edge->id_B];

			DrawModel(model, node_A->position, 1, BLUE);
			DrawModel(model, node_B->position, 1, BLUE);

			Color line_color = (i % 2 == 0) ? MAGENTA : GREEN; 
			DrawLine3D(node_A->position, node_B->position, line_color);
		}
	}
}

void DebugDrawNavGraphsText(MapSection *sect, Camera3D cam, Vector2 window_size) {
	Vector3 cam_dir = Vector3Normalize(Vector3Subtract(cam.target, cam.position));

	for(u16 i = 0; i < sect->navgraph_count; i++) {
		NavGraph *navgraph = &sect->navgraphs[i];

		for(u16 n = 0; n < navgraph->node_count; n++) {
			NavNode *node = &navgraph->nodes[n];

			Vector3 to_cam = Vector3Normalize(Vector3Subtract(cam.position, node->position));
			if(Vector3DotProduct(to_cam, cam_dir) > 0) continue;

			float dist = Vector3Distance(node->position, cam.position);
			float text_size = (30);

			Vector2 pos = GetWorldToScreen(node->position, cam);

			for(u16 e = 0; e < navgraph->edge_count; e++) {
				NavEdge *edge = &navgraph->edges[e];

				NavNode *node_A = &navgraph->nodes[edge->id_A];
				NavNode *node_B = &navgraph->nodes[edge->id_B];

				Vector3 mid = Vector3Scale(Vector3Add(node_A->position, node_B->position), 0.5f);

				Vector3 to_cam = Vector3Normalize(Vector3Subtract(cam.position, mid));
				if(Vector3DotProduct(to_cam, cam_dir) > 0) continue;

				Vector2 pos = GetWorldToScreen(mid, cam);

				float text_size = (30);

				DrawText(TextFormat("%d -> %d", edge->id_A, edge->id_B), pos.x, pos.y, text_size, GRAY);
				DrawText(TextFormat("%d", e), pos.x, pos.y - 32, text_size, LIGHTGRAY);
			}

			DrawText(TextFormat("%d", node->id), pos.x, pos.y, text_size, YELLOW);
			//DrawText(TextFormat("ec: %d", node->edge_count), pos.x, pos.y - 32, text_size, SKYBLUE);
			/*
			for(u8 k = 0; k < navgraph->nodes[n].edge_count; k++) {
				DrawText(TextFormat("%d", navgraph->nodes[n].edges[k]), pos.x, pos.y - (32 * (k+1)), text_size, SKYBLUE);
			}
			*/
		}
	}
}

#define MAX_MAP_MESH_RENDERS 128
typedef struct {
	u16 ids[MAX_MAP_MESH_RENDERS];
	u16 count;
	
} rMeshList;
rMeshList rmesh_list = {0};

void UpdateMapMeshList(MapSection *sect, Camera3D cam) {
	/*
	Vector3 view_pos = cam.position;
	Vector3 view_dir = Vector3Normalize(Vector3Subtract(cam.target, view_pos));

	rmesh_list.count = 0;
	for(u16 i = 0; i < sect->model.meshCount; i++) {
		Vector3 to_view = Vector3Subtract(view_pos, rmeshes_collection.rmeshes[i].position);

		rmesh_list.ids[rmesh_list.count++] = i;
	}
	*/
}

void DrawMap(MapSection *sect) {
	/*
	for(u16 i = 0; i < rmesh_list.count; i++) {
		DrawMesh(sect->model.meshes[rmesh_list.ids[i]], sect->model.materials[1], sect->model.transform);	
	}
	*/
	DrawModel(sect->model, Vector3Zero(), 1, WHITE);
}

