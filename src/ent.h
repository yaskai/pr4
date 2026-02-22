#include "raylib.h"
#include "../include/num_redefs.h"
#include "input_handler.h"
#include "geo.h"
#include "map.h"
#include "ai.h"

#ifndef ENT_H_
#define ENT_H_

// ----------------------------------------------------------------------------------------------------------------------------
void EntDebugText();

void LoadEntityBaseModels();

typedef struct {
	i16 c;	// x, column
	i16 r;	// y, row
	i16 t;	// z, tab

} Coords;

#define ENT_GRID_CELL_EXTENTS (Vector3) { 255, 255, 255 } 
#define MAX_ENTS_PER_CELL	16
typedef struct {
	BoundingBox aabb;

	i16 ents[MAX_ENTS_PER_CELL];
	u8 ent_count;

} EntGridCell;

typedef struct {
	EntGridCell *cells;

	Vector3 origin;
	Coords size;

	i16 cell_count;	

} EntGrid;

int16_t CellCoordsToId(Coords coords, EntGrid *grid);
Coords CellIdToCoords(int16_t id, EntGrid *grid);

Coords Vec3ToCoords(Vector3 v, EntGrid *grid);
Vector3 CoordsToVec3(Coords coords, EntGrid *grid);

bool CoordsInBounds(Coords coords, EntGrid *grid);

typedef struct  {
	BoundingBox bounds;

	Vector3 position;
	Vector3 velocity;
	
	Vector3 forward;

	Vector3 ground_normal;
	Vector3 prev_pos;

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


enum weapon_types : u8 {
	WEAP_PISTOL,
	WEAP_SHOTGUN,
	WEAP_REVOLVER,
	WEAP_DISRUPTOR
};

typedef struct {
	float cooldown;

	short travel_type;

	u8 id;

	u8 ammo_type;
	u8 ammo;

} comp_Weapon;

#define ENT_ACTIVE		0x01
#define ENT_COLLIDERS	0x02

enum ENT_BEHAVIORS : i8 {
	ENT_BEHAVIOR_NONE 		= -1,
	ENT_BEHAVIOR_PLAYER		=  0,
};

enum ENT_TYPES : u8 {
	ENT_PLAYER 		 	= 	0,
	ENT_TURRET 		 	= 	1,
	ENT_MAINTAINER 	 	= 	2,
	ENT_REGULATOR	 	= 	3,
	ENT_DRONE 		 	= 	4,
	ENT_HEALTHPACK	 	= 	5,
	ENT_AMMO_PISTOL	 	= 	6,
	ENT_AMMO_SHOTGUN 	= 	7,
	ENT_AMMO_REVOLVER	=	8,
	ENT_DISRUPTOR	 	= 	9,
};

typedef struct {
	Model model;
	ModelAnimation *animations;

	comp_Ai comp_ai;
	comp_Transform comp_transform;
	comp_Health comp_health;
	comp_Weapon comp_weapon;
	
	int anim_count, curr_anim,  anim_frame;
	float anim_timer;

	u16 id;
	i16 cell_id;

	i8 type;

	u8 flags;

} Entity;

typedef struct {
	Entity *ents;

	EntGrid grid;

	Vector3 player_start;

	float ai_tick;

	u16 count;
	u16 capacity;

	u16 player_id;
	u16 bug_id;

} EntityHandler;

void EntHandlerInit(EntityHandler *handler);
void EntHandlerClose(EntityHandler *handler);

void UpdateEntities(EntityHandler *handler, MapSection *sect, float dt);
void RenderEntities(EntityHandler *handler, float dt);

void UpdateRenderList(EntityHandler *handler, MapSection *sect);

void EntGridInit(EntityHandler *handler);
void UpdateGrid(EntityHandler *handler);

void DrawEntsDebugInfo();
// ----------------------------------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------------------------------
// *** Player defs *** 
typedef struct {
	BoundingBox sweep_box;

	Vector3 view_dir, view_dest;
	Vector3 move_dir, move_dest;

	float view_length;
	float move_length;

	float accel;

} PlayerDebugData;

void PlayerInit(Camera3D *camera, InputHandler *input, MapSection *test_section, PlayerDebugData *debug_data, EntityHandler *ent_handler);

void PlayerUpdate(Entity *player, float dt);
void PlayerDraw(Entity *player);

void PlayerDamage(Entity *player, short amount);
void PlayerDie(Entity *player);

void PlayerDisplayDebugInfo(Entity *player);
void PlayerDebugText(Entity *player);

void PlayerMove(Entity *player, float dt);
// ----------------------------------------------------------------------------------------------------------------------------

void ProcessEntity(EntSpawn *spawn_point, EntityHandler *handler, NavGraph *nav_graph);
Entity SpawnEntity(EntSpawn *spawn_point, EntityHandler *handler);

// ----------------------------------------------------------------------------------------------------------------------------
// **** Enemies **** 
void TurretUpdate(Entity *ent, float dt);
void TurretDraw(Entity *ent);

void MaintainerUpdate(Entity *ent, float dt);
void MaintainerDraw(Entity *ent, float dt);
// ----------------------------------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------------------------------
// **** AI ****
void AiSystemUpdate(EntityHandler *handler, MapSection *sect, float dt);
void AiComponentUpdate(Entity *ent, EntityHandler *handler, comp_Ai *ai, Ai_TaskData *task_data, MapSection *sect, float dt);

void AiCheckInputs(Entity *ent, EntityHandler *handler, MapSection *sect);
void AiDoSchedule(Entity *ent, EntityHandler *handler, MapSection *sect, comp_Ai *ai, Ai_TaskData *task_data, float dt);
void AiDoState(Entity *ent, comp_Ai *ai, Ai_TaskData *task_data, float dt);

int FindClosestNavNode(Vector3 position, MapSection *sect);
void AiNavSetup(EntityHandler *handler, MapSection *sect);

int FindClosestNavNodeInGraph(Vector3 position, NavGraph *graph);
bool MakeNavPath(Entity *ent, NavGraph *graph, u16 target_id);

bool AiMoveToNode(Entity *ent, NavGraph *graph, u16 path_id);
void AiPatrol(Entity *ent, MapSection *sect, float dt);

void AiFixFriendSchedule(Entity *ent, EntityHandler *handler, MapSection *sect, float dt);

// ----------------------------------------------------------------------------------------------------------------------------

typedef struct {
	Vector3 point;	
	float dist;

	i16 hit_ent;

} EntTraceData;

EntTraceData EntTraceDataEmpty();

Vector3 TraceEntities(Ray ray, EntityHandler *handler, float max_dist, u16 sender, EntTraceData *trace_data);

Vector3 TraceBullet(EntityHandler *handler, MapSection *sect, Vector3 origin, Vector3 dir, u16 ent_id, bool *hit);

void DebugDrawEntText(EntityHandler *handler, Camera3D cam); 


// ----------------------------------------------------------------------------------------------------------------------------
// **** BUG ****

// Bug states
enum BUG_STATES : u8 {
	BUG_DEFAULT,		// Default state, attached to player
	BUG_LAUNCHED,		// In air
	BUG_LANDED			// On ground/enemy
};

// Bug flags
// 0x01 reserved for ENT_ACTIVE
// 0x02 reserved for ENT_COLLIDERS
#define BUG_DISRUPTED_ENEMY 0x04

void BugInit(Entity *ent, EntityHandler *handler, MapSection *sect);
void BugUpdate(Entity *ent, EntityHandler *handler, MapSection *sect, float dt);
void BugDraw(Entity *ent);

void DisruptEntity(EntityHandler *handler, u16 ent_id);
void AlertMaintainers(EntityHandler *handler, u16 disrupted_id);

// ----------------------------------------------------------------------------------------------------------------------------

#endif

