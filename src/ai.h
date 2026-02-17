#include "../include/num_redefs.h"
#include "raylib.h"

#ifndef AI_H_
#define AI_H_

#define MAX_EDGES_PER_NODE	8
typedef struct {
	Vector3 position;

	u16 edges[MAX_EDGES_PER_NODE];
	u16 edge_count;

	u16 id;

} NavNode;

typedef struct {
	u16 id_A;
	u16 id_B;

} NavEdge;

typedef struct {
	NavNode *nodes;
	NavEdge *edges;

	u16 node_count, node_cap;
	u16 edge_count, edge_cap;

} NavGraph;

typedef struct {
	NavNode nodes[64];
	u16 count;	

} NavPath;

// ** Input mask definitions ** //
//
#define AI_INPUT_SEE_PLAYER		0x0001
#define AI_INPUT_SEE_PET		0x0002	
#define AI_INPUT_SEE_GLITCHED	0x0004
#define AI_INPUT_SELF_GLITCHED	0x0008
#define AI_INPUT_TAKE_DAMAGE	0x0010
#define AI_INPUT_LOST_PLAYER	0x0020
// *** 

enum anim_states : u8 {
	STATE_IDLE,
	STATE_MOVE,	
	STATE_ATTACK,
	STATE_DIE
};

enum AI_SCHEDULES : u8 {
	SCHED_IDLE,
	SCHED_FIX,
};

typedef struct {
	float sight_cone;

	u32 input_mask;

	u8 anim_state;

	u16 navgraph_id;

	bool component_valid;

} comp_Ai;

#endif
