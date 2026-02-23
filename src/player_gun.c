#include "raylib.h"
#include "raymath.h"
#include "player_gun.h"
#include "geo.h"
#include "ent.h"
#include "v_effect.h"

Vector3 gun_pos = {0};
float gun_rot = 0;
float p, y, r;
Matrix mat = {0};

#define REVOLVER_REST (Vector3) { -0.75f, -2.35f, 6.25f }
#define REVOLVER_ANGLE_REST 2.5f

#define PISTOL_REST (Vector3) { -1.15f, -2.65f, 6.25f }
#define PISTOL_ANGLE_REST 0.1f

#define DISRUPTOR_REST (Vector3) {  -1.75f, -1.35f, 6.25f  }
#define DISRUPTOR_REST_ANGLE_REST 2.5f

#define DISRUPTOR_THROW_FORCE 300.0f

float recoil = 0;
bool recoil_add = false;

float friction = 0;

typedef struct {
	EntityHandler *handler;	
	MapSection *sect;
	Entity *player;
	vEffect_Manager *effect_manager;

} PlayerGunRefs;
PlayerGunRefs gun_refs = {0};

Model models[4];
Matrix gun_matrix;

void PlayerGunInit(PlayerGun *player_gun, Entity *player, EntityHandler *handler, MapSection *sect, vEffect_Manager *effect_manager) {
	player_gun->cam = (Camera3D) {
		.position = (Vector3) { 0, 0, -1 },
		.target = (Vector3) { 0, 0, 1 },
		.up = (Vector3) {0, 1, 0},
		.fovy = 54,
		.projection = CAMERA_PERSPECTIVE
	};

	models[WEAP_PISTOL] 	= LoadModel("resources/models/weapons/pistol_00.glb");
	models[WEAP_SHOTGUN] 	= LoadModel("resources/models/weapons/pistol_00.glb");
	models[WEAP_REVOLVER] 	= LoadModel("resources/models/weapons/rev_00.glb");
	models[WEAP_DISRUPTOR] 	= LoadModel("resources/models/weapons/bug_00.glb");

	//player_gun->model = LoadModel("resources/models/weapons/rev_00.glb");

	gun_pos = REVOLVER_REST;
	gun_rot = REVOLVER_ANGLE_REST;

	gun_refs.player = player;
	gun_refs.sect = sect;
	gun_refs.handler = handler;
	gun_refs.effect_manager = effect_manager;

	player->comp_weapon.id = WEAP_REVOLVER;
	player_gun->current_gun = player->comp_weapon.id;

	player_gun->model = models[player_gun->current_gun];
	mat = player_gun->model.transform;
}

void PlayerGunUpdate(PlayerGun *player_gun, float dt) {
	int scroll = GetMouseWheelMove();
	gun_refs.player->comp_weapon.id = (gun_refs.player->comp_weapon.id + scroll) % 4;
	player_gun->current_gun = gun_refs.player->comp_weapon.id;

	switch(player_gun->current_gun) {
		case WEAP_PISTOL:
			PlayerGunUpdatePistol(player_gun, dt);
			break;

		case WEAP_SHOTGUN:
			PlayerGunUpdateShotgun(player_gun, dt);
			break;

		case WEAP_REVOLVER:
			PlayerGunUpdateRevolver(player_gun, dt);
			break;

		case WEAP_DISRUPTOR:
		 	PlayerGunUpdateDisruptor(player_gun, dt);
			break;
	}	
}

void PlayerGunUpdatePistol(PlayerGun *player_gun, float dt) {
	float recoil_angle = Clamp(recoil + gun_rot, -30, 90.0f);
	mat = MatrixRotateX(-recoil_angle * DEG2RAD);
	mat = MatrixMultiply(mat, MatrixRotateY(PISTOL_ANGLE_REST * DEG2RAD));

	//friction = (recoil > 40) ? 4.9f : 10.5f;
	friction = 17.5f;

	recoil -= (recoil * friction) * dt; 
	if(recoil <= -EPSILON) recoil = 0;

	if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && recoil <= 0.9f) {
		PlayerShootPistol(player_gun, gun_refs.handler, gun_refs.sect);

		recoil_add = false;
		recoil += 15 + (GetRandomValue(10, 20) * 0.01f);
	}

	gun_pos = PISTOL_REST;
	gun_pos.z = PISTOL_REST.z - (recoil * 0.1f);

	models[WEAP_PISTOL].transform = mat;
}

void PlayerGunUpdateShotgun(PlayerGun *player_gun, float dt) {
}

void PlayerGunUpdateRevolver(PlayerGun *player_gun, float dt) {
	float recoil_angle = Clamp(recoil + gun_rot, -30, 90.0f);
	mat = MatrixRotateX(-recoil_angle * DEG2RAD);
	mat = MatrixMultiply(mat, MatrixRotateY(REVOLVER_ANGLE_REST * DEG2RAD));

	friction = (recoil > 40) ? 4.9f : 10.5f;

	recoil -= (recoil * friction) * dt; 
	if(recoil <= -EPSILON) recoil = 0;

	if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && recoil <= 1.0f) {
		PlayerShootRevolver(player_gun, gun_refs.handler, gun_refs.sect);

		recoil_add = false;
		recoil += 130 + (GetRandomValue(10, 30) * 0.1f);
	}

	gun_pos = REVOLVER_REST;
	gun_pos.z = REVOLVER_REST.z - (recoil * 0.05f);

	models[WEAP_REVOLVER].transform = mat;
}

void PlayerGunUpdateDisruptor(PlayerGun *player_gun, float dt) {
	gun_pos = DISRUPTOR_REST;

	mat = MatrixRotateX(-DISRUPTOR_REST_ANGLE_REST * DEG2RAD);
	mat = MatrixMultiply(mat, MatrixRotateY(DISRUPTOR_REST_ANGLE_REST * DEG2RAD));

	if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		PlayerShoot(player_gun, gun_refs.handler, gun_refs.sect);
	}

	models[WEAP_DISRUPTOR].transform = mat;
}

void PlayerGunDraw(PlayerGun *player_gun) {
	Entity *bug_ent = &gun_refs.handler->ents[gun_refs.handler->bug_id];
	bool skip_draw = false;

	if(player_gun->current_gun == WEAP_DISRUPTOR) {
		if(bug_ent->comp_ai.state > 0) {
			skip_draw = true;
		}
	}

	if(!skip_draw) {
		float scale = 1.0f;
		if(player_gun->current_gun == WEAP_DISRUPTOR)
			scale = 0.8f;

		BeginMode3D(player_gun->cam);
		DrawModel(models[gun_refs.player->comp_weapon.id], gun_pos, scale, WHITE);
		EndMode3D();
	}

	DrawText(TextFormat("_H_%d", gun_refs.player->comp_health.amount), 64, 980, 80, ColorAlpha(SKYBLUE, 0.95f));	
}

void PlayerShoot(PlayerGun *player_gun, EntityHandler *handler, MapSection *sect) {
	switch(gun_refs.player->comp_weapon.id) {
		case WEAP_PISTOL:
			PlayerShootPistol(player_gun, handler, sect);
			break;

		case WEAP_SHOTGUN:
			PlayerShootShotgun(player_gun, handler, sect);
			break;

		case WEAP_REVOLVER:
			PlayerShootRevolver(player_gun, handler, sect);
			break;

		case WEAP_DISRUPTOR:
			PlayerShootDisruptor(player_gun, handler, sect);
			break;
	}
}

void PlayerShootPistol(PlayerGun *player_gun, EntityHandler *handler, MapSection *sect) {
	comp_Transform *ct = &gun_refs.player->comp_transform;

	Vector3 trace_start = ct->position;
	trace_start.y -= 4;

	Vector3 dir = ct->forward;
	float offset = GetRandomValue(-10, 10) * 0.001f;	

	Vector3 right = Vector3CrossProduct(ct->forward, UP);
	dir = Vector3Add(dir, Vector3Scale(right, offset));

	offset = GetRandomValue(-10, 10) * 0.001f;
	dir = Vector3Add(dir, Vector3Scale(UP, offset));

	dir = Vector3Normalize(dir);

	bool trace_hit = false;
	Vector3 point = TraceBullet(
		handler,
		sect,
		trace_start,
		dir,
		handler->player_id,
		&trace_hit
	);

	Vector3 trail_start = Vector3Add(trace_start, Vector3Scale(ct->forward, 12));
	//Vector3 right = Vector3CrossProduct(ct->forward, UP);
	trail_start = Vector3Add(trail_start, Vector3Scale(right, 2.5f));

	//Vector3 trail_end = Vector3Add(trail_start, Vector3Scale(ct->forward, Vector3Distance(ct->position, point)));
	Vector3 trail_end = point;
	if(!trace_hit) {
		trail_end = Vector3Add(trail_start, Vector3Scale(ct->forward, 2000.0f));
	}

	float dist = Vector3Distance(trail_start, trail_end);

	//if(dist >= 20)
	vEffectsAddTrail(gun_refs.effect_manager, trail_start, trail_end);
	gun_refs.effect_manager->trails[gun_refs.effect_manager->trail_count-1].timer = 0.75f;
}

void PlayerShootShotgun(PlayerGun *player_gun, EntityHandler *handler, MapSection *sect) {
}

void PlayerShootRevolver(PlayerGun *player_gun, EntityHandler *handler, MapSection *sect) {
	comp_Transform *ct = &gun_refs.player->comp_transform;

	Vector3 trace_start = ct->position;
	//trace_start.y -= 4;

	bool trace_hit = false;
	Vector3 point = TraceBullet(
		handler,
		sect,
		trace_start,
		ct->forward,
		handler->player_id,
		&trace_hit
	);

	Vector3 trail_start = Vector3Add(trace_start, Vector3Scale(ct->forward, 12));
	Vector3 right = Vector3CrossProduct(ct->forward, UP);
	trail_start = Vector3Add(trail_start, Vector3Scale(right, 3.5f));

	//Vector3 trail_end = Vector3Add(trail_start, Vector3Scale(ct->forward, Vector3Distance(ct->position, point)));
	Vector3 trail_end = point;
	if(!trace_hit) {
		trail_end = Vector3Add(trail_start, Vector3Scale(ct->forward, 2000.0f));
	}

	float dist = Vector3Distance(trail_start, trail_end);

	//if(dist >= 20)
	vEffectsAddTrail(gun_refs.effect_manager, trail_start, trail_end);
}

void PlayerShootDisruptor(PlayerGun *player_gun, EntityHandler *handler, MapSection *sect) {
	Entity *bug_ent = &handler->ents[handler->bug_id];
	Entity *player_ent = &handler->ents[handler->player_id];

	comp_Ai *ai = &bug_ent->comp_ai;
	comp_Transform *ct = &bug_ent->comp_transform;

	if(ai->state > 0) 
		return;

	ai->state = BUG_LAUNCHED;
	bug_ent->flags = ENT_ACTIVE;
	bug_ent->flags |= ENT_COLLIDERS;

	ct->position = player_ent->comp_transform.position;
	ct->position.y += 10;

	ct->forward = player_ent->comp_transform.forward;
	
	ct->position = Vector3Add(ct->position, Vector3Scale(ct->forward, 10));

	float updot = Vector3DotProduct(UP, ct->forward);

	Vector3 throw_dir = (Vector3) { ct->forward.x, 0, ct->forward.z };
	throw_dir = Vector3Normalize(throw_dir);

	if(ct->forward.y <= 0.98f && ct->forward.y > 0) {
		ct->velocity = Vector3Scale(throw_dir, DISRUPTOR_THROW_FORCE);
		ct->velocity.y += 250 + (((ct->forward.y) * (DISRUPTOR_THROW_FORCE)) * updot);
	} else {
		Vector3 throw_dir = (Vector3) { ct->forward.x, ct->forward.y, ct->forward.z };
		throw_dir = Vector3Normalize(throw_dir);

		ct->velocity = Vector3Scale(throw_dir, DISRUPTOR_THROW_FORCE);
		ct->velocity.y += 250;
	}

	//ct->velocity = Vector3Add(ct->velocity, player_ent->comp_transform.velocity);

	float angle = atan2f(-ct->forward.x, -ct->forward.z);
	bug_ent->model.transform = MatrixRotateY(angle);
}

