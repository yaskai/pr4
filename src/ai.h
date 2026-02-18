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
	NavNode nodes[32];
	u16 count;	

	u16 curr;
	u16 targ;

} NavPath;

// ** Input mask definitions ** //
//
#define AI_INPUT_SEE_PLAYER		0x0001
#define AI_INPUT_SEE_PET		0x0002	
#define AI_INPUT_SEE_GLITCHED	0x0004
#define AI_INPUT_SELF_GLITCHED	0x0008
#define AI_INPUT_TAKE_DAMAGE	0x0010
#define AI_INPUT_LOST_PLAYER	0x0020
#define AI_INPUT_HEAR_PLAYER	0x0040
// *** 

enum ANIM_STATES : u8 {
	STATE_IDLE,
	STATE_MOVE,	
	STATE_ATTACK,
	STATE_RELOAD,
	STATE_DEAD,
};

enum AI_SCHEDULES : u8 {
	SCHED_IDLE,
	SCHED_PATROL,
	SCHED_WAIT,
	SCHED_FIX_FRIEND,
	SCHED_SEARCH_FOR_PLAYER,
};

enum AI_TASKS : u8 {
	TASK_GOTO_POINT,	
	TASK_FIRE_WEAPON,
	TASK_RELOAD_WEAPON,
	TASK_WAIT_TIME,
	TASK_FIND_POINT,
};

typedef struct {
	NavPath path;

	u32 schedule_id;
	u32 interrupt_mask;

	u16 target_entity;

} Ai_TaskData;

typedef struct {
	Ai_TaskData task_data;

	float sight_cone;

	u32 input_mask;
	u32 curr_schedule;

	int curr_navnode_id;

	u16 self_ent_id;

	u16 navgraph_id;

	u8 state;

	bool component_valid;

} comp_Ai;

#endif

