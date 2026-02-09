#include <math.h>
#include <float.h>
#include <stdio.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "geo.h"
#include "player_gun.h"

#define PLAYER_MAX_PITCH (89.0f * DEG2RAD)
#define PLAYER_SPEED 130.0f
#define PLAYER_MAX_VEL 200.5f

#define PLAYER_MAX_ACCEL 15.5f
float player_accel;
float player_accel_forward;
float player_accel_side;

bool land_frame = false;
float y_vel_prev;

#define PLAYER_FRICTION 11.25f 
#define PLAYER_AIR_FRICTION 9.05f

#define PLAYER_BASE_JUMP_FORCE 180

Camera3D *ptr_cam;
InputHandler *ptr_input;
MapSection *ptr_sect;

void PlayerInput(Entity *player, InputHandler *input, float dt);

float cam_bob, cam_tilt;

BoxPoints box_points;

PlayerDebugData *player_debug_data = { 0 };

Hull test_hull;

void PlayerInit(Camera3D *camera, InputHandler *input, MapSection *test_section, PlayerDebugData *debug_data) {
	ptr_cam = camera;
	ptr_input = input;
	ptr_sect = test_section;

	player_debug_data = debug_data;

	Mesh test_mesh = GenMeshCube(30, 30, 30);
	u16 tri_count = 0;
	MeshToTris(test_mesh, &tri_count, 0, &test_hull);
}

void PlayerUpdate(Entity *player, float dt) {
	player->comp_transform.bounds = BoxTranslate(player->comp_transform.bounds, player->comp_transform.position);

	PlayerInput(player, ptr_input, dt);

	y_vel_prev = player->comp_transform.velocity.y;
	land_frame = false;

	// **
	// Apply friction
	float friction = (player->comp_transform.on_ground) ? PLAYER_FRICTION : PLAYER_AIR_FRICTION;

	player->comp_transform.velocity.x += -player->comp_transform.velocity.x * (friction) * dt;
	if(fabsf(player->comp_transform.velocity.x) <= EPSILON) player->comp_transform.velocity.x = 0;

	player->comp_transform.velocity.z += -player->comp_transform.velocity.z * (friction) * dt;
	if(fabsf(player->comp_transform.velocity.z) <= EPSILON) player->comp_transform.velocity.z = 0;

	player->comp_transform.velocity.x = Clamp(player->comp_transform.velocity.x, -PLAYER_MAX_VEL, PLAYER_MAX_VEL);
	player->comp_transform.velocity.z = Clamp(player->comp_transform.velocity.z, -PLAYER_MAX_VEL, PLAYER_MAX_VEL);
	// **

	Vector3 horizontal_velocity = (Vector3) { player->comp_transform.velocity.x, 0, player->comp_transform.velocity.z };
	Vector3 wish_point = Vector3Add(player->comp_transform.position, horizontal_velocity);


	ApplyMovement(&player->comp_transform, wish_point, ptr_sect, &ptr_sect->bvh[0], dt);
	if(player->comp_transform.velocity.y == 0 && y_vel_prev <= -335.0f) {
		land_frame = true;
	}

	ApplyGravity(&player->comp_transform, ptr_sect, &ptr_sect->bvh[0], GRAV_DEFAULT, dt);

	/*
	PlayerMove(player, dt);
	player->comp_transform.position = Vector3Add(player->comp_transform.position, Vector3Scale(player->comp_transform.velocity, 100 * dt));
	*/

	ptr_cam->position = Vector3Add(player->comp_transform.position, Vector3Scale(UP, 1.0f));
	ptr_cam->target = Vector3Add(ptr_cam->position, player->comp_transform.forward);

	if(!player->comp_transform.on_ground) cam_bob = 0;
	ptr_cam->position.y += cam_bob;
	ptr_cam->target.y += cam_bob;

	box_points = BoxGetPoints(player->comp_transform.bounds);
}

void PlayerDraw(Entity *player) {
	//PlayerDisplayDebugInfo(player);
}

void PlayerInput(Entity *player, InputHandler *input, float dt) {
	// Adjust pitch and yaw using mouse delta
	player->comp_transform.pitch = Clamp(
		player->comp_transform.pitch - input->mouse_delta.y * input->mouse_sensitivity, -PLAYER_MAX_PITCH, PLAYER_MAX_PITCH);

	player->comp_transform.yaw += input->mouse_delta.x * input->mouse_sensitivity;
	
	// Update player's forward vector
	player->comp_transform.forward = (Vector3) {
		.x = cosf(player->comp_transform.yaw) * cosf(player->comp_transform.pitch),
		.y = sinf(player->comp_transform.pitch),
		.z = sinf(player->comp_transform.yaw) * cosf(player->comp_transform.pitch)
	};

	player->comp_transform.forward = Vector3Normalize(player->comp_transform.forward);

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

	/*
	if(land_frame) 
		cam_roll_targ = Vector3RotateByAxisAngle(ptr_cam->up, player->comp_transform.forward, 15 + (y_vel_prev) / 1000);
		//ptr_cam->up = Vector3RotateByAxisAngle(ptr_cam->up, player->comp_transform.forward, 15 + (y_vel_prev) / 1000);
		//ptr_cam->up = Vector3RotateByAxisAngle(ptr_cam->up, player->comp_transform.forward, 0.00005f * y_vel_prev);
	*/

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
	//BvhTracePointEx(view_ray, ptr_sect, &ptr_sect->bvh[1], 0, &tr);

	BoundingBox vbox = player->comp_transform.bounds;

	BvhBoxSweep(view_ray, ptr_sect, &ptr_sect->bvh[0], 0, vbox, &tr);
	player_debug_data->view_dest = tr.point;

	if(tr.hit) {
		DrawLine3D(player->comp_transform.position, tr.point, GREEN);
		BoundingBox box = BoxTranslate(vbox, tr.contact);
		DrawBoundingBox(box, GREEN);

	} else 
		DrawRay(view_ray, GREEN);

	/*
	if(tr.hit) {
		Tri *tri = &ptr_sect->tris[tr.tri_id];
		//DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(GREEN, 0.25f));
		//DrawTriangle3D(tri->vertices[2], tri->vertices[1], tri->vertices[0], ColorAlpha(GREEN, 0.25f));

		u16 hull_id = tri->hull_id;
		Hull *hull = &ptr_sect->hulls[hull_id];

		for(short i = 0; i < hull->vertex_count; i++) {
			//DrawSphereEx(hull->vertices[i], 2, , int slices, Color color)
			DrawSphere(hull->vertices[i], 1.5f, SKYBLUE);
			//DrawPoint3D(hull->vertices[i], SKYBLUE);
		}
	}
	*/

	// Draw box points
	for(short i = 0; i < 8; i++) {
		DrawSphere(box_points.v[i], 2, RED);
	}

	player_debug_data->accel = player_accel;	

	/*
	Ray move_ray = (Ray) { .position = player->comp_transform.position, .direction = Vector3Normalize(horizontal_velocity) };
	player_debug_data->move_dir = move_ray.direction;

	BoundingBox sweep_box = player->comp_transform.bounds;
	BvhTraceData sweep_data = TraceDataEmpty();
	BvhTracePointEx(view_ray, ptr_sect, &ptr_sect->bvh[1], 0, &sweep_data);

	if(tr.hit)
		DrawLine3D(player->comp_transform.position, tr.point, GREEN);
	else 
		DrawRay(view_ray, GREEN);

	sweep_box = player->comp_transform.bounds;
	sweep_data = TraceDataEmpty();
	BvhTracePointEx(view_ray, ptr_sect, &ptr_sect->bvh[1], 0, &tr);

	if(tr.hit) {
		Tri *tri = &ptr_sect->tris[tr.tri_id];
		DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(GREEN, 0.25f));
		DrawTriangle3D(tri->vertices[2], tri->vertices[1], tri->vertices[0], ColorAlpha(GREEN, 0.25f));
	}
	*/

	/*
	float feet = player->comp_transform.position.y - (BoxExtent(player->comp_transform.bounds).y * 0.5f);	
	DrawSphere((Vector3) { player->comp_transform.position.x, feet, player->comp_transform.position.z }, 3, PINK);	
	*/
}

