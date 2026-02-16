#include "../include/num_redefs.h"
#include "raylib.h"

#ifndef AI_H_
#define AI_H_

typedef struct {
	Vector3 position;
	u16 id;

} NavNode;

typedef struct {
	u16 node_A;
	u16 node_B;

} NavEdge;

typedef struct {
	NavNode *nodes;
	NavEdge *edges;

	u16 node_count;
	u16 edge_count;

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
// *** 

enum anim_states : u8 {
	STATE_IDLE,
	STATE_MOVE,	
	STATE_ATTACK,
	STATE_DIE
};

typedef struct {
	float sight_cone;

	u32 input_mask;

	u8 anim_state;

	bool component_valid;

} comp_Ai;

#endif
