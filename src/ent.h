#include "raylib.h"
#include "../include/num_redefs.h"
#include "input_handler.h"
#include "geo.h"

#ifndef ENT_H_
#define ENT_H_

#define UP (Vector3) {0, 1, 0}

typedef struct  {
	BoundingBox bounds;

	Vector3 position;
	Vector3 velocity;
	
	Vector3 forward;

	float pitch, yaw, roll;

} comp_Transform;

typedef struct {
	BoundingBox hit_box;

	float damage_cooldown;
	short amount;

	// Hit function id
	short on_hit;

	// Destroy function id
	short on_destroy;

} comp_Health;

void ApplyDamage(comp_Health *comp_health, short amount);

#define WEAPON_TRAVEL_HITSCAN		0
#define WEAPON_TRAVEL_PROJECTILE	1	

typedef struct {
	float cooldown;

	u16 model_id;

	short travel_type;

	u8 ammo_type;
	u8 ammo;

} comp_Weapon;

#define ENT_ACTIVE	0x01

enum ENT_BEHAVIORS : i8 {
	ENT_BEHAVIOR_NONE 		= -1,
	ENT_BEHAVIOR_PLAYER		=  0,
};

typedef struct {
	comp_Transform comp_transform;
	comp_Health comp_health;
	comp_Weapon comp_weapon;

	i8 behavior_id;

	u8 flags;

} Entity;

typedef struct {
	Entity *ents;

	u16 count;
	u16 capacity;

	u16 player_id;

} EntityHandler;

void EntHandlerInit(EntityHandler *handler);
void EntHandlerClose(EntityHandler *handler);

void UpdateEntities(EntityHandler *handler, float dt);
void RenderEntities(EntityHandler *handler);

void PlayerInit(Camera3D *camera, InputHandler *input, MapSection *test_section);

void PlayerUpdate(Entity *player, float dt);
void PlayerDraw(Entity *player);

void PlayerDamage(Entity *player, short amount);
void PlayerDie(Entity *player);

#endif
