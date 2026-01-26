#include <math.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "geo.h"

#define PLAYER_MAX_PITCH (89.0f * DEG2RAD)
#define PLAYER_SPEED 100.0f

Camera3D *ptr_cam;
InputHandler *ptr_input;
MapSection *ptr_sect;

///Ray ray;
//Vector3 point;

void PlayerInput(Entity *player, InputHandler *input, float dt);

void PlayerInit(Camera3D *camera, InputHandler *input, MapSection *test_section) {
	ptr_cam = camera;
	ptr_input = input;
	ptr_sect = test_section;
}

void PlayerUpdate(Entity *player, float dt) {
	PlayerInput(player, ptr_input, dt);	

	//ray = (Ray) { .position = player->comp_transform.position, .direction = player->comp_transform.forward }; 
	//point = Vector3Add(ray.position, Vector3Scale(ray.direction, FLT_MAX * 0.25f));	

	//BvhTracePoint(ray, ptr_sect, 0, FLT_MAX, &point);	
}

void PlayerDraw(Entity *player) {
	//DrawRay(ray, RED);
	//DrawSphere(point, 10, ColorAlpha(RED, 1.0f));
}

void PlayerInput(Entity *player, InputHandler *input, float dt) {
	// Adjust pitch and yaw using mouse delta
	player->comp_transform.pitch = Clamp(
		player->comp_transform.pitch - input->mouse_delta.y * dt * input->mouse_sensitivity, -PLAYER_MAX_PITCH, PLAYER_MAX_PITCH);

	player->comp_transform.yaw += input->mouse_delta.x * dt * input->mouse_sensitivity;
	
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

	if(input->actions[ACTION_MOVE_UP].state == INPUT_ACTION_DOWN)
		movement = Vector3Add(movement, player->comp_transform.forward);

	if(input->actions[ACTION_MOVE_DOWN].state == INPUT_ACTION_DOWN)	
		movement = Vector3Subtract(movement, player->comp_transform.forward);

	if(input->actions[ACTION_MOVE_RIGHT].state == INPUT_ACTION_DOWN)
		movement = Vector3Add(movement, right);

	if(input->actions[ACTION_MOVE_LEFT].state == INPUT_ACTION_DOWN)	
		movement = Vector3Subtract(movement, right);
	
	movement.y = 0;
	movement = Vector3Normalize(movement);
	movement = Vector3Scale(movement, PLAYER_SPEED * dt);

	player->comp_transform.position = Vector3Add(player->comp_transform.position, movement);	

	ptr_cam->position = player->comp_transform.position;
	ptr_cam->target = Vector3Add(ptr_cam->position, player->comp_transform.forward);
}

void PlayerDamage(Entity *player, short amount) {
}

void PlayerDie(Entity *player) {
}

