#include "raylib.h"
#include "../include/num_redefs.h"
#include "ai.h"

#ifndef GEO_H_
#define GEO_H_

#define UP 		(Vector3) {  0,  1,  0 }
#define DOWN 	(Vector3) {  0, -1,  0 } 

// Triangle primitive struct
typedef struct {
	Vector3 vertices[3];
	Vector3 normal;

	u16 hull_id;

} Tri;

// Compute a triangle's normal vector
Vector3 TriNormal(Tri tri);

// Compute a triangle's centroid vector
Vector3 TriCentroid(Tri tri);

// Return a triangle translated to new position
Tri TriTranslate(Tri tri, Vector3 point);

// Plane struct, used for volume to surface collision
typedef struct {
	Vector3 normal;
	float d;

} Plane;

// Get plane from tri
Plane TriToPlane(Tri tri);

// Calculate signed distance from point to a plane
float PlaneDistance(Plane plane, Vector3 point);

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

// Translate a box to a new position
BoundingBox BoxTranslate(BoundingBox box, Vector3 point);

// Get a box with it's points clamped to the back side of a plane
BoundingBox BoxFitToSurface(BoundingBox box, Vector3 point, Vector3 normal);

float BoxGetSurfaceDepth(BoundingBox box, Vector3 point, Vector3 normal);

Vector3 BoxSurfaceDelta(BoundingBox box, Vector3 point, Vector3 normal);

typedef struct {
	Vector3 v[8];
	
} BoxPoints; 

BoxPoints BoxGetPoints(BoundingBox box);

typedef struct {
	Vector3 n[6]; 	

} BoxNormals;

BoxNormals BoxGetFaceNormals(BoundingBox box);

// Create a primitive array from mesh
Tri *MeshToTris(Mesh mesh, u16 *tri_count);

// Create a primitive array from model (with indexing)
Tri *ModelToTris(Model model, u16 *tri_count, u16 **tri_ids);

// Tri primitive collection
typedef struct { 
	Tri *arr;
	u16 *ids;
	u16 count;

} TriPool;

#define MAX_TRIS_PER_NODE	4
// BVH node struct
typedef struct {
	// Minimum and maximum points
	BoundingBox bounds;				// 24 bytes

	// Number of primitives 
	u16 tri_count;					// 2 bytes

	// Index of first primitive contained in node
	u16 first_tri;					// 2 bytes

	// Child node indices
	u16 child_lft, child_rgt;		// 4 bytes

} BvhNode;							// 32 byte total

// *
// Not used 
typedef struct {
	BoundingBox bounds;

	u16 hull_id;

	u16 child_lft, child_rgt;

} HullBvhNode;

#define BVH_BIN_COUNT	16
typedef struct {
	BoundingBox bounds;
	u16 count;

} Bin;

#define BVH_TREE_START_CAPACITY	1024
// BVH Tree struct
// Separate from nodes for indexed based approach
// Using pointers means at least 2x node size, slowing search
typedef struct {
	TriPool tris;

	BvhNode *nodes;

	Vector3 shape;

	u16 count;
	u16 capacity;

} BvhTree;

#define HULL_MAX_PLANES 18
#define HULL_MAX_VERTS	24	
typedef struct {
	Plane planes[HULL_MAX_PLANES];
	Vector3 verts[HULL_MAX_VERTS];

	BoundingBox aabb;

	Vector3 center;

	u16 plane_count;
	u16 vert_count;

} Hull;

typedef struct {
	Hull *arr;	
	u16 count;

} HullPool;

#define MAP_SECT_LOADED	0x01
#define MAP_SECT_QUEUED	0x02
typedef struct {
	BvhTree bvh[4];
	TriPool _tris[4];
	HullPool _hulls[4];

	NavGraph base_navgraph;
	NavGraph *navgraphs;

	Model model;

	u16 hull_count;

	u8 navgraph_count;
	u8 flags;

} MapSection;

// Calculate cost of a node
float BvhNodeCost(BvhNode *node);

// Grow bounding box of a node using it's contained primitives
void BvhNodeUpdateBounds(MapSection *sect, BvhTree *bvh, u16 node_id);

#define BODY_VOLUME_SMALL (Vector3) { 8, 8, 8 }
#define BODY_VOLUME_MEDIUM (Vector3) { 28, 64, 28 }

enum BVH_SHAPES : u8 {
	BVH_POINT		= 0,	
	BVH_BOX_MED 	= 1,
	BVH_BOX_SMALL	= 2
};

// Start BVH tree construction
void BvhConstruct(MapSection *sect, BvhTree *bvh, Vector3 volume, TriPool *tri_pool);

// Unload BVH tree
void BvhClose(BvhTree *bvh);

// Compute optimal axis and position for node subdivision  
float FindBestSplit(MapSection *sect, BvhTree *Bvh, BvhNode *node, short *axis, float *split_pos);

// Recursively split BVH nodes 
void BvhNodeSubdivide(MapSection *sect, BvhTree *bvh, u16 node_id);

// Initialize map section,
// Load geometry, materials, construct BVH, etc.  
void MapSectionInit(MapSection *sect, Model model);

void ConstructExpandedMapMesh(MapSection *sect, Model model, BoundingBox aabb);

// Unload map section data
void MapSectionClose(MapSection *sect);

typedef struct {
	Vector3 point;
	Vector3 normal;

	Vector3 contact;

	float distance;

	float contact_dist;

	u16 node_id;
	u16 tri_id;

	u16 hull_id;
	
	bool hit;

} BvhTraceData;

BvhTraceData TraceDataEmpty();

void BvhTraceNodes(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, float smallest_dist, BvhNode *node_hit);

// Trace a point through world space
void BvhTracePoint(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, float *smallest_dist, Vector3 *point, bool skip_root);

void BvhTracePointEx(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BvhTraceData *data, float max_dist);

void BvhBoxSweep(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BoundingBox box, BvhTraceData *data);
void BvhBoxSweepNoInvert(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BoundingBox *box, BvhTraceData *data);

typedef struct {
	Vector3 point;
	Vector3 normal;
	float t;
	
} SweepEntry;

typedef struct {
	SweepEntry *entries;
	u16 count;

} SweepStack;

void BvhBoxSweepStack(Ray ray, MapSection *sect, BvhTree *bvh, u16 node_id, BoundingBox *box, BvhTraceData *data, SweepStack *stack);

void MapSectionDisplayNormals(MapSection *sect);

float BoundsToRadius(BoundingBox bounds);

// Calculate Minkowski Difference from normal of shape A and half extents of shape B
float MinkowskiDiff(Vector3 normal, Vector3 h);

#define BVH_INTERSECT_MAX_NODES 8
typedef struct {
	u16 nodes[BVH_INTERSECT_MAX_NODES];
	u16 count;

} IntersectData;

IntersectData IntersectDataEmpty();

void BvhBoxIntersect(BoundingBox box, MapSection *sect, BvhTree *bvh, u16 node_id, IntersectData *data);

#endif

