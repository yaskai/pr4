#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "../include/num_redefs.h"
#include "geo.h"

// Swap triangle indices
void SwapTriIds(u16 *a, u16 *b) {
	u16 temp = *a;
	*a = *b;
	*b = temp;
} 

// Compute a triangle's normal vector
Vector3 TriNormal(Tri tri) {
	Vector3 u = Vector3Subtract(tri.vertices[1], tri.vertices[0]);
	Vector3 v = Vector3Subtract(tri.vertices[2], tri.vertices[0]);
	return Vector3Normalize(Vector3CrossProduct(u, v));
};

// Compute a triangle's centroid vector
Vector3 TriCentroid(Tri tri) {
	return (Vector3) { 
		.x = (tri.vertices[0].x + tri.vertices[1].x + tri.vertices[2].x) * 0.33f,
		.y = (tri.vertices[0].y + tri.vertices[1].y + tri.vertices[2].y) * 0.33f,
		.z = (tri.vertices[0].z + tri.vertices[1].z + tri.vertices[2].z) * 0.33f
	};
}

// Return a triangle translated to a new point
Tri TriTranslate(Tri tri, Vector3 movement) {
	for(short i = 0; i < 3; i++) tri.vertices[i] = Vector3Add(tri.vertices[i], movement);
	return tri;
}

// Get plane from tri
Plane TriToPlane(Tri tri) {
	Vector3 n = Vector3CrossProduct(Vector3Subtract(tri.vertices[1], tri.vertices[0]), Vector3Subtract(tri.vertices[2], tri.vertices[0]));
	n = Vector3Normalize(n);

	float d = -Vector3DotProduct(tri.vertices[0], n);
	
	return (Plane) { .normal = n, .d = d };
}

// Calculate signed distance from point to a plane
float PlaneDistance(Plane plane, Vector3 point) {
	return Vector3DotProduct(plane.normal, point) - plane.d;
}

// Calculate length of a box in each axis
Vector3 BoxExtent(BoundingBox box) {
	return Vector3Subtract(box.max, box.min);
}

// Compute surface area of box
float BoxSurfaceArea(BoundingBox box) {
	Vector3 extent = BoxExtent(box); 
	return (extent.x * extent.y + extent.y * extent.z + extent.z * extent.x);
}

// Find center point of box
Vector3 BoxCenter(BoundingBox box) {
	Vector3 extent = BoxExtent(box);
	return Vector3Subtract(box.max, Vector3Scale(extent, 0.5f));
}

// Return a box with min as [float max] and max as [float min]
BoundingBox EmptyBox() {
	return (BoundingBox) { .min = Vector3Scale(Vector3One(), FLT_MAX), .max = Vector3Scale(Vector3One(), -FLT_MAX) };
}

// Grow a box to fit a point in space
BoundingBox BoxExpandToPoint(BoundingBox box, Vector3 point) {
	return (BoundingBox) { .min = Vector3Min(box.min, point), .max = Vector3Max(box.max, point) };
}

// Translate a box to a new position
BoundingBox BoxTranslate(BoundingBox box, Vector3 point) {
	Vector3 extent = BoxExtent(box);	
	return (BoundingBox) { .min = Vector3Add(point, Vector3Scale(extent, -0.5f)), .max = Vector3Add(point, Vector3Scale(extent, 0.5f)) };
}

// Get a box with it's points clamped to the back side of a plane
BoundingBox BoxFitToSurface(BoundingBox box, Vector3 point, Vector3 normal) {
	//box = BoxTranslate(box, point);
	normal = Vector3Normalize(normal);

	Vector3 center = BoxCenter(box);

	float max_depth = 0.0f;
	BoxPoints points = BoxGetPoints(box);

	for(u8 i = 0; i < 8; i++) {
		Vector3 to_point = Vector3Subtract(points.v[i], point);	
		float dot = Vector3DotProduct(to_point, normal); 

		if(dot > max_depth) {
			max_depth = dot;
		}
	}

	if(max_depth > 0.0f) {
		Vector3 offset = Vector3Scale(normal, -max_depth);
		box = BoxTranslate(box, Vector3Subtract(center, offset));
	}

	return box;
}

float BoxGetSurfaceDepth(BoundingBox box, Vector3 point, Vector3 normal) {
	normal = Vector3Normalize(normal);

	Vector3 center = BoxCenter(box);

	float max_depth = 0.0f;
	BoxPoints points = BoxGetPoints(box);

	for(u8 i = 0; i < 8; i++) {
		Vector3 to_point = Vector3Subtract(points.v[i], point);	
		float dot = Vector3DotProduct(to_point, normal); 

		if(dot > max_depth) 
			max_depth = dot;
	}

	return max_depth;
}

Vector3 BoxSurfaceDelta(BoundingBox box, Vector3 point, Vector3 normal) {
	normal = Vector3Normalize(normal);

	Vector3 center = BoxCenter(box);
	Vector3 offset = Vector3Zero();

	BoxPoints points = BoxGetPoints(box);
	float max_depth = 0.0f;

	for(u8 i = 0; i < 8; i++) {
		Vector3 to_point = Vector3Subtract(points.v[i], point);	
		float dot = Vector3DotProduct(to_point, normal); 

		if(dot > max_depth) 
			max_depth = dot;
	}

	if(max_depth > 0.0f)
		offset = Vector3Scale(normal, -max_depth);

	return offset;
}

BoxNormals BoxGetFaceNormals(BoundingBox box) {
	BoxNormals normals = {0};

	for(u8 i = 0; i < 3; i++) {
		float x = (1 << i & 0x01) ? 1 : 0;
		float y = (1 << i & 0x02) ? 1 : 0;
		float z = (1 << i & 0x04) ? 1 : 0;

		normals.n[i] = (Vector3) { x, y, z }; 
		normals.n[3 + i] = Vector3Scale(normals.n[i], -1);
	}

	return normals;
}

BoxPoints BoxGetPoints(BoundingBox box) {
	BoxPoints box_points = (BoxPoints) {0};

	for(u8 i = 0; i < 8; i++) {
		box_points.v[i].x = (i & 0x01) ? box.min.x : box.max.x;
		box_points.v[i].y = (i & 0x02) ? box.min.y : box.max.y;
		box_points.v[i].z = (i & 0x04) ? box.min.z : box.max.z;
	}

	return box_points;
}

// Create a primitive array from mesh
Tri *MeshToTris(Mesh mesh, u16 *tri_count) {
	// Get triangle count
	u16 count = mesh.triangleCount;
	*tri_count = count;
	
	// Create and populate array
	Tri *tris = calloc(count, sizeof(Tri));
	for(u16 i = 0; i < count; i++) {
		Tri tri = (Tri) {0};

		// Get mesh indices
		u16 indices[3] = {0};
		memcpy(indices, mesh.indices + i * 3, sizeof(u16) * 3);

		// Get XYZ values for each vertex
		for(short j = 0; j < 3; j++) {
			u16 id = indices[j];

			tri.vertices[j] = (Vector3) { 
				.x = mesh.vertices[id * 3 + 0],
				.y = mesh.vertices[id * 3 + 1],
				.z = mesh.vertices[id * 3 + 2] 
			};
		}

		// Calculate normal vector
		tri.normal = TriNormal(tri);

		// Copy triangle to array
		tris[i] = tri;
	}

	// Return array
	return tris;
}

// Create a primitive array from model (with indexing)
Tri *ModelToTris(Model model, u16 *tri_count, u16 **tri_ids) {
	u16 count = 0;

	// Intialize arrays
	Tri *tris = NULL;
	u16 *ids = NULL;

	// Iterate through meshes
	for(u16 i = 0; i < model.meshCount; i++) {
		// Create temporary array for mesh triangles
		u16 temp_count = 0;
		Tri *temp_tris = MeshToTris(model.meshes[i], &temp_count);

		// Increment count by mesh's tri count
		count += temp_count; 

		// Resize arrays
		tris = realloc(tris, sizeof(Tri) * count);
		ids = realloc(ids, sizeof(u16) * count);

		// Set ids
		for(u16 j = 0; j < temp_count; j++) {
			u16 id = count - temp_count + j; 
			ids[id] = id;
		}

		// Copy temp tris to primary array
		memcpy(tris + count - temp_count, temp_tris, sizeof(Tri) * temp_count);

		// Free temporary array
		free(temp_tris);
	}

	// Set id and count values 
	*tri_ids = ids;
	*tri_count = count;

	// Return triangle array
	return tris;
}

// Calculate cost of a node
float BvhNodeCost(BvhNode *node) {
	return BoxSurfaceArea(node->bounds) * node->tri_count;
}

// Grow bounding box of a node using it's contained primitives
void BvhNodeUpdateBounds(MapSection *sect, BvhTree *bvh, u16 node_id) {
	BvhNode *node = &bvh->nodes[node_id];

	node->bounds = EmptyBox();
	
	for(u16 i = 0; i < node->tri_count; i++) {
		u16 tri_id = bvh->tri_ids[node->first_tri + i];
		Tri tri = sect->tris[tri_id];

		for(short j = 0; j < 3; j++) {
			node->bounds.min = (Vector3) {
				fminf(node->bounds.min.x, tri.vertices[j].x),
				fminf(node->bounds.min.y, tri.vertices[j].y),
				fminf(node->bounds.min.z, tri.vertices[j].z)
			};

			node->bounds.max = (Vector3) {
				fmaxf(node->bounds.max.x, tri.vertices[j].x),
				fmaxf(node->bounds.max.y, tri.vertices[j].y),
				fmaxf(node->bounds.max.z, tri.vertices[j].z)
			};
		}
	}

	Vector3 h = Vector3Scale(bvh->shape, 0.5f);
	node->bounds.min = Vector3Subtract(node->bounds.min, h);
	node->bounds.max = Vector3Add(node->bounds.max, h);
}

// Start BVH tree construction
void BvhConstruct(MapSection *sect, BvhTree *bvh, Vector3 volume) {
	// Reset count
	bvh->count = 0;

	// Allocate memory for nodes
	bvh->capacity = BVH_TREE_START_CAPACITY;
	bvh->nodes = calloc(bvh->capacity, sizeof(BvhNode));

	bvh->shape = volume;
	Vector3 shape_half = Vector3Scale(bvh->shape, 0.5f);

	bvh->tri_ids = malloc(sect->tri_count * sizeof(u16));
	memcpy(bvh->tri_ids, sect->tri_ids, sizeof(u16) * sect->tri_count);

	// Initalize empty node to use as root
	BvhNode root = (BvhNode) {0};
	root.bounds = EmptyBox();

	// Grow bounds of root to contain all section geoemetry  
	for(u16 i = 0; i < sect->tri_count; i++) {
		//Tri tri = sect->tris[sect->tri_ids[i]];
		Tri tri = sect->tris[bvh->tri_ids[i]];

		float diff = MinkowskiDiff(tri.normal, shape_half);

		for(short j = 0; j < 3; j++) {
			//root.bounds = BoxExpandToPoint(root.bounds, tri.vertices[j]);
			root.bounds = BoxExpandToPoint(root.bounds, Vector3Add(tri.vertices[j], Vector3Scale(tri.normal, diff)));
		}
	}

	root.bounds.min = Vector3Subtract(root.bounds.min, shape_half);
	root.bounds.max = Vector3Add(root.bounds.max, shape_half);

	// Assign root to array, increment node count 
	root.tri_count = sect->tri_count;
	bvh->nodes[bvh->count++] = root;

	// Start recursive node splitting
	BvhNodeSubdivide(sect, bvh, 0);

	// When done splitting, trim array to save some memory
	bvh->capacity = bvh->count;
	bvh->nodes = realloc(bvh->nodes, sizeof(BvhNode) * bvh->capacity);
}

// Unload BVH tree
void BvhClose(BvhTree *bvh) {
	if(bvh->nodes) free(bvh->nodes);
	bvh->count = 0;
}

// Compute optimal axis and position for node subdivision  
float FindBestSplit(MapSection *sect, BvhTree *bvh, BvhNode *node, short *axis, float *split_pos) {
	float best_cost = FLT_MAX;	

	for(short a = 0; a < 3; a++) {
		float vmin = FLT_MAX, vmax = -FLT_MAX;

		for(u16 i = 0; i < node->tri_count; i++) {
			//Tri tri = sect->tris[sect->tri_ids[node->first_tri + i]];
			Tri tri = sect->tris[bvh->tri_ids[node->first_tri + i]];
			float3 centroid = Vector3ToFloatV(TriCentroid(tri));

			vmin = fminf(vmin, centroid.v[a]);
			vmax = fmaxf(vmax, centroid.v[a]);
		}

		if(vmin == vmax) continue;

		Bin bins[BVH_BIN_COUNT] = {0};
		for(short i = 0; i < BVH_BIN_COUNT; i++) bins[i].bounds = EmptyBox();

		float scale = BVH_BIN_COUNT / (vmax - vmin);

		for(u16 i = 0; i < node->tri_count; i++) {
			//Tri tri = sect->tris[sect->tri_ids[node->first_tri + i]];
			Tri tri = sect->tris[bvh->tri_ids[node->first_tri + i]];
			float3 centroid = Vector3ToFloatV(TriCentroid(tri));

			int bin_id = fmin(BVH_BIN_COUNT - 1, (int)((centroid.v[a] - vmin) * scale));

			bins[bin_id].count++;	

			for(short j = 0; j < 3; j++)
				bins[bin_id].bounds = BoxExpandToPoint(bins[bin_id].bounds, tri.vertices[j]);
		}

		float area_lft[BVH_BIN_COUNT - 1], area_rgt[BVH_BIN_COUNT - 1];
		u16 count_lft[BVH_BIN_COUNT - 1], count_rgt[BVH_BIN_COUNT - 1];

		BoundingBox bounds_lft = EmptyBox(), bounds_rgt = EmptyBox();
		u32 sum_lft = 0, sum_rgt = 0;

		for(short i = 0; i < BVH_BIN_COUNT - 1; i++) {
			short id_lft = i;
			sum_lft += bins[id_lft].count;
			count_lft[i] = sum_lft; 
			bounds_lft.min = Vector3Min(bins[id_lft].bounds.min, bounds_lft.min);
			bounds_lft.max = Vector3Max(bins[id_lft].bounds.max, bounds_lft.max);

			short id_rgt = BVH_BIN_COUNT - 2 - i;
			sum_rgt += bins[BVH_BIN_COUNT - 1 - i].count;
			count_rgt[id_rgt] = sum_rgt;
			bounds_rgt.min = Vector3Min(bins[BVH_BIN_COUNT - 1 - i].bounds.min, bounds_rgt.min);
			bounds_rgt.max = Vector3Max(bins[BVH_BIN_COUNT - 1 - i].bounds.max, bounds_rgt.max);
			area_rgt[id_rgt] = BoxSurfaceArea(bounds_rgt);
		}

		scale = (vmax - vmin) / BVH_BIN_COUNT;

		for(short i = 0; i < BVH_BIN_COUNT - 1; i++) {
			float cost = (count_lft[i] * area_lft[i] + count_rgt[i] * area_rgt[i]) / BoxSurfaceArea(node->bounds);

			if(cost < best_cost) {
				*axis = a;
				*split_pos = vmin + scale * (i + 1); 
				best_cost = cost;
			}
		}
	}

	return best_cost;
}

// Recursively split BVH nodes 
void BvhNodeSubdivide(MapSection *sect, BvhTree *bvh, u16 node_id) {
	BvhNode *node = &bvh->nodes[node_id];

	// Base case:
	// Node has too few tris to continue splitting
	//if(node->tri_count <= MAX_TRIS_PER_NODE) return;

	// Determine split position and axis
	short split_axis = -1;
	float split_pos = 0;
	float best_cost = FindBestSplit(sect, bvh, node, &split_axis, &split_pos);

	// Base case:
	// Cost of splitting exceeds cost of leaving as is
	float parent_cost = BvhNodeCost(node);
	if(best_cost > parent_cost) return;

	Vector3 h = Vector3Scale(bvh->shape, 0.5f);
	node->bounds.min = Vector3Subtract(node->bounds.min, h);
	node->bounds.max = Vector3Add(node->bounds.max, h);

	// In-place partition
	u16 i = node->first_tri;
	u16 j = i + node->tri_count - 1;
	while(i <= j) {
		Tri tri = sect->tris[bvh->tri_ids[i]];

		float3 centroid = Vector3ToFloatV(TriCentroid(tri));

		if(centroid.v[split_axis] < split_pos) i++;
		else SwapTriIds(&bvh->tri_ids[i], &bvh->tri_ids[j--]);
	}

	u16 count_lft = i - node->first_tri;

	// Base case: 
	// Cancel subdivision if either side is empty
	if(count_lft == 0 || count_lft == node->tri_count) return;

	// Resize node array if needed
	if(bvh->count + 2 >= bvh->capacity) {
		bvh->capacity = bvh->capacity << 1;
		bvh->nodes = realloc(bvh->nodes, sizeof(BvhNode) * bvh->capacity);
	}

	// Create child nodes	
	u16 child_lft = bvh->count++;
	u16 child_rgt = bvh->count++;

	bvh->nodes[child_lft] = (BvhNode) {
		.first_tri = node->first_tri,	
		.tri_count = count_lft,
		.child_lft = 0, 
		.child_rgt = 0
	};
	node->child_lft = child_lft;

	bvh->nodes[child_rgt] = (BvhNode) {
		.first_tri = i,
		.tri_count = node->tri_count - count_lft,
		.child_lft = 0,
		.child_rgt = 0
	};
	node->child_rgt = child_rgt;

	// Remove tris from parent node
	node->tri_count = 0;

	// Update child node bounds
	BvhNodeUpdateBounds(sect, bvh, child_lft);
	BvhNodeUpdateBounds(sect, bvh, child_rgt);
	
	// Continue splitting with left and right child nodes 
	BvhNodeSubdivide(sect, bvh, child_lft);
	BvhNodeSubdivide(sect, bvh, child_rgt);
}
// Initialize map section,
// Load geometry, materials, construct BVH, etc.  
void MapSectionInit(MapSection *sect, Model model) {
	sect->model = model;
	sect->tris = ModelToTris(model, &sect->tri_count, &sect->tri_ids);

	BvhConstruct(sect, &sect->bvh[0], Vector3Zero());
	BvhConstruct(sect, &sect->bvh[1], BODY_VOLUME_MEDIUM);

	sect->flags = (MAP_SECT_LOADED);
}

// Unload map section data
void MapSectionClose(MapSection *sect) {
	if(sect->tris) 
		free(sect->tris);	

	if(sect->tri_ids)
		free(sect->tri_ids);

	BvhClose(&sect->bvh[0]);	
	BvhClose(&sect->bvh[1]);	
	UnloadModel(sect->model);
}

// Return empty TraceData struct
BvhTraceData TraceDataEmpty() {
	return (BvhTraceData) { 
		.point = Vector3Zero(),
		.normal = Vector3Zero(),
		.distance = FLT_MAX,
		.node_id = 0,
		.tri_id = 0,
		.hit = false,
		.contact_dist = FLT_MAX,
		.contact = Vector3Zero()
	};
}

void BvhTraceNodes(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, float smallest_dist, BvhNode *node_hit) {
	BvhNode *node = &bvh->nodes[node_id];
	
	RayCollision coll = GetRayCollisionBox(ray, node->bounds);	
	if(!coll.hit) return;
	
	if(coll.distance <= smallest_dist) {
		smallest_dist = coll.distance;
		node_hit = node;
	}

	bool leaf = (node->tri_count > 0 && node->child_rgt + node->child_lft == 0);
	if(leaf) return;

	BvhTraceNodes(ray, sect, bvh, node->child_lft, smallest_dist, node_hit);
	BvhTraceNodes(ray, sect, bvh, node->child_rgt, smallest_dist, node_hit);
}

// Trace a point through world space
void BvhTracePoint(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, float *smallest_dist, Vector3 *point, bool skip_root) {
	BvhNode *node = &bvh->nodes[node_id];

	RayCollision coll;

	if(!skip_root) {
		coll = GetRayCollisionBox(ray, node->bounds);

		if(!coll.hit)
			return;

		if(coll.distance > *smallest_dist)
			return;
	};

	for(u16 i = 0; i < node->tri_count; i++) {
		//u16 tri_id = sect->tri_ids[node->first_tri + i];
		u16 tri_id = bvh->tri_ids[node->first_tri + i];
		Tri *tri = &sect->tris[tri_id];

		coll = GetRayCollisionTriangle(ray, tri->vertices[0], tri->vertices[1], tri->vertices[2]);
		if(!coll.hit) continue;

		if(coll.distance < *smallest_dist) {
			*smallest_dist = coll.distance;
			*point = coll.point;
		}
	}

	bool leaf = (node->tri_count > 0);
	if(leaf) return;

	RayCollision hit_l = GetRayCollisionBox(ray, bvh->nodes[node->child_lft].bounds);	
	RayCollision hit_r = GetRayCollisionBox(ray, bvh->nodes[node->child_rgt].bounds);

	if(!(hit_l.hit + hit_r.hit)) return;

	float dl = (hit_l.hit) ? hit_l.distance : FLT_MAX;
	float dr = (hit_r.hit) ? hit_r.distance : FLT_MAX;

	if(dl < dr) {
		BvhTracePoint(ray, sect, bvh, node->child_lft, smallest_dist, point, true);
		BvhTracePoint(ray, sect, bvh, node->child_rgt, smallest_dist, point, true);
		return;
	}

	BvhTracePoint(ray, sect, bvh, node->child_rgt, smallest_dist, point, true);
	BvhTracePoint(ray, sect, bvh, node->child_lft, smallest_dist, point, true);
}

void BvhTracePointEx(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BvhTraceData *data) {
	BvhNode *node = &bvh->nodes[node_id];

	RayCollision coll;
	Vector3 h = Vector3Scale(bvh->shape, 0.5f);

	if(node_id == 0) {
		coll = GetRayCollisionBox(ray, node->bounds);

		if(!coll.hit)
			return;

		if(coll.distance > data->distance)
			return;
	};

	for(u16 i = 0; i < node->tri_count; i++) {
		u16 tri_id = bvh->tri_ids[node->first_tri + i];
		Tri tri = sect->tris[tri_id];

		float diff = MinkowskiDiff(tri.normal, h);

		/*
		Vector3 centroid = TriCentroid(tri);
		for(short j = 0; j < 3; j++) {
			Vector3 to_centroid = Vector3Normalize(Vector3Subtract(centroid, tri.vertices[j]));
			tri.vertices[j] = Vector3Subtract(tri.vertices[j], Vector3Scale(to_centroid, diff));
		}
		*/

		coll = GetRayCollisionTriangle(ray, tri.vertices[0], tri.vertices[1], tri.vertices[2]);
		if(!coll.hit) continue;

		coll.point = Vector3Add(ray.position, Vector3Scale(ray.direction, coll.distance - diff));
		coll.distance -= diff;
		
		if(coll.distance > data->distance) return;

		data->point = coll.point;
		data->normal = tri.normal;
		data->distance = coll.distance;
		data->tri_id = tri_id;
		data->node_id = node_id;
		data->hit = true;
	}

	bool leaf = (node->tri_count > 0);
	if(leaf) return;

	RayCollision hit_l = GetRayCollisionBox(ray, bvh->nodes[node->child_lft].bounds);	
	RayCollision hit_r = GetRayCollisionBox(ray, bvh->nodes[node->child_rgt].bounds);

	if(!(hit_l.hit + hit_r.hit)) return;

	float dl = (hit_l.hit) ? hit_l.distance : FLT_MAX;
	float dr = (hit_r.hit) ? hit_r.distance : FLT_MAX;

	if(dl < dr) {
		BvhTracePointEx(ray, sect, bvh, node->child_lft, data);
		BvhTracePointEx(ray, sect, bvh, node->child_rgt, data);
		return;
	}

	BvhTracePointEx(ray, sect, bvh, node->child_rgt, data);
	BvhTracePointEx(ray, sect, bvh, node->child_lft, data);
}

/*
void BvhBoxSweep(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BoundingBox *box, BvhTraceData *data) {
	BvhNode *node = &bvh->nodes[node_id];

	RayCollision coll;

	if(node_id == 0) {
		coll = GetRayCollisionBox(ray, node->bounds);

		if(!coll.hit)
			return;

		if(coll.distance > data->distance)
			return;
	};

	for(u16 i = 0; i < node->tri_count; i++) {
		u16 tri_id = sect->tri_ids[node->first_tri + i];
		Tri tri = sect->tris[tri_id];

		coll = GetRayCollisionTriangle(ray, tri.vertices[0], tri.vertices[1], tri.vertices[2]);
		if(!coll.hit) continue;

		Vector3 h = Vector3Scale(BoxExtent(*box), 0.5f);
		float diff = ( fabsf(tri.normal.x) * h.x +  fabsf(tri.normal.y) * h.y + fabsf(tri.normal.z) * h.z ); 
		
		coll.distance -= diff;
		coll.point = Vector3Add(ray.position, Vector3Scale(ray.direction, coll.distance));

		data->point = coll.point;
		data->normal = coll.normal;
		data->distance = coll.distance;
		data->tri_id = tri_id;
		data->node_id = node_id;
		data->hit = true;
	}

	bool leaf = (node->tri_count > 0);
	if(leaf) return;

	RayCollision hit_l = GetRayCollisionBox(ray, bvh->nodes[node->child_lft].bounds);	
	RayCollision hit_r = GetRayCollisionBox(ray, bvh->nodes[node->child_rgt].bounds);

	if(!(hit_l.hit + hit_r.hit)) return;

	float dl = (hit_l.hit) ? hit_l.distance : FLT_MAX;
	float dr = (hit_r.hit) ? hit_r.distance : FLT_MAX;

	if(dl < dr) {
		BvhBoxSweep(ray, sect, bvh, node->child_lft, box, data);
		BvhBoxSweep(ray, sect, bvh, node->child_rgt, box, data);
		return;
	}

	BvhBoxSweep(ray, sect, bvh, node->child_rgt, box, data);
	BvhBoxSweep(ray, sect, bvh, node->child_lft, box, data);
}
*/

void BvhBoxSweep(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BoundingBox *box, BvhTraceData *data) {
	BvhNode *node = &bvh->nodes[node_id];

	RayCollision coll;

	if(node_id == 0) {
		coll = GetRayCollisionBox(ray, node->bounds);

		if(!coll.hit)
			return;

		if(coll.distance > data->distance)
			return;
	};

	for(u16 i = 0; i < node->tri_count; i++) {
		//u16 tri_id = sect->tri_ids[node->first_tri + i];
		u16 tri_id = bvh->tri_ids[node->first_tri + i];
		Tri tri = sect->tris[tri_id];

	}

	bool leaf = (node->tri_count > 0);
	if(leaf) return;

	RayCollision hit_l = GetRayCollisionBox(ray, bvh->nodes[node->child_lft].bounds);	
	RayCollision hit_r = GetRayCollisionBox(ray, bvh->nodes[node->child_rgt].bounds);

	if(!(hit_l.hit + hit_r.hit)) return;

	float dl = (hit_l.hit) ? hit_l.distance : FLT_MAX;
	float dr = (hit_r.hit) ? hit_r.distance : FLT_MAX;

	if(dl < dr) {
		BvhBoxSweep(ray, sect, bvh, node->child_lft, box, data);
		BvhBoxSweep(ray, sect, bvh, node->child_rgt, box, data);
		return;
	}

	BvhBoxSweep(ray, sect, bvh, node->child_rgt, box, data);
	BvhBoxSweep(ray, sect, bvh, node->child_lft, box, data);
}
void BvhBoxSweepNoInvert(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BoundingBox *box, BvhTraceData *data) {
	BvhNode *node = &bvh->nodes[node_id];

	RayCollision coll;

	if(node_id == 0) {
		coll = GetRayCollisionBox(ray, node->bounds);

		if(!coll.hit)
			return;

		if(coll.distance > data->distance)
			return;
	};

	for(u16 i = 0; i < node->tri_count; i++) {
		u16 tri_id = bvh->tri_ids[node->first_tri + i];
		Tri tri = sect->tris[tri_id];

		coll = GetRayCollisionTriangle(ray, tri.vertices[0], tri.vertices[1], tri.vertices[2]);
		if(!coll.hit) continue;

		Vector3 h = Vector3Scale(BoxExtent(*box), 0.5f);
		float diff = ( fabsf(tri.normal.x) * h.x +  fabsf(tri.normal.y) * h.y + fabsf(tri.normal.z) * h.z ); 
		
		coll.distance -= diff;
		coll.point = Vector3Subtract(coll.point, Vector3Scale(ray.direction, diff));

		if(coll.distance < data->distance) {
			data->point = coll.point;
			data->normal = tri.normal;
			data->distance = coll.distance;
			data->tri_id = tri_id;
			data->node_id = node_id;
			data->hit = true;
		}
	}

	bool leaf = (node->tri_count > 0);
	if(leaf) return;

	RayCollision hit_l = GetRayCollisionBox(ray, bvh->nodes[node->child_lft].bounds);	
	RayCollision hit_r = GetRayCollisionBox(ray, bvh->nodes[node->child_rgt].bounds);

	if(!(hit_l.hit + hit_r.hit)) return;

	float dl = (hit_l.hit) ? hit_l.distance : FLT_MAX;
	float dr = (hit_r.hit) ? hit_r.distance : FLT_MAX;

	if(dl < dr) {
		BvhBoxSweepNoInvert(ray, sect, bvh, node->child_lft, box, data);
		BvhBoxSweepNoInvert(ray, sect, bvh, node->child_rgt, box, data);
		return;
	}

	BvhBoxSweepNoInvert(ray, sect, bvh, node->child_rgt, box, data);
	BvhBoxSweepNoInvert(ray, sect, bvh, node->child_lft, box, data);
}

void MapSectionDisplayNormals(MapSection *sect) {
	for(u16 i = 0; i < sect->tri_count; i++) {
		Tri tri = sect->tris[i];
		Vector3 centroid = TriCentroid(tri);

		DrawLine3D(centroid, Vector3Add(centroid, Vector3Scale(tri.normal, 4)), SKYBLUE);
	}
}

float BoundsToRadius(BoundingBox bounds) {
	Vector3 h = Vector3Scale(BoxExtent(bounds), 0.5f);
	return sqrtf(h.x*h.x + h.z*h.z);
}

// Calculate Minkowski Difference from normal of shape A and half extents of shape B
float MinkowskiDiff(Vector3 normal, Vector3 h) {
	return fabsf(normal.x) * h.x +  fabsf(normal.y) * h.y + fabsf(normal.z) * h.z; 
}

IntersectData IntersectDataEmpty() {
	return (IntersectData) {0};
}

void BvhBoxIntersect(BoundingBox box, MapSection *sect, BvhTree *bvh, u16 node_id, IntersectData *data) {
	BvhNode *node = &bvh->nodes[node_id];

	if(!CheckCollisionBoxes(box, node->bounds))
		return;
	
	bool leaf = node->tri_count > 0;
	if(leaf) {
		for(u16 i = 0; i < node->tri_count; i++) {
		}

		return;
	}
	
	//BvhBoxIntersect(box, MapSection *sect, BvhTree *bvh, u16 node_id, IntersectData *data)	
}

