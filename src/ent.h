#include "raylib.h"
#include "../include/num_redefs.h"
#include "input_handler.h"
#include "geo.h"
#include "map.h"
#include "ai.h"

#ifndef ENT_H_
#define ENT_H_

void EntDebugText();

void LoadEntityBaseModels();

typedef struct  {
	BoundingBox bounds;

	Vector3 position;
	Vector3 velocity;
	
	Vector3 forward;

	Vector3 ground_normal;

	float radius;

	float pitch, yaw, roll;

	float air_time;

	i32 last_ground_surface;

	short on_ground;

} comp_Transform;

Vector3 ClipVelocity(Vector3 in, Vector3 normal, float overbounce);
void ApplyMovement(comp_Transform *comp_transform, Vector3 wish_point, MapSection *sect, BvhTree *bvh, float dt);

#define GRAV_DEFAULT 800.0f
void ApplyGravity(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh, float gravity, float dt);
short CheckGround(comp_Transform *comp_transform, Vector3 pos, MapSection *sect, BvhTree *bvh, float dt);
short CheckCeiling(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh);

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

#define ENT_PLAYER 			0
#define ENT_TURRET 			1
#define ENT_MAINTAINER 		2
#define ENT_REGULATOR		3
#define ENT_DRONE 			4
#define ENT_HEALTHPACK		5
#define ENT_AMMO_PISTOL		6
#define ENT_AMMO_SHOTGUN	7
#define ENT_AMMO_REVOLVER	8
typedef struct {
	Model model;
	ModelAnimation *animations;

	comp_Ai comp_ai;
	comp_Transform comp_transform;
	comp_Health comp_health;
	comp_Weapon comp_weapon;
	
	int anim_count, curr_anim,  anim_frame;
	float anim_timer;

	i8 type;

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

void UpdateEntities(EntityHandler *handler, MapSection *sect, float dt);
void RenderEntities(EntityHandler *handler, float dt);

void DrawEntsDebugInfo();

// *** Player defs *** 
typedef struct {
	BoundingBox sweep_box;

	Vector3 view_dir, view_dest;
	Vector3 move_dir, move_dest;

	float view_length;
	float move_length;

	float accel;

} PlayerDebugData;

void PlayerInit(Camera3D *camera, InputHandler *input, MapSection *test_section, PlayerDebugData *debug_data);

void PlayerUpdate(Entity *player, float dt);
void PlayerDraw(Entity *player);

void PlayerDamage(Entity *player, short amount);
void PlayerDie(Entity *player);

void PlayerDisplayDebugInfo(Entity *player);
void PlayerDebugText(Entity *player);

void PlayerMove(Entity *player, float dt);
// ***

void ProcessEntity(EntSpawn *spawn_point, EntityHandler *handler);
Entity SpawnEntity(EntSpawn *spawn_point, EntityHandler *handler);

// ** Enemies ** 

void TurretUpdate(Entity *ent, float dt);
void TurretDraw(Entity *ent);

void MaintainerUpdate(Entity *ent, float dt);
void MaintainerDraw(Entity *ent, float dt);

// ***

// ** AI **

void AiCheckInputs(Entity *ent, EntityHandler *handler, MapSection *sect);
// ***

#endif
