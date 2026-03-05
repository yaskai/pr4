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
#include "pm.h"

#define PLAYER_MAX_PITCH (89.0f * DEG2RAD)
#define PLAYER_SPEED 265.0f
#define PLAYER_MAX_SPEED 300.0f
#define PLAYER_MAX_VEL 300.5f

#define PLAYER_GROUND_SPEED 256.0f
#define PLAYER_AIR_SPEED	270.0f

#define PLAYER_MAX_ACCEL 15.5f
float player_accel;
float player_accel_forward;
float player_accel_side;

float cam_input_forward, cam_input_side;

bool land_frame = false;
float z_vel_prev;

#define FALLDAMAGE_THRESHOLD 800.0f
#define FALLDAMAGE_MULTIPLIER -0.095f

short nudged_this_frame = 0;

#define PLAYER_FRICTION 14.25f 
#define PLAYER_AIR_FRICTION 0.75f
#define PLAYER_HURT_FRICTION 40.0f

Vector2 FlatVec2(Vector3 vec3) { return (Vector2) { vec3.x, vec3.y }; }
Vector3 clipZ(Vector3 vec) { return Vector3Normalize((Vector3) { vec.x, vec.y, 0 }); }

Camera3D *ptr_cam;
InputHandler *ptr_input;
MapSection *ptr_sect;
EntityHandler *ptr_ent_handler;

void PlayerInput(Entity *player, InputHandler *input, float dt);

float cam_bob, cam_tilt;

BoxPoints box_points;

PlayerDebugData *player_debug_data = { 0 };

void cam_Adjust(comp_Transform *ct, float dt);

Vector3 dbg_hull_norm;
float dbg_hull_pen;

pmTraceData last_pm = { .start_in_solid = -1, .end_in_solid = -1 };

Vector3 debug_vel_full;
Vector3 debug_vel_clipped;

bool player_dead = false;
float death_timer = 0;

i16 player_curr_checkpoint = -1;

// **
// -----------------------------------------------------------------------------

void pm_TraceMoveEx(Entity *ent, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt, EntityHandler *handler) {
	comp_Transform *ct = &ent->comp_transform;

	Bsp_Hull *bsp = &ptr_sect->bsp[1];
	EntGrid *grid = &handler->grid;

	*pm = (pmTraceData) { .start_in_solid = -1, .end_in_solid = -1, .origin = start, .block = 0, .clip_count = 0 };

	Vector3 dest = start;
	Vector3 vel = wish_vel;

	pm->start_vel = wish_vel;

	float t_remain = dt;

	Vector3 clips[MAX_CLIPS] = {0};
	u8 num_clips = 0;

	for(short i = 0; i < MAX_BUMPS; i++) {
		// End slide trace if velocity too low
		if(Vector3LengthSqr(vel) <= STOP_EPS)
			break;
		
		// Scale slide movement by time remaining
		Vector3 move = Vector3Scale(vel, t_remain);

		// Upate ray
		Ray ray = (Ray) { .position = dest, .direction = Vector3Normalize(move) };

		// Trace geometry 
		Bsp_TraceData tr = Bsp_TraceDataEmpty();
		Bsp_RecursiveTraceEx(bsp, bsp->first_node, 0, 1, dest, Vector3Add(dest, move), &tr);

		// Determine how much of movement was obstructed
		float fraction = tr.fraction;
		fraction = Clamp(fraction, 0.0f, 1.0f);

		EntTraceData ent_tr = { .dist = Vector3Length(move), .hit_ent = -1, .point = dest, .normal = Vector3Zero() };
		Vector3 ent_point = TraceEntities(ray, handler, Vector3Length(move), ent->id, &ent_tr);

		float ent_frac = 1.0f;
		bool use_ent = (ent_tr.hit_ent != -1);
		if(use_ent) {
			ent_frac = (ent_tr.dist / Vector3Length(move));
			ent_frac = Clamp(ent_frac, 0.0f, 1.0f);
			
			use_ent = ent_frac < fraction;

			if(use_ent)
				fraction = ent_frac;
		}

		pm->fraction = fraction;

		// Update destination
		//dest = Vector3Add(dest, Vector3Scale(move, fraction - 0.01f));
		dest = Vector3Add(dest, Vector3Scale(move, fraction));

		// No obstruction, do full movement 
		if(fraction >= 1.0f) 
			break;

		if(use_ent) {
			fraction -= 0.01f;
			if(fraction < 0) fraction = 0;
		}

		// Add clip plane
		if(num_clips < MAX_CLIPS) {
			clips[num_clips] = (use_ent) ? ent_tr.normal : (Vector3) { tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2] }; 
			num_clips++;
		} else 
			break;

		// Update velocity by each clip plane
		for(short j = 0; j < num_clips; j++) {
			float into = Vector3DotProduct(vel, clips[j]);

			if(into < 0) {
				pm_ClipVelocity(vel, clips[j], &vel, 1.0005f, pm->block);
			}
		}

		// Add small offset to prevent tunneling through surfaces
		//dest = Vector3Add(dest, Vector3Scale(tr.normal, 0.1f)); 

		// Update remaining time
		t_remain *= (1 - fraction);
	}

	pm->move_dist = Vector3Distance(start, dest);
	pm->end_vel = vel;
	pm->end_pos = dest;

	pm->clip_count = num_clips;
	memcpy(pm->clips, clips, sizeof(Vector3) * num_clips);
}

void PlayerInit(Camera3D *camera, InputHandler *input, MapSection *test_section, PlayerDebugData *debug_data, EntityHandler *ent_handler) {
	ptr_cam = camera;
	ptr_input = input;
	ptr_sect = test_section;
	ptr_ent_handler = ent_handler;
	player_debug_data = debug_data;
}

void PlayerUpdate(Entity *player, float dt) {
	player_dead = (player->comp_health.amount <= 0);
	if(player_dead)	{
		player->comp_ai.state = STATE_DEAD;
		
		death_timer += dt;

		if(death_timer > 3) 
			death_timer = 3;

		player->comp_ai.task_data.timer = death_timer;
	}

	player->comp_transform.bounds = BoxTranslate(player->comp_transform.bounds, player->comp_transform.position);
	land_frame = false;

	// Update position + velocity
	pm_Move(player, &player->comp_transform, ptr_input, ptr_ent_handler, dt);
	if(land_frame) {
		//printf("land frame!\n");
		if(z_vel_prev < -FALLDAMAGE_THRESHOLD) player->comp_health.amount -= (short)(z_vel_prev * FALLDAMAGE_MULTIPLIER);
	}

	// Update camera
	if(!player_dead)
		cam_Adjust(&player->comp_transform, dt);

	player->comp_health.damage_cooldown -= dt;
}

void PlayerDraw(Entity *player) {
	//PlayerDisplayDebugInfo(player);
}

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
	box_points = BoxGetPoints(player->comp_transform.bounds);
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

void pm_Move(Entity *ent, comp_Transform *ct, InputHandler *input, EntityHandler *handler, float dt) {
	// 1. Categorize position
	ct->on_ground = pm_CheckGround(ct, ct->position);
	
	// 2. Get wishdir
	Vector3 wish_dir = Vector3Zero();

	if(!player_dead)
		wish_dir = pm_GetWishDir(ct, input);

	//float wish_speed = PLAYER_SPEED;
	float wish_speed = (ct->on_ground) ? PLAYER_GROUND_SPEED : PLAYER_AIR_SPEED;
	if(wish_speed > PLAYER_MAX_SPEED) {
		wish_speed = PLAYER_MAX_SPEED;
	} 
	Vector3 wish_vel = Vector3Scale(wish_dir, wish_speed);
	
	// 3. Apply friction
	pm_GroundFriction(ct, dt);
	pm_AirFriction(ct, dt);

	// 4. Acceleration
	float accel = (ct->on_ground) ? PLAYER_ACCEL_GROUND : PLAYER_ACCEL_AIR;
	pm_Accelerate(ct, wish_dir, wish_speed, accel, dt);

	if(ct->on_ground) {	
		pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.0025f, 0);
	}
	
	// Check jump
	pm_Jump(ct, input);

	// 5. Apply gravity
	pm_ApplyGravity(ct, dt);

	// 6. Movement tracing 
	pmTraceData pm = (pmTraceData) {0};
	//pm_TraceMove(ct, ct->position, ct->velocity, &pm, dt);
	pm_TraceMoveEx(ent, ct->position, ct->velocity, &pm, dt, handler);

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

	/*
	if(pm.start_in_solid > -1) {
		ct->velocity = Vector3Subtract(ct->last_safe_pos, ct->position);
		ct->position = ct->last_safe_pos;
		puts("correction");
	} else if(pm.end_in_solid > -1) {
		ct->velocity = Vector3Subtract(ct->last_safe_pos, ct->position);
		ct->position = ct->last_safe_pos;
		puts("correction");
	} else if (pm.start_in_solid > -1 && pm.end_in_solid > -1) {
		ct->velocity = Vector3Subtract(ct->last_safe_pos, ct->position);
		ct->position = ct->last_safe_pos;
		puts("correction");
	} else
		ct->last_safe_pos = ct->position;
	*/

	land_frame = (ct->on_ground == 1 && last_pm.start_vel.z <= -600);
	z_vel_prev = last_pm.start_vel.z;
	last_pm = pm;

	ct->on_ground = pm_CheckGround(ct, ct->position);

	for(u16 i = 0; i < handler->checkpoint_list.count; i++) {
		if(CheckCollisionSpheres(handler->checkpoint_list.points[i], 128, ct->position, 32)) {
			player_curr_checkpoint = i;
			handler->checkpoint_list.active = player_curr_checkpoint;
			break;
		}
	}
}

Vector3 pm_GetWishDir(comp_Transform *ct, InputHandler *input) {
	// Update pitch, yaw, roll	
	// (in the case of the player, these are the same for both player entity and the camera)
	ct->pitch -= input->mouse_delta.y;
	ct->yaw -= input->mouse_delta.x;

	// Clamp pitch to *almost* 90 degrees in both directions, 
	// prevents jitter/direction flipping
	ct->pitch = Clamp(ct->pitch, -PLAYER_MAX_PITCH, PLAYER_MAX_PITCH);

	// Get look direction from angles  
	Vector3 look = (Vector3) {
		.x = cosf(ct->yaw) * cosf(ct->pitch),
		.y = (sinf(ct->yaw) * cosf(ct->pitch)),
		.z = sinf(ct->pitch),
	};

	ct->targ_look = Vector3Normalize(look);

	// Update forward direction, infer 'right' (orthogonal direction to look, needed for side input)
	Vector3 forward = Vector3Normalize(look);
	Vector3 right = Vector3CrossProduct(forward, UP);

	// Copy forward to player transform for camera to use
	ct->forward = forward;			
	ptr_cam->target = Vector3Add(ptr_cam->position, ct->forward);

	forward = clipZ(forward);
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
	Vector3 sum = clipZ(Vector3Add(wish_forw, wish_side));

	return Vector3Normalize(sum);
} 

#define GROUND_EPS 0.01f
u8 pm_CheckGround(comp_Transform *ct, Vector3 position) {
	Ray ray = (Ray) { .position = ct->position, .direction = DOWN };	

	//BvhTraceData tr = TraceDataEmpty();	
	//BvhTracePointEx(ray, ptr_sect, &ptr_sect->bvh[BVH_BOX_MED], 0, &tr, 1 + GROUND_EPS);

	Bsp_TraceData tr = Bsp_TraceDataEmpty();
	Bsp_RecursiveTraceEx(
		&ptr_sect->bsp[1],
		ptr_sect->bsp[1].first_node,
		0,
		1,
		ct->position,
		Vector3Add(ct->position, Vector3Scale(DOWN, 1 + GROUND_EPS)),
		&tr
	);

	if(tr.fraction >= 1) {
		ct->ground_normal = Vector3Zero();
		return 0;
	}

	ct->ground_normal = (Vector3) { tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2] };
	//pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.00001f, 0);
	//pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.0f, 0);
	if(fabsf(ct->velocity.z) < STOP_EPS) ct->velocity.z = 0;

	return 1;
}

void pm_GroundFriction(comp_Transform *ct, float dt) {
	// Only apply friction if grounded
	if(!ct->on_ground) return;

	// If horizontal velocity is smaller than 1 just set to 0,
	// prevents infinite small movements all the time
	Vector3 vel = ct->velocity;
	vel.z = 0;

	float speed = Vector3Length(vel);
	if(speed < 0.01f) {
		ct->velocity.x = 0;
		ct->velocity.y = 0;
		return;
	}

	// Remove velocity scaled by player's current speed
	float remove = speed * PLAYER_FRICTION * dt;
	float new_speed = fmaxf(speed - remove, 0);

	vel = Vector3Scale(vel, new_speed / speed);
	
	ct->velocity.x = vel.x;
	ct->velocity.y = vel.y;
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

#define PLAYER_GRAV 980.0f
void pm_ApplyGravity(comp_Transform *ct, float dt) {
	if(ct->on_ground) {
		//ct->velocity.y = 0;
		return;
	}
	ct->velocity.z -= (PLAYER_GRAV * dt); 
}

#define MIN_TRACE_DIST (0.0333f)
#define MAX_TRACE_DIST (2000.0f)
void pm_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt) {
	Bsp_Hull *bsp = &ptr_sect->bsp[1];

	*pm = (pmTraceData) { .start_in_solid = -1, .end_in_solid = -1, .origin = start, .block = 0, .clip_count = 0 };

	Vector3 dest = start;
	Vector3 vel = wish_vel;

	pm->start_vel = wish_vel;

	float t_remain = dt;

	Vector3 clips[MAX_CLIPS] = {0};
	u8 num_clips = 0;

	for(short i = 0; i < MAX_BUMPS; i++) {
		// End slide trace if velocity too low
		if(Vector3LengthSqr(vel) <= STOP_EPS)
			break;
		
		// Scale slide movement by time remaining
		Vector3 move = Vector3Scale(vel, t_remain);

		// Upate ray
		Ray ray = (Ray) { .position = dest, .direction = Vector3Normalize(move) };

		// Trace geometry 
		Bsp_TraceData tr = Bsp_TraceDataEmpty();
		Bsp_RecursiveTraceEx(bsp, bsp->first_node, 0, 1, dest, Vector3Add(dest, move), &tr);

		// Determine how much of movement was obstructed
		float fraction = tr.fraction;
		fraction = Clamp(fraction, 0.0f, 1.0f);
		pm->fraction = fraction;

		// Update destination
		//dest = Vector3Add(dest, Vector3Scale(move, fraction - 0.01f));
		dest = Vector3Add(dest, Vector3Scale(move, fraction));

		// No obstruction, do full movement 
		if(fraction >= 1.0f) 
			break;

		// Add clip plane
		if(num_clips < MAX_CLIPS) {
			clips[num_clips] = (Vector3) { tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2] };
			num_clips++;
		} else 
			break;

		// Update velocity by each clip plane
		for(short j = 0; j < num_clips; j++) {
			float into = Vector3DotProduct(vel, clips[j]);

			if(into < 0) {
				pm_ClipVelocity(vel, clips[j], &vel, 1.0005f, pm->block);
			}
		}

		// Add small offset to prevent tunneling through surfaces
		//dest = Vector3Add(dest, Vector3Scale(tr.normal, 0.1f)); 

		// Update remaining time
		t_remain *= (1 - fraction);
	}

	pm->move_dist = Vector3Distance(start, dest);
	pm->end_vel = vel;
	pm->end_pos = dest;

	pm->clip_count = num_clips;
	memcpy(pm->clips, clips, sizeof(Vector3) * num_clips);
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

	wish_vel.z = 0;

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
	step_start.z += PM_STEP_Z;
	
	pmTraceData step_up;
	pm_TraceMove(ct, step_start, wish_vel, &step_up, dt);

	// Check for ground
	Ray ground_ray = (Ray) { .position = step_up.end_pos, .direction = DOWN };
	tr = TraceDataEmpty();
	BvhTracePointEx(ground_ray, ptr_sect, &ptr_sect->bvh[BVH_BOX_MED], 0, &tr, PM_STEP_Z + GROUND_EPS);
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
	if(normal.z >= FLOOR_NORMAL_Z)  	// Floor
		blocked |= BLOCK_GROUND;		
	else if(fabsf(normal.z) <= 0.01f)	// Wall or step
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

	Vector3 horizontal_velocity = (Vector3) { ct->velocity.x, ct->velocity.y, 0 };

	if(input->actions[ACTION_JUMP].state == 2) {
		ct->on_ground = false;
		//ct->velocity.z += (BASE_JUMP_FORCE) + (Vector3Length(horizontal_velocity) * 0.2f);
		ct->velocity.z = (BASE_JUMP_FORCE) + (Vector3Length(horizontal_velocity) * 0.33f);
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

#define CORRECTION_STEPS 16
#define NUDGE_EPS 0.001f
short pm_NudgePosition(comp_Transform *ct, u16 hull_id) {
	puts("--------------------------");
	puts("pm_NudgePosition()");

	Hull *hull = &ptr_sect->_hulls[BVH_BOX_MED].arr[hull_id];
	short moved = 0;

	for(short i = 0; i < CORRECTION_STEPS; i++) {
		float best_pen = 1.0f;
		Plane *best_plane = NULL;

		for(short j = 0; j < hull->plane_count; j++) {
			Plane *pl = &hull->planes[j];
			float dist = PlaneDistance(*pl, ct->position);

			if(dist > 0)
				continue;

			if(dist < best_pen) {
				best_pen = dist;
				best_plane = pl;
			}
		}

		if(!best_plane) {
			puts("no best plane");
			return moved;
		}

		printf("best plane: \n");
		printf("normal: { %f %f %f }\n", best_plane->normal.x, best_plane->normal.y, best_plane->normal.z);

		Vector3 push = Vector3Scale(best_plane->normal, -(best_pen + NUDGE_EPS));
		printf("push: { %f %f %f }\n", push.x, push.y, push.z);

		ct->position = Vector3Add(ct->position, push);
		
		/*
		while(PlaneDistance(*best_plane, ct->position) > 0) {
			Vector3 push = Vector3Scale(best_plane->normal, best_pen + NUDGE_EPS);
			ct->position = Vector3Subtract(ct->position, push);

			if(PlaneDistance(*best_plane, ct->position) > 0)
				break;
		}
		*/
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
	ptr_cam->position = Vector3Add(ct->position, Vector3Scale(UP, 12.0f));
	ptr_cam->target = Vector3Add(ptr_cam->position, ct->forward);

	// Apply camera motion effects (bob, tilt) 
	float t = GetTime();

	// * NOTE:
	// okay for now...
	float bob_input = (cam_input_forward + (cam_input_side * 0.5f));
	float bob_targ = (5 * bob_input * sinf(t * 13 + (cam_input_forward)) + 1);	
	cam_bob = Lerp(cam_bob, bob_targ, dt * 10);
	
	float tilt_input = cam_input_side * 0.1f;
	tilt_input = Clamp(tilt_input, -0.028f, 0.028f);
	Vector3 tilt_targ = UP;

	if(land_frame) {
		cam_bob += (18.5f * z_vel_prev * 0.00125f);
		tilt_input += 0.5f;
	}

	if(fabsf(tilt_input) >= EPSILON) tilt_targ = Vector3RotateByAxisAngle(UP, ct->forward, tilt_input);
	ptr_cam->up = Vector3Lerp(ptr_cam->up, tilt_targ, dt * 7.5f);

	if(!ct->on_ground) cam_bob = 0;
	ptr_cam->position.z += cam_bob;
	ptr_cam->target.z += cam_bob;
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
	DrawText(TextFormat("cell_id: %d", player->cell_id), 16, 960, 24, RAYWHITE);
	DrawText(TextFormat("graph_id: %d", player->comp_ai.navgraph_id), 16, 990, 24, RAYWHITE);
	DrawText(TextFormat("checkpoint: %d", player_curr_checkpoint), 16, 1020, 24, RAYWHITE);
	//DrawText(TextFormat("in hull norm: { %f, %f, %f }", dbg_hull_norm.x, dbg_hull_norm.y, dbg_hull_norm.z), 16, 1020, 24, RAYWHITE);
}

void pm_AirFriction(comp_Transform *ct, float dt) {
	// Only apply friction if not grounded
	if(ct->on_ground) return;

	// If horizontal velocity is smaller than 1 just set to 0,
	// prevents infinite small movements all the time
	Vector3 vel = ct->velocity;
	vel.z = 0;

	float speed = Vector3Length(vel);
	if(speed < 1.0f) {
		ct->velocity.x = 0;
		ct->velocity.y = 0;
		return;
	}

	// Remove velocity scaled by player's current speed
	float remove = speed * PLAYER_AIR_FRICTION * dt;
	float new_speed = fmaxf(speed - remove, 0);

	vel = Vector3Scale(vel, new_speed / speed);
	
	ct->velocity.x = vel.x;
	ct->velocity.y = vel.y;
}

void OnHitPlayer(Entity *ent, short damage) {
	comp_Health *health = &ent->comp_health;
	comp_Transform *ct = &ent->comp_transform;

	health->amount -= damage; 
}

void SpawnPlayer(Entity *ent, Vector3 position) {
	ent->comp_transform.position = position;
	ent->comp_transform.position.z += 20;

	ent->comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  1.0f);
	ent->comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -1.0f);
	ent->comp_transform.on_ground = true;

	ent->comp_health.amount = 100;
	ent->comp_health.on_hit = -1;

	ent->comp_ai.component_valid = false;
	ent->comp_ai.state = STATE_IDLE;
	ent->comp_ai.task_data.timer = 0;

	ent->flags = (ENT_ACTIVE | ENT_COLLIDERS);

	death_timer = 0;
	player_dead = false;
}

