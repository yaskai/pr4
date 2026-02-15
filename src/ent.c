#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "geo.h"
#include "ai.h"

#define MAX_CLIPS 6
#define SLIDE_STEPS 4

#define STEP_SIZE 8.0f

// *
float ground_diff = 0;
float proj_y;

Model base_ent_models[16] = {0};
void LoadEntityBaseModels() {
	char *prefix = "resources/models";
	base_ent_models[ENT_TURRET] = LoadModel(TextFormat("%s/enemies/turret.glb", prefix));	 
	base_ent_models[ENT_MAINTAINER] = LoadModel(TextFormat("%s/enemies/maintainer.glb", prefix));	 
}

int base_ent_anims_count[16] = {0};
ModelAnimation *base_ent_anims[16] = {0};
void LoadEntityBaseAnims() {
	char *prefix = "resources/models";
	base_ent_anims[ENT_MAINTAINER] = LoadModelAnimations(TextFormat("%s/enemies/maintainer.glb", prefix), &base_ent_anims_count[ENT_MAINTAINER]);	 
}

Vector3 ClipVelocity(Vector3 in, Vector3 normal, float overbounce) {
	float3 out = Vector3ToFloatV(in), in3 = out;
	float3 n = Vector3ToFloatV(normal);

	float backoff;
	float change;

	backoff = Vector3DotProduct(in, normal) * overbounce;

	for(short i = 0; i < 3; i++) {
		change = n.v[i] * backoff;
		out.v[i] = in3.v[i] - change;

		if(out.v[i] > -EPSILON && out.v[i] < EPSILON)
			out.v[i] = 0; 

	}

	return (Vector3) { out.v[0], out.v[1], out.v[2] };
}

void ApplyMovement(comp_Transform *comp_transform, Vector3 wish_point, MapSection *sect, BvhTree *bvh, float dt) {
	Vector3 wish_move = Vector3Subtract(wish_point, comp_transform->position);
	Vector3 pos = comp_transform->position;
	Vector3 vel = wish_move;

	Vector3 clips[MAX_CLIPS] = {0};
	short clip_count = 0;

	float y_offset = -(BoxExtent(comp_transform->bounds).y) * 0.25f;
	if(!comp_transform->on_ground) y_offset = 0;

	for(short i = 0; i < SLIDE_STEPS; i++) {
		Ray ray = (Ray) { .position = Vector3Add(pos, Vector3Scale(UP, y_offset)), .direction = Vector3Normalize(vel) };

		BvhTraceData tr = TraceDataEmpty();
		//BvhTracePointEx(ray, sect, bvh, 0, &tr);
		BvhBoxSweep(ray, sect, &sect->bvh[1], 0, comp_transform->bounds, &tr);

		if(tr.contact_dist > Vector3Length(wish_move)) {
			pos = Vector3Add(pos, vel);
			break;
		}

		float allowed = (tr.contact_dist - 0.001f);

		if(fabsf(allowed) < 0.01f) {
			allowed = 0;
		}

		pos = Vector3Add(pos, Vector3Scale(ray.direction, allowed));

		if(clip_count + 1 < MAX_CLIPS)
			clips[clip_count++] = tr.normal;

		vel = wish_move;
		for(short j = 0; j < clip_count; j++) {
			float into = Vector3DotProduct(vel, clips[j]);	
			if(into <= 0.0f) {
				vel = Vector3Subtract(vel, Vector3Scale(clips[j], into));	
				comp_transform->velocity =
					Vector3Subtract(comp_transform->velocity, Vector3Scale(clips[j], (into * 1 + EPSILON) * dt));	
					//Vector3Subtract(comp_transform->velocity, Vector3Scale(comp_transform->velocity, 0.1f * dt));	
			}
		}

		pos = Vector3Add(pos, Vector3Scale(tr.normal, 0.01f));
	}

	comp_transform->position = pos;
	comp_transform->on_ground = CheckGround(comp_transform, pos, sect, &sect->bvh[1], dt);
}

void ApplyGravity(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh, float gravity, float dt) {
	if(!comp_transform->on_ground)
		comp_transform->air_time += dt;
	else 
		comp_transform->air_time = 0;

	comp_transform->on_ground = CheckGround(comp_transform, comp_transform->position, sect, bvh, dt);
	CheckCeiling(comp_transform, sect, bvh);

	if(!comp_transform->on_ground) {
		comp_transform->velocity.y -= gravity * dt;
		comp_transform->position.y += comp_transform->velocity.y * dt;
	}
}

short CheckGround(comp_Transform *comp_transform, Vector3 pos, MapSection *sect, BvhTree *bvh, float dt) {
	if(comp_transform->velocity.y > 0.0f) return 0;

	Vector3 h_vel = (Vector3) { comp_transform->velocity.x, 0, comp_transform->velocity.z };
	Vector3 offset = Vector3Scale(h_vel, dt);

	float ent_height = BoxExtent(comp_transform->bounds).y;
	float feet = (ent_height * 0.5f) - 1;

	Ray ray = (Ray) { .position = pos, .direction = DOWN };
	ray.position = comp_transform->position;
	//ray.position = Vector3Add(ray.position, offset);

	BvhTraceData tr = TraceDataEmpty();
	BvhBoxSweep(ray, sect, &sect->bvh[1], 0, comp_transform->bounds, &tr);
	//BvhTracePointEx(ray, sect, &sect->bvh[1], 0, &tr);

	if(tr.contact_dist >= 1.0f) {
		return 0;
	}

	if(tr.normal.y == 0) return 0;

	float into_slope = Vector3DotProduct(h_vel, tr.normal);
	float slope_y = (into_slope) / tr.normal.y;

	if(fabsf(tr.normal.y) == 1.0f) slope_y = 0;

	comp_transform->on_ground = 1;
	comp_transform->velocity.y = 0;

	float change = (tr.contact.y - slope_y) - (comp_transform->position.y);
	comp_transform->position.y = (tr.contact.y - slope_y);
	//comp_transform->position.y = (tr.point.y + ent_height*2) - slope_y;

	comp_transform->last_ground_surface = tr.tri_id; 
	//comp_transform->position.y += change;

	return 1;
}

short CheckCeiling(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh) {
	if(comp_transform->velocity.y < 0) return 0;

	Ray ray = (Ray) { .position = comp_transform->position, .direction = UP };
	
	BvhTraceData tr = TraceDataEmpty();	
	//BvhBoxSweepNoInvert(ray, sect, bvh, 0, &comp_transform->bounds, &tr);

	if(tr.distance > EPSILON)
		return 0;

	float dot = Vector3DotProduct(tr.normal, UP);
	if(dot > 0.0f) return 0;

	comp_transform->position.y = tr.point.y;
	comp_transform->velocity.y *= -0.5f;

	return 1;
}

void EntHandlerInit(EntityHandler *handler) {
	handler->count = 0;
	handler->capacity = 128;
	handler->ents = calloc(handler->capacity, sizeof(Entity));
	handler->player_id = 0;

	LoadEntityBaseModels();
	LoadEntityBaseAnims();
}

void EntHandlerClose(EntityHandler *handler) {
	if(handler->ents) free(handler->ents);

	for(int i = 0; i < 16; i++) {
		UnloadModel(base_ent_models[i]);
		UnloadModelAnimations(base_ent_anims[i], base_ent_anims[i]->frameCount);
	}
}

// **
// This struct stores IDs of entities to draw
#define MAX_RENDERED_ENTS	128
#define MIN_VIEW_RADIUS		(40.0*40.0)
#define MAX_VIEW_DOT		(-0.707*DEG2RAD)
typedef struct {
	u16 ids[MAX_RENDERED_ENTS];
	u16 count;	

} RenderList;
RenderList render_list = {0};

void UpdateEntities(EntityHandler *handler, MapSection *sect, float dt) {
	Entity *player_ent = &handler->ents[handler->player_id];
	PlayerUpdate(player_ent, dt);

	render_list.count = 0;
	Vector3 view_dir = player_ent->comp_transform.forward;

	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];

		if(!ent->type)
			continue;

		if(!(ent->flags & ENT_ACTIVE))
			continue;

		switch(ent->type) {
			case ENT_MAINTAINER: {
				MaintainerUpdate(ent, dt);
			} break;
		}

		AiCheckInputs(ent, handler, sect);

		// *** Render visibility checking ***

		Vector3 view_pos = player_ent->comp_transform.position;
		Vector3 to_player = Vector3Subtract(view_pos, ent->comp_transform.position);

		float dist = Vector3LengthSqr(to_player);
		to_player = Vector3Normalize(to_player);

		// Entities that are very close will always be rendered
		if(dist <= MIN_VIEW_RADIUS) {
			render_list.ids[render_list.count++] = i;
			continue;
		}

		// Cull behind camera
		float vis_dot = Vector3DotProduct(to_player, view_dir);
		if(vis_dot > MAX_VIEW_DOT) 
			continue;

		Vector3 right = Vector3CrossProduct(view_dir, UP);

		short visible = 2;
		for(short j = 0; j < 2; j++) {
			short offset = (j & 0x01) ? -1 : 1;

			Vector3 test_point = Vector3Subtract(ent->comp_transform.position, Vector3Scale(right, 72 * offset));
			if(view_pos.y > ent->comp_transform.position.y) test_point.y = ent->comp_transform.bounds.max.y;

			to_player = Vector3Normalize(Vector3Subtract(view_pos, test_point));
				
			Ray ray = (Ray) { .position = view_pos, .direction = Vector3Negate(to_player) };

			BvhTraceData tr = TraceDataEmpty();
			BvhTracePointEx(ray, sect, &sect->bvh[0], 0, &tr, dist);

			if(Vector3DistanceSqr(ray.position, tr.point) < (dist + (MIN_VIEW_RADIUS*0.25f)))
				visible--;
		}

		if(visible <= 0)
			continue;

		render_list.ids[render_list.count++] = i;
	}
}


void RenderEntities(EntityHandler *handler, float dt) {
	for(u16 i = 0; i < render_list.count; i++) {
		Entity *ent = &handler->ents[render_list.ids[i]];

		if(!(ent->flags & ENT_ACTIVE)) 
			continue;

		switch(ent->type) {
			case ENT_TURRET:
				TurretDraw(ent);
				break;

			case ENT_MAINTAINER:
				MaintainerDraw(ent, dt);
				break;
		}
	}
}

void DrawEntsDebugInfo() {
	DrawText(TextFormat("diff: %.2f", ground_diff), 0, 340, 32, DARKPURPLE);
	DrawText(TextFormat("proj: %.2f", proj_y), 256, 340, 32, DARKPURPLE);
}

Entity SpawnEntity(EntSpawn *spawn_point, EntityHandler *handler) {
	Entity ent = (Entity) {0};

	ent.comp_transform.position = spawn_point->position;

	ent.comp_transform.forward.x = sinf(-spawn_point->angle * DEG2RAD);
	ent.comp_transform.forward.y = 0;
	ent.comp_transform.forward.z = -cosf(-spawn_point->angle * DEG2RAD);
	ent.comp_transform.forward = Vector3Normalize(ent.comp_transform.forward);

	ent.comp_ai = (comp_Ai) {0};
	ent.comp_ai.component_valid = true;

	ent.comp_health = (comp_Health) {0};
	ent.comp_health.amount = 100;

	// * TODO:
	// Entity type specific stuff
	ent.type = spawn_point->ent_type;
	switch(ent.type) {
		case ENT_TURRET: {
			ent.model = base_ent_models[ENT_TURRET];

			ent.comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  0.5f);
			ent.comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -0.5f);
			ent.comp_transform.bounds = BoxTranslate(ent.comp_transform.bounds, ent.comp_transform.position);
			
			float angle = atan2f(ent.comp_transform.forward.x, ent.comp_transform.forward.z);
			ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateY(angle + 90 * DEG2RAD));

			ent.comp_ai.sight_cone = 0.25f;

		} break;

		case ENT_MAINTAINER: {
			ent.model = base_ent_models[ENT_MAINTAINER];
			ent.animations = base_ent_anims[ENT_MAINTAINER];

			ent.curr_anim = 0;
			//ent.anim_frame = GetRandomValue(0, 200);

			ent.comp_transform.position.y -= 4;

			ent.comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  0.5f);
			ent.comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -0.5f);
			ent.comp_transform.bounds = BoxTranslate(ent.comp_transform.bounds, ent.comp_transform.position);
			
			float angle = atan2f(ent.comp_transform.forward.x, ent.comp_transform.forward.z);
			ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateY(angle + 90 * DEG2RAD));

			ent.comp_ai.sight_cone = 0.25f;


		} break;
	}

	ent.flags |= ENT_ACTIVE;

	return ent;
}

void TurretUpdate(Entity *ent, float dt) {
}

void TurretDraw(Entity *ent) {
	//DrawBoundingBox(ent->comp_transform.bounds, PURPLE);
	DrawModel(ent->model, ent->comp_transform.position, 1.0f, LIGHTGRAY);

	/*
	Vector3 center = BoxCenter(ent->comp_transform.bounds);
	center.y += 10;
	Vector3 forward = ent->comp_transform.forward;

	DrawLine3D(center, Vector3Add(center, Vector3Scale(forward, 60)), PURPLE);
	*/
}

void MaintainerUpdate(Entity *ent, float dt) {
	ent->anim_frame = (ent->anim_frame + 1) % ent->animations[ent->curr_anim].frameCount;
}

void MaintainerDraw(Entity *ent, float dt) {
	//float angle = atan2f(ent->comp_transform.forward.x, ent->comp_transform.forward.z);
	//ent->model.transform = MatrixMultiply(ent->model.transform, MatrixRotateY(angle * DEG2RAD));

	//DrawBoundingBox(ent->comp_transform.bounds, PURPLE);
	//DrawModel(ent->model, ent->comp_transform.position, 0.1f, LIGHTGRAY);
	ent->anim_timer -= dt;
	if(ent->anim_timer <= 0) {
		UpdateModelAnimation(ent->model, ent->animations[ent->curr_anim], ent->anim_frame);
		ent->anim_timer = (0.125f);
	}
	//UpdateModelAnimation(ent->model, ent->animations[ent->curr_anim], ent->anim_frame);
	DrawModel(ent->model, ent->comp_transform.position, 0.75f, LIGHTGRAY);

	Vector3 center = BoxCenter(ent->comp_transform.bounds);
	center.y += 10;
	Vector3 forward = ent->comp_transform.forward;

	//DrawLine3D(center, Vector3Add(center, Vector3Scale(forward, 30)), PURPLE);

	if(ent->comp_ai.input_mask & AI_INPUT_SEE_PLAYER) {
		//DrawBoundingBox(ent->comp_transform.bounds, GREEN);
		//DrawLine3D(center, Vector3Add(center, Vector3Scale(forward, 30)), GREEN);
	}
}

// Update senses inputs for an entitie's AI component,
// executed once per frame for every entity with a valid component.
void AiCheckInputs(Entity *ent, EntityHandler *handler, MapSection *sect) {
	comp_Ai *ai = &ent->comp_ai;

	// Do not check inputs on non-valid ai components 
	if(!ai->component_valid) 
		return;

	// Clear inputs
	ai->input_mask = 0;

	comp_Transform *ct = &ent->comp_transform;

	BvhTree *bvh = &sect->bvh[0];

	// ** Check if player is visible **	
	//
	Entity *player_ent = &handler->ents[handler->player_id];
	Vector3 to_player = Vector3Normalize(Vector3Subtract(player_ent->comp_transform.position, ct->position));

	// Player is in ai's sight cone
	if(Vector3DotProduct(ct->forward, to_player) > ai->sight_cone) { 
		// Check for obstructions
		Ray ray = (Ray) { .position = ct->position, .direction = to_player };
		RayCollision player_coll = GetRayCollisionBox(ray, player_ent->comp_transform.bounds);

		// Trace map geometry
		// Small affordance to account for spatial partition structure (+32)
		BvhTraceData tr = TraceDataEmpty();
		BvhTracePointEx(ray, sect, bvh, 0, &tr, player_coll.distance + 32);

		// Player hitbox collision closer than possible surface collision.
		// No obstruction, player is visible 
		if(player_coll.distance < tr.distance)
			ai->input_mask |= AI_INPUT_SEE_PLAYER;
	}
	
	// ***
}

void EntDebugText() {
}

