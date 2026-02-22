#include "raylib.h"
#include "ent.h"

#ifndef PM_H_
#define PM_H_

// -----------------------------------------------------------------------------
// ** 
// Player movement functions 
// labeled "pm_<Function>"

#define FLOOR_NORMAL_Y 0.7f

// Main movement loop
void pm_Move(comp_Transform *ct, InputHandler *input, float dt);

// Get input velocity and look direction
// In Quake: first half of "PM_Air_Move()"
Vector3 pm_GetWishDir(comp_Transform *ct, InputHandler *input); 

// Set on_ground to true if ground surface beneath player
u8 pm_CheckGround(comp_Transform *ct, Vector3 position);

void pm_ApplyGravity(comp_Transform *ct, float dt);

#define PLAYER_ACCEL_GROUND	20.0f
#define PLAYER_ACCEL_AIR	5.51f
void pm_Accelerate(comp_Transform *ct, Vector3 wish_dir, float wish_speed, float accel, float dt);

// -*
// Player movement trace data struct
// Needed for tracking certain values persistently 
//
#define MAX_CLIPS 8
#define MAX_BUMPS 12
#define STOP_EPS  0.1f
typedef struct {
	Vector3 clips[MAX_CLIPS];	// Clip planes hit 
	
	Vector3 origin;				// Start position of trace
	Vector3 start_vel;			// Starting velocity of trace

	Vector3 end_pos; 			// Destination of trace
	Vector3 end_vel;			// Velocity after clip

	Vector3 ground_normal;		// Ground normal at trace end point

	float fraction;				// How much of the requested movement was actually traveled 
	float move_dist;			// How far player moved

	int start_in_solid;			// ID of hull inside at origin, -1 if none 
	int end_in_solid;			// ID of hull inside at destination, -1 if none 

	u8 bvh_id;					// Which BVH tree to use

	u8 clip_count;				// How many clip planes were hit during trace
	u8 block;					// What blocks movement: GROUND, STEP, WALL, etc.

} pmTraceData;
//
// -*

void pm_GroundFriction(comp_Transform *ct, float dt);

void pm_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt);

void pm_GroundMove(comp_Transform *ct, Vector3 start, pmTraceData *pm, float dt, Vector3 wish_vel);

int pm_CheckHull(Vector3 point, u16 hull_id);

short pm_NudgePosition(comp_Transform *ct, u16 hull_id);

int pm_CheckHullEx(Vector3 point, u16 node_id);
int pm_NudgePositionEx(comp_Transform *ct, u16 node_id);

void pm_AirFriction(comp_Transform *ct, float dt);

#define PM_STEP_Y 4.0f

#define BLOCK_GROUND 	0x01
#define BLOCK_STEP	 	0x02
u8 pm_ClipVelocity(Vector3 in, Vector3 normal, Vector3 *out, float bounce, u8 blocked);

#define BASE_JUMP_FORCE 180.0f
void pm_Jump(comp_Transform *ct, InputHandler *input);

#endif
