#include <math.h>
#include <float.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "geo.h"
#include "player_gun.h"

#define PLAYER_MAX_PITCH (89.0f * DEG2RAD)
#define PLAYER_SPEED 230.0f
#define PLAYER_MAX_SPEED 250.0f
#define PLAYER_MAX_VEL 200.5f

#define PLAYER_MAX_ACCEL 15.5f
float player_accel;
float player_accel_forward;
float player_accel_side;

float cam_input_forward, cam_input_side;

bool land_frame = false;
float y_vel_prev;

short nudged_this_frame = 0;

#define PLAYER_FRICTION 15.25f 
#define PLAYER_AIR_FRICTION 13.05f

#define PLAYER_BASE_JUMP_FORCE 420

Vector2 FlatVec2(Vector3 vec3) { return (Vector2) { vec3.x, vec3.z }; }

Vector3 clipY(Vector3 vec) { return Vector3Normalize((Vector3) { vec.x, 0, vec.z }); }

Camera3D *ptr_cam;
InputHandler *ptr_input;
MapSection *ptr_sect;

void PlayerInput(Entity *player, InputHandler *input, float dt);

float cam_bob, cam_tilt;

BoxPoints box_points;

PlayerDebugData *player_debug_data = { 0 };

void cam_Adjust(comp_Transform *ct, float dt);

Vector3 dbg_hull_norm;
float dbg_hull_pen;

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
#define PLAYER_ACCEL_AIR	6.51f
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

	u8 clip_count;				// How many clip planes were hit during trace
	u8 block;					// What blocks movement: GROUND, STEP, WALL, etc.

} pmTraceData;

pmTraceData last_pm = { .start_in_solid = -1, .end_in_solid = -1 };
//
// -*

void pm_GroundFriction(comp_Transform *ct, float dt);

void pm_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt);

void pm_GroundMove(comp_Transform *ct, Vector3 start, pmTraceData *pm, float dt, Vector3 wish_vel);

int pm_CheckHull(Vector3 point, u16 hull_id);

short pm_NudgePosition(comp_Transform *ct, u16 hull_id);

int pm_CheckHullEx(Vector3 point, u16 node_id);
int pm_NudgePositionEx(comp_Transform *ct, u16 node_id);

#define PM_STEP_Y 4.0f

#define BLOCK_GROUND 	0x01
#define BLOCK_STEP	 	0x02
u8 pm_ClipVelocity(Vector3 in, Vector3 normal, Vector3 *out, float bounce, u8 blocked);

#define BASE_JUMP_FORCE 180.0f
void pm_Jump(comp_Transform *ct, InputHandler *input);

Vector3 debug_vel_full;
Vector3 debug_vel_clipped;

// **
// -----------------------------------------------------------------------------

void PlayerInit(Camera3D *camera, InputHandler *input, MapSection *test_section, PlayerDebugData *debug_data) {
	ptr_cam = camera;
	ptr_input = input;
	ptr_sect = test_section;

	player_debug_data = debug_data;
}

void PlayerUpdate(Entity *player, float dt) {
	player->comp_transform.bounds = BoxTranslate(player->comp_transform.bounds, player->comp_transform.position);

	/*
	PlayerInput(player, ptr_input, dt);

	y_vel_prev = player->comp_transform.velocity.y;
	land_frame = false;

	// -
	// Apply friction
	float friction = (player->comp_transform.on_ground) ? PLAYER_FRICTION : PLAYER_AIR_FRICTION;

	player->comp_transform.velocity.x += -player->comp_transform.velocity.x * (friction) * dt;
	if(fabsf(player->comp_transform.velocity.x) <= EPSILON) player->comp_transform.velocity.x = 0;

	player->comp_transform.velocity.z += -player->comp_transform.velocity.z * (friction) * dt;
	if(fabsf(player->comp_transform.velocity.z) <= EPSILON) player->comp_transform.velocity.z = 0;

	player->comp_transform.velocity.x = Clamp(player->comp_transform.velocity.x, -PLAYER_MAX_VEL, PLAYER_MAX_VEL);
	player->comp_transform.velocity.z = Clamp(player->comp_transform.velocity.z, -PLAYER_MAX_VEL, PLAYER_MAX_VEL);
	// -

	Vector3 horizontal_velocity = (Vector3) { player->comp_transform.velocity.x, 0, player->comp_transform.velocity.z };
	Vector3 wish_point = Vector3Add(player->comp_transform.position, horizontal_velocity);

	ApplyMovement(&player->comp_transform, wish_point, ptr_sect, &ptr_sect->bvh[1], dt);
	if(player->comp_transform.velocity.y == 0 && y_vel_prev <= -335.0f) {
		land_frame = true;
	}

	ApplyGravity(&player->comp_transform, ptr_sect, &ptr_sect->bvh[1], GRAV_DEFAULT, dt);
	*/

	/*
	PlayerMove(player, dt);
	player->comp_transform.position = Vector3Add(player->comp_transform.position, Vector3Scale(player->comp_transform.velocity, 100 * dt));
	*/
	
	
	// Track previous y velocity,
	// needed to check if player landed on groun this frame
	//y_vel_prev = player->comp_transform.velocity.y;
	land_frame = false;

	// Update position + velocity
	pm_Move(&player->comp_transform, ptr_input, dt);
	if(land_frame) {
		//printf("land frame!\n");
		//if(y_vel_prev < -600) player->comp_health.amount--;
		if(y_vel_prev < -600) player->comp_health.amount -= (short)(y_vel_prev * -0.025f);
	}

	// Update camera
	cam_Adjust(&player->comp_transform, dt);

	/*
	ptr_cam->position = Vector3Add(player->comp_transform.position, Vector3Scale(UP, 0.0f));
	ptr_cam->target = Vector3Add(ptr_cam->position, player->comp_transform.forward);

	if(!player->comp_transform.on_ground) cam_bob = 0;
	ptr_cam->position.y += cam_bob;
	ptr_cam->target.y += cam_bob;
	*/

	box_points = BoxGetPoints(player->comp_transform.bounds);

	if(IsKeyPressed(KEY_R)) {
		player->comp_transform.position = (Vector3) { 0, 60, 0 };
		player->comp_transform.velocity = Vector3Zero();
		player->comp_transform.on_ground = true;
	} 
}

void PlayerDraw(Entity *player) {
	//PlayerDisplayDebugInfo(player);
}

/*
void PlayerInput(Entity *player, InputHandler *input, float dt) {
	// Get look direction
	player->comp_transform.forward = pm_GetLookDir(&player->comp_transform, input, dt);

	// Update camera target
	ptr_cam->target = Vector3Add(player->comp_transform.position, player->comp_transform.forward);

	Vector3 right = Vector3CrossProduct(player->comp_transform.forward, UP);

	Vector3 movement = Vector3Zero();
	Vector3 move_forward = movement, move_side = movement;

	if(input->actions[ACTION_MOVE_UP].state == INPUT_ACTION_DOWN)
		move_forward = Vector3Add(move_forward, player->comp_transform.forward);

	if(input->actions[ACTION_MOVE_DOWN].state == INPUT_ACTION_DOWN)	
		move_forward = Vector3Subtract(move_forward, player->comp_transform.forward);

	if(input->actions[ACTION_MOVE_RIGHT].state == INPUT_ACTION_DOWN)
		move_side = Vector3Add(move_side, Vector3Scale(right, 1));

	if(input->actions[ACTION_MOVE_LEFT].state == INPUT_ACTION_DOWN)	
		move_side = Vector3Subtract(move_side, Vector3Scale(right, 1));
	
	move_forward = Vector3Normalize( (Vector3) { move_forward.x, 0, move_forward.z } );
	move_side = Vector3Normalize( (Vector3) { move_side.x, 0, move_side.z } );
	movement = Vector3Normalize(Vector3Add(move_forward, move_side));

	float t = GetTime();

	float len_forward = Vector3Length(move_forward);
	float len_side = Vector3Length(move_side);

	if(len_forward + len_side > 0) {
		player_accel_forward = Clamp(player_accel_forward + (PLAYER_SPEED) * dt, 1.0f, PLAYER_MAX_ACCEL);
		//cam_bob = Lerp(cam_bob, (1.95f + len_forward * 0.75f) * sinf(t * 12 + (len_forward * 0.95f)) + 1.0f, player_accel_forward * dt);
		float bob_targ = (3 * (len_forward + (len_side * 0.5f))) * sinf(t * 12 + (len_forward) * 5.95f) + 1;
		cam_bob = Lerp(cam_bob, bob_targ, 10 * dt);
	} else {
		player_accel_forward = Clamp(player_accel_forward - (PLAYER_FRICTION) * dt, 1.0f, PLAYER_MAX_ACCEL);
		cam_bob = Lerp(cam_bob, 0, 10 * dt);
		if(cam_bob <= EPSILON) cam_bob = 0;
	}

	if(land_frame && player_accel_forward >= PLAYER_MAX_ACCEL * 0.85f)
		cam_bob += (18.5f * y_vel_prev * 0.00125f);

	Vector3 cam_roll_targ = UP;
	if(len_side) {
		player_accel_side = Clamp(player_accel_side + (PLAYER_SPEED) * dt, 0.9f, PLAYER_MAX_ACCEL);

		float side_vel = Vector3DotProduct(movement, right);

		float tilt_max = Clamp(len_side, 0, 0.05f);

		cam_tilt = (side_vel * len_side * player_accel_side);
		cam_tilt = Clamp(cam_tilt, -tilt_max, tilt_max);

		if(fabs(cam_tilt) > EPSILON) cam_roll_targ = Vector3RotateByAxisAngle(UP, player->comp_transform.forward, cam_tilt);
	} else {
		player_accel_side = Clamp(player_accel_side - (PLAYER_FRICTION) * dt, 0.0f, PLAYER_MAX_ACCEL);
	}

	// Slight tilt when player lands on ground
	if(land_frame) 
		cam_roll_targ = Vector3RotateByAxisAngle(ptr_cam->up, player->comp_transform.forward, 35);

	ptr_cam->up = Vector3Lerp(ptr_cam->up, cam_roll_targ, 0.1f);


	Vector3 vel_forward = Vector3Scale(move_forward, (PLAYER_SPEED * player_accel_forward) * dt);
	Vector3 vel_side = Vector3Scale(move_side, (PLAYER_SPEED * player_accel_side) * dt);
	Vector3 horizontal_velocity = Vector3Add(vel_forward, vel_side);

	player->comp_transform.velocity.x += horizontal_velocity.x * dt;
	player->comp_transform.velocity.z += horizontal_velocity.z * dt;

	if(input->actions[ACTION_JUMP].state == INPUT_ACTION_PRESSED) {
		if(player->comp_transform.on_ground && !CheckCeiling(&player->comp_transform, ptr_sect, &ptr_sect->bvh[0])) {
			player->comp_transform.position.y += 0.01f;	
			player->comp_transform.on_ground = 0;
			player->comp_transform.air_time = 1;
			player->comp_transform.velocity.y = PLAYER_BASE_JUMP_FORCE + player_accel_forward * 2.5f;
		}
	}
}
*/

void PlayerDamage(Entity *player, short amount) {
}

void PlayerDie(Entity *player) {
}

void PlayerDisplayDebugInfo(Entity *player) {
	DrawBoundingBox(player->comp_transform.bounds, RED);
	DrawSphere(player->comp_transform.position, 1, RED);

	//DrawCubeV(player->comp_transform.position, Vector3Scale(BODY_VOLUME_MEDIUM, 1), LIGHTGRAY);

	Ray view_ray = (Ray) { .position = ptr_cam->position, .direction = player->comp_transform.forward };	
	player_debug_data->view_dest = Vector3Add(view_ray.position, Vector3Scale(view_ray.direction, FLT_MAX * 0.25f));	

	player_debug_data->view_length = FLT_MAX;

	BvhTraceData tr = TraceDataEmpty();
	BvhTracePointEx(view_ray, ptr_sect, &ptr_sect->bvh[0], 0, &tr, FLT_MAX);
	//BvhTracePointEx(view_ray, ptr_sect, &ptr_sect->bvh[1], 0, &tr, FLT_MAX);

	if(tr.hit) {
		DrawLine3D(player->comp_transform.position, tr.point, SKYBLUE);
		
		BvhNode *node = &ptr_sect->bvh[1].nodes[tr.node_id];
		//u16 hull_id = ptr_sect->bvh[1].tris.arr[tr.tri_id].hull_id;
		//u16 hull_id = tr.hull_id;

		//DrawBoundingBox(ptr_sect->_hulls[1].arr[hull_id].aabb, SKYBLUE);

	} else {
		DrawRay(view_ray, SKYBLUE);
	}

	/*
	if(tr.hit) {
		Tri *tri = &ptr_sect->bvh[0].tris.arr[tr.tri_id];
		DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(SKYBLUE, 0.25f));
		DrawTriangle3D(tri->vertices[2], tri->vertices[1], tri->vertices[0], ColorAlpha(SKYBLUE, 0.25f));
	}
	*/

	// Draw box points
	for(short i = 0; i < 8; i++) {
		DrawSphere(box_points.v[i], 2, RED);
	}

	player_debug_data->accel = player_accel;	

	/*
	Ray rayX = (Ray) { .position = player->comp_transform.bounds.min, .direction = (Vector3) {1, 0, 0} };
	Ray rayY = (Ray) { .position = player->comp_transform.bounds.min, .direction = (Vector3) {0, 1, 0} };
	Ray rayZ = (Ray) { .position = player->comp_transform.bounds.min, .direction = (Vector3) {0, 0, 1} };
	DrawRay(rayX, RED);
	DrawRay(rayY, GREEN);
	DrawRay(rayZ, SKYBLUE);
	*/

	Ray vel_ray = (Ray) { .position = player->comp_transform.position, .direction = Vector3Normalize(debug_vel_full) };
	DrawRay(vel_ray, RAYWHITE);

	Ray clip_ray = (Ray) { .position = player->comp_transform.position, .direction = Vector3Normalize(debug_vel_clipped) };
	DrawRay(clip_ray, GREEN);
}

void pm_Move(comp_Transform *ct, InputHandler *input, float dt) {
	// 1. Categorize position
	ct->on_ground = pm_CheckGround(ct, ct->position);
	
	// 2. Get wishdir
	Vector3 wish_dir = pm_GetWishDir(ct, input);
	float wish_speed = PLAYER_SPEED;
	if(wish_speed > PLAYER_MAX_SPEED) {
		wish_speed = PLAYER_MAX_SPEED;
	} 
	Vector3 wish_vel = Vector3Scale(wish_dir, wish_speed);
	

	// 3. Apply friction (if grounded)
	pm_GroundFriction(ct, dt);

	// 4. Acceleration
	float accel = (ct->on_ground) ? PLAYER_ACCEL_GROUND : PLAYER_ACCEL_AIR;
	pm_Accelerate(ct, wish_dir, wish_speed, accel, dt);

	if(ct->on_ground) {	
		pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.005f, 0);
	}
	
	// Check jump
	pm_Jump(ct, input);

	// 5. Apply gravity
	pm_ApplyGravity(ct, dt);

	// 6. Movement tracing 
	pmTraceData pm;
	pm_TraceMove(ct, ct->position, ct->velocity, &pm, dt);

	// Step trace
	//if(ct->on_ground)
		//pm_GroundMove(ct, (Vector3) {pm.end_pos.x, pm.origin.y, pm.end_pos.z}, &pm, dt, wish_vel);
	/*
	if(ct->on_ground && fabsf(ct->velocity.y) < EPSILON) {
		float xz_dist_base = Vector2Distance(FlatVec2(ct->position), FlatVec2(pm.end_pos));

		// Step up
		Vector3 step_up_start = ct->position;
		step_up_start.y += PM_STEP_Y;

		// Trace up
		pmTraceData pm_step_up;	
		pm_TraceMove(ct, step_up_start, ct->velocity, &pm_step_up, dt);

		// Measure distance up
		float xz_dist_up = Vector2Distance(FlatVec2(step_up_start), FlatVec2(pm_step_up.end_pos));	

		// Use step up trace position if further than base trace
		if(xz_dist_up > xz_dist_base) {
			//pm = pm_step_up;
		}
	}
	*/

	debug_vel_full = ct->velocity;
	debug_vel_clipped = pm.end_vel;

	ct->velocity = pm.end_vel;
	ct->position = pm.end_pos;	

	nudged_this_frame = 0;

	land_frame = (ct->on_ground == 1 && last_pm.start_vel.y <= -300);
	y_vel_prev = last_pm.start_vel.y;
	last_pm = pm;

	ct->on_ground = pm_CheckGround(ct, ct->position);
}

Vector3 pm_GetWishDir(comp_Transform *ct, InputHandler *input) {
	// Update pitch, yaw, roll	
	// (in the case of the player, these are the same for both player entity and the camera)
	ct->pitch -= input->mouse_delta.y;
	ct->yaw += input->mouse_delta.x;

	// Clamp pitch to *almost* 90 degrees in both directions, 
	// prevents jitter/direction flipping
	ct->pitch = Clamp(ct->pitch, -PLAYER_MAX_PITCH, PLAYER_MAX_PITCH);

	// Get look direction from angles  
	Vector3 look = (Vector3) {
		.x = cosf(ct->yaw) * cosf(ct->pitch),
		.y = sinf(ct->pitch),
		.z = sinf(ct->yaw) * cosf(ct->pitch)
	};

	// Update forward direction, infer 'right' (orthogonal direction to look, needed for side input)
	Vector3 forward = Vector3Normalize(look);
	Vector3 right = Vector3CrossProduct(forward, UP);

	// Copy forward to player transform for camera to use
	ct->forward = forward;			
	ptr_cam->target = Vector3Add(ptr_cam->position, ct->forward);

	forward = clipY(forward);
	forward = Vector3Normalize(forward);
	right = Vector3CrossProduct(forward, UP);

	// Get move input from keys
	// index 0 = towards, index 1 = away
	// Forward:
	short input_forw[2] = {
		(input->actions[ACTION_MOVE_U].state == 1) ? 1 : 0,		// up	
		(input->actions[ACTION_MOVE_D].state == 1) ? 1 : 0		// down
	};
	// Side: 
	short input_side[2] = {
		(input->actions[ACTION_MOVE_R].state == 1) ? 1 : 0,		// right
		(input->actions[ACTION_MOVE_L].state == 1) ? 1 : 0		// left
	};

	// Convert input to vectors
	// Y component must be removed and vector normalized
	// Forward:
	Vector3 wish_forw = Vector3Scale(forward, input_forw[0] - input_forw[1]);

	// Side: 
	Vector3 wish_side = Vector3Scale(right, input_side[0] - input_side[1]);

	// ** NOTE:
	// Change names of these vars, it's confusing...
	cam_input_forward = Vector3Length(Vector3Normalize(wish_forw)) * (input_forw[0] - input_forw[1]);
	cam_input_side = Vector3Length(Vector3Normalize(wish_side)) * (input_side[0] - input_side[1]);

	// Add both vectors and renormalize to get direction
	Vector3 sum = clipY(Vector3Add(wish_forw, wish_side));

	return Vector3Normalize(sum);
} 

#define GROUND_EPS 0.1f
u8 pm_CheckGround(comp_Transform *ct, Vector3 position) {
	Ray ray = (Ray) { .position = ct->position, .direction = DOWN };	

	BvhTraceData tr = TraceDataEmpty();	
	BvhTracePointEx(ray, ptr_sect, &ptr_sect->bvh[BVH_BOX_MED], 0, &tr, 1 + GROUND_EPS);
	
	if(!tr.hit) {
		ct->ground_normal = Vector3Zero();
		return 0;
	}

	ct->ground_normal = tr.normal;
	pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.0f, 0);
	if(fabsf(ct->velocity.y) < 0.0001f) ct->velocity.y = 0;

	return 1;
}

void pm_GroundFriction(comp_Transform *ct, float dt) {
	// Only apply friction if grounded
	if(!ct->on_ground) return;

	// If horizontal velocity is smaller than 1 just set to 0,
	// prevents infinite small movements all the time
	Vector3 vel = ct->velocity;
	vel.y = 0;

	float speed = Vector3Length(vel);
	if(speed < 1.0f) {
		ct->velocity.x = 0;
		ct->velocity.z = 0;
		return;
	}

	// Remove velocity scaled by player's current speed
	float remove = speed * PLAYER_FRICTION * dt;
	float new_speed = fmaxf(speed - remove, 0);

	vel = Vector3Scale(vel, new_speed / speed);
	
	ct->velocity.x = vel.x;
	ct->velocity.z = vel.z;
}

void pm_Accelerate(comp_Transform *ct, Vector3 wish_dir, float wish_speed, float accel, float dt) {
	float curr_speed = Vector3DotProduct(ct->velocity, wish_dir);
	float add_speed = wish_speed - curr_speed;
	
	if(add_speed <= 0) 
		return;
	
	float accel_speed = accel * dt * wish_speed;
	if(accel_speed >= add_speed)
		accel_speed = add_speed;

	ct->velocity = Vector3Add(ct->velocity, Vector3Scale(wish_dir, accel_speed));
}

#define PLAYER_GRAV 680.0f
void pm_ApplyGravity(comp_Transform *ct, float dt) {
	if(ct->on_ground) {
		//ct->velocity.y = 0;
		return;
	}
	ct->velocity.y -= (PLAYER_GRAV * dt); 
}

void pm_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt) {
	*pm = (pmTraceData) { .start_in_solid = -1, .end_in_solid = -1, .origin = start, .block = 0 };

	// Check if inside solid before starting trace
	Ray start_ray = (Ray) { .position = start, .direction = Vector3Normalize(wish_vel) };
	BvhTraceData start_tr = TraceDataEmpty();
	BvhTracePointEx(start_ray, ptr_sect, &ptr_sect->bvh[1], 0, &start_tr, Vector3Length(wish_vel));
	if(start_tr.hit) {
		pm->start_in_solid = start_tr.hull_id;
	}

	Vector3 dest = start;
	Vector3 vel = wish_vel;

	pm->start_vel = wish_vel;

	float t_remain = dt;

	// Tracked clip planes
	Vector3 clips[MAX_CLIPS] = {0};
	u8 num_clips = 0;

	for(short i = 0; i < MAX_BUMPS; i++) {
		// End slide trace if velocity too low
		if(Vector3Length(vel) <= STOP_EPS)
			break;
		
		// Scale slide movement by time remaining
		Vector3 move = Vector3Scale(vel, t_remain);

		// Upate ray
		Ray ray = (Ray) { .position = dest, .direction = Vector3Normalize(move) };

		// Trace geometry 
		BvhTraceData tr = TraceDataEmpty();
		BvhTracePointEx(ray, ptr_sect, &ptr_sect->bvh[BVH_BOX_MED], 0, &tr, FLT_MAX);

		// Determine how much of movement was obstructed
		float fraction = (tr.distance / Vector3Length(move));
		fraction = Clamp(fraction, 0.0f, 1.0f);

		// Update destination
		dest = Vector3Add(dest, Vector3Scale(move, fraction));

		if(fraction < 1.0f)
			pm->end_in_solid = (tr.hit) ? pm_CheckHull(dest, tr.hull_id) : -1;

		// No obstruction, do full movement 
		if(fraction >= 1.0f) 
			break;

		// Add clip plane
		if(num_clips + 1 < MAX_CLIPS) {
			clips[num_clips++] = tr.normal;

			// Update velocity by each clip plane
			for(short j = 0; j < num_clips; j++) {
				float into = Vector3DotProduct(vel, clips[j]);

				if(into < 0) 
					pm_ClipVelocity(vel, clips[j], &vel, 1.0005f, pm->block);
			}

		} else 
			break;

		// Add small offset to prevent tunneling through surfaces
		dest = Vector3Add(dest, Vector3Scale(tr.normal, 0.01f));

		// Update remaining time
		t_remain *= (1 - fraction);
	}

	pm->move_dist = Vector3Distance(start, dest);
	pm->end_vel = vel;
	pm->end_pos = dest;
}

void pm_GroundMove(comp_Transform *ct, Vector3 start, pmTraceData *pm, float dt, Vector3 wish_vel) {
	/*
	if(pm->fraction == 1.0f) {
		return;
	}

	Vector3 horizontal_vel = (Vector3) { ct->velocity.x, 0, ct->velocity.z };
	horizontal_vel = Vector3Scale(horizontal_vel, dt);

	Ray wall_ray = (Ray) { .position = ct->position, .direction = Vector3Normalize(horizontal_vel) };
	BvhTraceData tr = TraceDataEmpty();
	BvhTracePointEx(wall_ray, ptr_sect, &ptr_sect->bvh[1], 0, &tr, Vector3Length(horizontal_vel));
	if(!tr.hit || tr.normal.y > 0) {
		return;
	}

	float base_dist_xz = Vector2Distance(FlatVec2(ct->position), FlatVec2(pm->end_pos));
	Vector3 base_dest = pm->end_pos;

	Vector3 step_start = ct->position;
	step_start.y += PM_STEP_Y;

	pmTraceData pm_step;
	Vector3 vel_temp = ct->velocity;
	//vel_temp.y = 0;
	pm_TraceMove(ct, step_start, wish_vel, &pm_step, dt);

	Ray down_ray = (Ray) { .position = pm_step.end_pos, .direction = DOWN };
	tr = TraceDataEmpty();
	BvhTracePointEx(down_ray, ptr_sect, &ptr_sect->bvh[1], 0, &tr, PM_STEP_Y);

	// ** NOTE: 
	// FIX THIS LATER!
	// * Do three traces for [up ^], [horizontal ->], [down V] !! * 
	float step_dist_xz = Vector2Distance(FlatVec2(step_start), FlatVec2(pm_step.end_pos));
	if(step_dist_xz > base_dist_xz && tr.hit) {
		//pm->end_pos = pm_step.end_pos;

		//pm_step.end_vel.x = wish_vel.x + (wish_vel.x * pm_step.fraction * dt); 
		//pm_step.end_vel.z = wish_vel.z + (wish_vel.z * pm_step.fraction * dt);

		Vector3 step_down = Vector3Subtract((Vector3){pm_step.end_pos.x, pm->end_pos.y, pm_step.end_pos.z}, pm_step.end_pos);
		step_down = Vector3Scale(step_down, Vector3Length(wish_vel) - (1.0f - pm_step.fraction));

		pm->end_vel = pm_step.end_vel;
		pm_TraceMove(ct, pm_step.end_pos, Vector3Add(pm_step.end_vel, step_down), pm, dt);
	}
	*/

	if(ct->ground_normal.y < 1.0f) return;

	pmTraceData base = *pm;

	// Base trace moved full distance, no stepping needed
	if(base.fraction >= 1.0f) {
		return;
	}

	wish_vel.y = 0;

	// Check for wall block
	Ray wall_ray = (Ray) { .position = start, .direction = Vector3Normalize(wish_vel) }; 
	BvhTraceData tr = TraceDataEmpty();
	BvhTracePointEx(wall_ray, ptr_sect, &ptr_sect->bvh[BVH_BOX_MED], 0, &tr, Vector3Length(wish_vel));
	// No wall, step invalid, exit...
	if(!tr.hit || tr.normal.y > 0) {
		return;
	}

	// Store base movement
	Vector3 base_pos = pm->end_pos;
	Vector3 base_vel = pm->end_vel;

	// Step up
	Vector3 step_start = start;
	step_start.y += PM_STEP_Y;
	
	pmTraceData step_up;
	pm_TraceMove(ct, step_start, wish_vel, &step_up, dt);

	// Check for ground
	Ray ground_ray = (Ray) { .position = step_up.end_pos, .direction = DOWN };
	tr = TraceDataEmpty();
	BvhTracePointEx(ground_ray, ptr_sect, &ptr_sect->bvh[BVH_BOX_MED], 0, &tr, PM_STEP_Y + GROUND_EPS);
	// No ground below trace, exit
	if(!tr.hit || tr.normal.y < 1.0f) {
		return;
	}

	Vector3 step_pos = step_up.end_pos;
	//step_pos.y += tr.distance;

	float base_xz = 
		Vector2Distance(FlatVec2(start), FlatVec2(base_pos));

	float step_xz = Vector2Distance(FlatVec2(start), FlatVec2(step_pos));

	if(step_xz <= base_xz)
		return;

	base.end_pos = step_pos;
	base.end_vel = step_up.end_vel;

	*pm = base;
}

u8 pm_ClipVelocity(Vector3 in, Vector3 normal, Vector3 *out, float bounce, u8 blocked) {
	if(normal.y >= FLOOR_NORMAL_Y)  	// Floor
		blocked |= BLOCK_GROUND;		
	else if(fabsf(normal.y) <= 0.01f)	// Wall or step
		blocked |= BLOCK_STEP; 		

	float backoff = Vector3DotProduct(in, normal) * bounce;

	Vector3 change = Vector3Scale(normal, backoff);
	*out = Vector3Subtract(in, change);

	if(fabsf(out->x) < STOP_EPS) out->x = 0;
	if(fabsf(out->y) < STOP_EPS) out->y = 0;
	if(fabsf(out->z) < STOP_EPS) out->z = 0;

	return blocked;
}

void pm_Jump(comp_Transform *ct, InputHandler *input) {
	if(!ct->on_ground) return;

	Vector3 horizontal_velocity = (Vector3) { ct->velocity.x, 0, ct->velocity.z };

	if(input->actions[ACTION_JUMP].state == 2) {
		ct->on_ground = false;
		ct->velocity.y += (BASE_JUMP_FORCE) + (Vector3Length(horizontal_velocity) * 0.2f);
	}
}

int pm_CheckHull(Vector3 point, u16 hull_id) {
	Hull *hull = &ptr_sect->_hulls[BVH_BOX_MED].arr[hull_id];
	short in = 0;
	float worst_dist = FLT_MAX;
	Vector3 worst_norm = Vector3Zero();

	for(short i = 0; i < hull->plane_count; i++) {
		Plane *pl = &hull->planes[i];

		float dist = Vector3DotProduct(pl->normal, point) - pl->d;

		if(dist < worst_dist) {
			worst_norm = pl->normal;
			worst_dist = dist;	
		}

		if(dist >= 0)
			return -1;
	}

	dbg_hull_pen = worst_dist;
	dbg_hull_norm = worst_norm;

	return hull_id;
}

#define CORRECTION_STEPS 32
#define NUDGE_EPS 0.001f
#define MAX_NUDGE 0.1f
short pm_NudgePosition(comp_Transform *ct, u16 hull_id) {
	Hull *hull = &ptr_sect->_hulls[BVH_BOX_MED].arr[hull_id];

	short moved = 0;

	for(short i = 0; i < CORRECTION_STEPS; i++) {
		float deepest_pen = 0;
		Plane plane = {0};
		float nudge = 0; 

		for(short j = 0; j < hull->plane_count; j++) {
			Plane *pl = &hull->planes[j];

			float dist = Vector3DotProduct(pl->normal, ct->position) - pl->d;
			//if(dist > 0) continue;

			if(fabsf(dist) > deepest_pen) {
				plane = (Plane) { .normal = pl->normal, .d = pl->d } ;
				deepest_pen = dist;
			}
		}

		if(!deepest_pen) return 0;
		
		//if(deepest_pen > 0) 
			//continue;

		float push = (-deepest_pen);
		//push = Clamp(push, -MAX_NUDGE, 0);

		//ct->position = Vector3Add(ct->position, Vector3Scale(plane.normal, push));

		moved = 1;
	}

	return moved;
}

int pm_CheckHullEx(Vector3 point, u16 node_id) {
	float worst_dist = 0;
	Vector3 worst_norm = Vector3Zero();

	BvhNode *node = &ptr_sect->bvh[1].nodes[node_id];
	for(u16 i = 0; i < node->tri_count; i++) {
		u16 tri_id = ptr_sect->bvh->tris.ids[node->first_tri + i];
		Tri tri = ptr_sect->bvh[1].tris.arr[tri_id];

		Plane pl = TriToPlane(tri);
		float dist = Vector3DotProduct(pl.normal, point) - pl.d;

		if(dist < worst_dist) {
			worst_dist = dist;
			worst_norm = pl.normal;
		}

		if(dist > 0)
			return -1;

		dbg_hull_pen = worst_dist;
		dbg_hull_norm = worst_norm;
	}

	return node_id;
}

#define TILT_MAX 0.1f
void cam_Adjust(comp_Transform *ct, float dt) {
	ptr_cam->position = Vector3Add(ct->position, Vector3Scale(UP, 0.0f));
	ptr_cam->target = Vector3Add(ptr_cam->position, ct->forward);

	// Apply camera motion effects (bob, tilt) 
	float t = GetTime();

	// * NOTE:
	// okay for now...
	float bob_input = (cam_input_forward + (cam_input_side * 0.5f));
	float bob_targ = (5 * bob_input * sinf(t * 13 + (cam_input_forward)) + 1);	
	cam_bob = Lerp(cam_bob, bob_targ, dt * 10);
	
	float tilt_input = cam_input_side * 0.1f;
	tilt_input = Clamp(tilt_input, -0.025f, 0.025f);
	Vector3 tilt_targ = UP;

	if(land_frame) {
		cam_bob += (18.5f * y_vel_prev * 0.00125f);
		tilt_input += 0.5f;
	}

	if(tilt_input != 0.0f) tilt_targ = Vector3RotateByAxisAngle(UP, ct->forward, tilt_input);
	ptr_cam->up = Vector3Lerp(ptr_cam->up, tilt_targ, dt * 10);

	if(!ct->on_ground) cam_bob = 0;
	ptr_cam->position.y += cam_bob;
	ptr_cam->target.y += cam_bob;
}

void PlayerDebugText(Entity *player) {
	comp_Transform *ct = &player->comp_transform;

	Rectangle rect = (Rectangle) { 
		.x = 0,
		.y = 850,
		.width = 800,
		.height = 1080 - 850
	};
	DrawRectangleRec(rect, ColorAlpha(BLACK, 0.5f));

	DrawText(TextFormat("pos: { %f, %f, %f }", ct->position.x, ct->position.y, ct->position.z), 16, 870, 24, RAYWHITE);
	DrawText(TextFormat("on_ground: %d", ct->on_ground), 16, 900, 24, RAYWHITE);
	DrawText(TextFormat("ground_norm: { %f, %f, %f }", ct->ground_normal.x, ct->ground_normal.y, ct->ground_normal.z), 16, 930, 24, RAYWHITE);
	DrawText(TextFormat("nudge: %d", nudged_this_frame), 16, 960, 24, RAYWHITE);
	DrawText(TextFormat("in hull dist: %f", dbg_hull_pen), 16, 990, 24, RAYWHITE);
	DrawText(TextFormat("in hull norm: { %f, %f, %f }", dbg_hull_norm.x, dbg_hull_norm.y, dbg_hull_norm.z), 16, 1020, 24, RAYWHITE);
}

