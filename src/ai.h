#include "../include/num_redefs.h"
#include "raylib.h"

#ifndef AI_H_
#define AI_H_

typedef struct NavNode {
	struct NavNode *prev;
	struct NavNode *next;

	Vector3 position;

} NavNode;

typedef struct {
	NavNode *nodes;
	u16 count;

} NavGraph;

// ** Input mask definitions ** //
//
#define AI_INPUT_SEE_PLAYER		0x01
#define AI_INPUT_SEE_PET		0x02	
#define AI_INPUT_SEE_GLITCHED	0x04
#define AI_INPUT_SELF_GLITCHED	0x08
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
