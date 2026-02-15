#include "../include/num_redefs.h"
#include "raylib.h"

#ifndef AI_H_
#define AI_H_

typedef struct {
	Vector3 target_point;	

	float timer;

	bool complete;

} Ai_Action;

// ** Input mask definitions **
//
#define AI_INPUT_SEE_PLAYER		0x01
#define AI_INPUT_SEE_PET		0x02	
#define AI_INPUT_SEE_GLITCHED	0x04
#define AI_INPUT_SELF_GLITCHED	0x08
// *** 

typedef struct {
	Ai_Action action_curr, action_prev, action_next;

	float sight_cone;

	u32 input_mask;

	bool component_valid;

} comp_Ai;

#endif
