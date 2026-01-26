#include "raylib.h"
#include "../include/num_redefs.h"

#ifndef GEO_H_
#define GEO_H_

// Triangle primitive struct
typedef struct {
	Vector3 vertices[3];
	Vector3 normal;

} Tri;

// Compute a triangle's normal vector
Vector3 TriNormal(Tri tri);

// Compute a triangle's centroid vector
Vector3 TriCentroid(Tri tri);

// Calculate length of a box in each axis
Vector3 BoxExtent(BoundingBox box);

// Compute surface area of box
float BoxSurfaceArea(BoundingBox box);

// Find center point of box
Vector3 BoxCenter(BoundingBox box);

// Return a box with min as [float max] and max as [float min]
BoundingBox EmptyBox();

// Grow a box to fit a point in space
BoundingBox BoxExpandToPoint(BoundingBox box, Vector3 point);

// Create a primitive array from mesh
Tri *MeshToTris(Mesh mesh, u16 *tri_count);

// Create a primitive array from model (with indexing)
Tri *ModelToTris(Model model, u16 *tri_count, u16 **tri_ids);

#define MAX_TRIS_PER_NODE	4

// BVH node struct
// Size of 32 bytes reduces cache misses
typedef struct {
	// Minimum and maximum points
	BoundingBox bounds;

	// Number of primitives 
	u16 tri_count;

	// Index of first primitive contained in node
	u16 first_tri;

	// Child node indices
	u16 child_lft, child_rgt;

} BvhNode;

#define BVH_BIN_COUNT	16

typedef struct {
	BoundingBox bounds;
	u16 count;

} Bin;

#define BVH_TREE_START_CAPACITY	1024

typedef struct {
	// Node array
	BvhNode *nodes;

	u16 count;
	u16 capacity;

} BvhTree;

#define MAP_SECT_LOADED	0x01
#define MAP_SECT_QUEUED	0x02

typedef struct {
	BvhTree bvh;

	Model model;

	Tri *tris;
	u16 *tri_ids;
	u16 tri_count;

	u8 flags;

} MapSection;

// Calculate cost of a node
float BvhNodeCost(BvhNode *node);

// Grow bounding box of a node using it's contained primitives
void BvhNodeUpdateBounds(MapSection *sect, BvhTree *bvh, u16 node_id);

// Start BVH tree construction
void BvhConstruct(MapSection *sect);

// Unload BVH tree
void BvhClose(BvhTree *bvh);

// Compute optimal axis and position for node subdivision  
float FindBestSplit(MapSection *sect, BvhNode *node, short *axis, float *split_pos);

// Recursively split BVH nodes 
void BvhNodeSubdivide(MapSection *sect, BvhTree *bvh, u16 node_id);

// Initialize map section,
// Load geometry, materials, construct BVH, etc.  
void MapSectionInit(MapSection *sect, Model model);

// Unload map section data
void MapSectionClose(MapSection *sect);

// Trace a point through world space
void BvhTracePoint(Ray ray, MapSection *sect, u16 node_id, float smallest_dist, Vector3 *point);

#endif
