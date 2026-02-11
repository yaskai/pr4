#include <math.h>
#include <float.h>
#include <stdatomic.h>
#include <stdio.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "geo.h"
#include "player_gun.h"

#define PLAYER_MAX_PITCH (89.0f * DEG2RAD)
#define PLAYER_SPEED 230.0f
#define PLAYER_MAX_SPEED 300.0f
#define PLAYER_MAX_VEL 200.5f

#define PLAYER_MAX_ACCEL 15.5f
float player_accel;
float player_accel_forward;
float player_accel_side;

bool land_frame = false;
float y_vel_prev;

#define PLAYER_FRICTION 11.25f 
#define PLAYER_AIR_FRICTION 9.05f

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

void cam_Adjust(comp_Transform *ct);

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
void pm_CheckGround(comp_Transform *ct);

void pm_ApplyGravity(comp_Transform *ct, float dt);

#define PLAYER_ACCEL_GROUND	20.0f
#define PLAYER_ACCEL_AIR	10.0f
void pm_Accelerate(comp_Transform *ct, Vector3 wish_dir, float wish_speed, float accel, float dt);

// -*
// Player movement trace data struct
// Needed for tracking certain values persistently 
//
#define MAX_CLIPS 4
#define MAX_BUMPS 4
#define STOP_EPS  0.001f
typedef struct {
	Vector3 clips[MAX_CLIPS];	// Clip planes hit 

	Vector3 end_pos; 			// Destination of trace
	Vector3 end_vel;			// Velocity after clip

	Vector3 ground_normal;		// Ground normal at trace end point

	float fraction;				// How much of the requested movement was actually traveled 
	float move_dist;			// How far player moved

	u8 clip_count;				// How many clip planes were hit during trace
	u8 block;					// What blocks movement: GROUND, STEP, WALL, etc.

} pmTraceData;
//
// -*

void pm_GroundFriction(comp_Transform *ct, float dt);

void pm_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt);

#define PM_STEP_Y 16.0f

#define BLOCK_GROUND 	0x01
#define BLOCK_STEP	 	0x02
u8 pm_ClipVelocity(Vector3 in, Vector3 normal, Vector3 *out, float bounce);

#define BASE_JUMP_FORCE 180.0f
void pm_Jump(comp_Transform *ct, InputHandler *input);

//void pm_NudgePosition(comp_Transform *ct, );

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
	
	pm_Move(&player->comp_transform, ptr_input, dt);

	ptr_cam->position = Vector3Add(player->comp_transform.position, Vector3Scale(UP, 0.0f));
	ptr_cam->target = Vector3Add(ptr_cam->position, player->comp_transform.forward);

	if(!player->comp_transform.on_ground) cam_bob = 0;
	ptr_cam->position.y += cam_bob;
	ptr_cam->target.y += cam_bob;

	box_points = BoxGetPoints(player->comp_transform.bounds);

	if(IsKeyPressed(KEY_R)) {
		player->comp_transform.position = (Vector3) { 0, 40, 0 };
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

	if(IsKeyPressed(KEY_R)) {
		player->comp_transform.position = (Vector3) { 0, 40, 0 };
		player->comp_transform.velocity = Vector3Zero();
		player->comp_transform.on_ground = true;
	} 
}
*/

void PlayerDamage(Entity *player, short amount) {
}

void PlayerDie(Entity *player) {
}

void PlayerDisplayDebugInfo(Entity *player) {
	DrawBoundingBox(player->comp_transform.bounds, RED);

	Ray view_ray = (Ray) { .position = ptr_cam->position, .direction = player->comp_transform.forward };	
	player_debug_data->view_dest = Vector3Add(view_ray.position, Vector3Scale(view_ray.direction, FLT_MAX * 0.25f));	

	player_debug_data->view_length = FLT_MAX;

	BvhTraceData tr = TraceDataEmpty();
	BvhTracePointEx(view_ray, ptr_sect, &ptr_sect->bvh[0], 0, &tr);

	if(tr.hit) {
		DrawLine3D(player->comp_transform.position, tr.point, SKYBLUE);
	} else 
		DrawRay(view_ray, SKYBLUE);

	if(tr.hit) {
		Tri *tri = &ptr_sect->bvh[0].tris.arr[tr.tri_id];
		DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(SKYBLUE, 0.25f));
		DrawTriangle3D(tri->vertices[2], tri->vertices[1], tri->vertices[0], ColorAlpha(SKYBLUE, 0.25f));
	}

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
	pm_CheckGround(ct);
	
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

	if(ct->on_ground && Vector3DotProduct(ct->velocity, ct->ground_normal) < 0) {	
		pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.0001f);
	}
	
	// Check jump
	pm_Jump(ct, input);

	// 5. Apply gravity
	pm_ApplyGravity(ct, dt);

	// 6. Movement tracing 
	pmTraceData pm;
	pm_TraceMove(ct, ct->position, ct->velocity, &pm, dt);

	// Step trace
	if(ct->on_ground) {
		float xz_dist_base = Vector2Distance(FlatVec2(ct->position), FlatVec2(pm.end_pos));

		Vector3 step_up = ct->position;
		step_up.y += PM_STEP_Y;

		pmTraceData pm_step;
		pm_TraceMove(ct, step_up, ct->velocity, &pm_step, dt);

		pmTraceData pm_step_down;
		pm_TraceMove(ct, (Vector3) { pm_step.end_pos.x, pm_step.end_pos.y - PM_STEP_Y, pm_step.end_pos.z } , pm_step.end_vel, &pm_step_down, dt);

		float xz_dist_step = Vector2Distance(FlatVec2(ct->position), FlatVec2(pm_step.end_pos));

		if(xz_dist_step >= xz_dist_base) {
			Ray step_ray = (Ray) { .position =  pm_step.end_pos, .direction = DOWN };	
			BvhTraceData step_tr = TraceDataEmpty();
			BvhBoxSweep(step_ray, ptr_sect, &ptr_sect->bvh[1], 0, ct->bounds, &step_tr);

			if(step_tr.normal.y >= 1) {
				if(fabsf(ct->position.y - pm_step_down.end_pos.y) <= PM_STEP_Y) {
					pm.end_vel = pm_step_down.end_vel;
					pm.end_pos = pm_step.end_pos;
					pm.end_pos.y = step_tr.contact.y;
				}
			}
		}
	}

	debug_vel_full = ct->velocity;
	debug_vel_clipped = pm.end_vel;

	ct->velocity = pm.end_vel;
	ct->position = pm.end_pos;	
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
	wish_forw.y = 0;
	wish_forw = Vector3Normalize(wish_forw);

	// Side: 
	Vector3 wish_side = Vector3Scale(right, input_side[0] - input_side[1]);
	wish_side.y = 0;
	wish_side = Vector3Normalize(wish_side);

	// Add both vectors and renormalize to get direction
	Vector3 wish_dir = Vector3Add(wish_forw, wish_side);
	return Vector3Normalize(wish_dir);
} 

#define GROUND_EPS 0.001f
void pm_CheckGround(comp_Transform *ct) {
	if(ct->velocity.y >= 100.0f) {
		ct->on_ground = false;
		ct->ground_normal = Vector3Zero();
		return;
	}

	Ray ray = (Ray) { .position = ct->position, .direction = DOWN };	

	BvhTraceData tr = TraceDataEmpty();	
	BvhBoxSweep(ray, ptr_sect, &ptr_sect->bvh[1], 0, ct->bounds, &tr);
	
	if(!tr.hit || tr.normal.y < FLOOR_NORMAL_Y || tr.contact_dist > EPSILON) {
		ct->on_ground = false;
		return;
	}

	ct->on_ground = true;
	ct->ground_normal = tr.normal;
	
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
	if(ct->on_ground) return;
	ct->velocity.y -= (PLAYER_GRAV * dt); 
}

void pm_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt) {
	*pm = (pmTraceData) {0};

	Vector3 dest = start;
	Vector3 vel = wish_vel;

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
		BvhBoxSweep(ray, ptr_sect, &ptr_sect->bvh[1], 0, ct->bounds, &tr);

		// Determine how much of movement was obstructed
		float fraction = (tr.contact_dist / Vector3Length(move));
		fraction = Clamp(fraction, 0.0f, 1.0f);

		// Update destination
		dest = Vector3Add(dest, Vector3Scale(move, fraction));

		// No obstruction, do full movement 
		if(fraction >= 1.0f) 
			break;

		// Add clip plane
		if(num_clips + 1 < MAX_CLIPS)
			clips[num_clips++] = tr.normal;
		else 
			break;

		// Update velocity by each clip plane
		for(short j = 0; j < num_clips; j++) {
			float into = Vector3DotProduct(vel, clips[j]);

			if(into < 0) {
				pm_ClipVelocity(vel, clips[j], &vel, 1.0001f);
			}
		}

		// Update remaining time
		t_remain *= (1 - fraction);
	}

	pm->move_dist = Vector3Distance(start, dest);
	pm->end_vel = vel;
	pm->end_pos = dest;
}

u8 pm_ClipVelocity(Vector3 in, Vector3 normal, Vector3 *out, float bounce) {
	u8 blocked = 0;	
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

	Vector3 horizontal_velocity = (Vector3) { ct->velocity.x, 0, ct->velocity.y };

	if(input->actions[ACTION_JUMP].state == 2) {
		ct->on_ground = false;
		ct->velocity.y += (BASE_JUMP_FORCE) + (Vector3Length(horizontal_velocity) * 0.25f);
	}
}

