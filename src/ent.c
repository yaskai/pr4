#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "geo.h"

typedef void(*DamageFunc)(Entity *ent, short amount);
DamageFunc damage_fn[4] = {
	&PlayerDamage,
	NULL,
	NULL,
	NULL
};

typedef void(*DieFunc)(Entity *ent);
DieFunc die_fn[4] = {
	&PlayerDie,
	NULL,
	NULL,
	NULL
};

typedef void(*UpdateFunc)(Entity *ent, float dt);
UpdateFunc update_fn[4] = {
	&PlayerUpdate,
	NULL,
	NULL,
	NULL
};

typedef void(*DrawFunc)(Entity *ent);
DrawFunc draw_fn[4] = {
	&PlayerDraw,
	NULL,
	NULL,
	NULL
};

#define MAX_CLIPS 6
#define SLIDE_STEPS 4

#define STEP_SIZE 8.0f

// *
float ground_diff = 0;
float proj_y;

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
		//BvhTracePointEx(ray, sect, &sect->bvh[0], 0, &tr);
		BvhBoxSweep(ray, sect, &sect->bvh[0], 0, comp_transform->bounds, &tr);

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
					Vector3Subtract(comp_transform->velocity, Vector3Scale(clips[j], into * dt));	
					//Vector3Subtract(comp_transform->velocity, Vector3Scale(comp_transform->velocity, 0.1f * dt));	
			}
		}

		pos = Vector3Add(pos, Vector3Scale(tr.normal, 0.01f));
	}

	comp_transform->position = pos;
	comp_transform->on_ground = CheckGround(comp_transform, pos, sect, bvh, dt);
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
	ray.position = Vector3Add(ray.position, offset);

	BvhTraceData tr = TraceDataEmpty();
	BvhBoxSweep(ray, sect, &sect->bvh[0], 0, comp_transform->bounds, &tr);
	//BvhTracePointEx(ray, sect, &sect->bvh[0], 0, &tr);

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

	comp_transform->last_ground_surface = tr.tri_id; 
	//comp_transform->position.y += change;

	return 1;
}

/*
short CheckGround(comp_Transform *comp_transform, Vector3 pos, MapSection *sect, BvhTree *bvh, float dt) {
	if(comp_transform->velocity.y > 0.1f) return 0;

	Vector3 h_vel = (Vector3) { comp_transform->velocity.x, 0, comp_transform->velocity.z };
	Vector3 offset = Vector3Scale(h_vel, dt);

	float ent_height = BoxExtent(comp_transform->bounds).y;
	float feet = (ent_height * 0.5f) - 1;

	Ray ray = (Ray) { .position = pos, .direction = DOWN };
	ray.position = Vector3Add(ray.position, offset);

	BvhTraceData tr = TraceDataEmpty();
	BvhBoxSweep(ray, sect, &sect->bvh[0], 0, comp_transform->bounds, &tr);

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

	//comp_transform->last_ground_surface = tr.tri_id; 
	//comp_transform->position.y += change;

	return 1;
}
*/

short CheckCeiling(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh) {
	if(comp_transform->velocity.y < 0) return 0;

	Ray ray = (Ray) { .position = comp_transform->position, .direction = UP };
	
	BvhTraceData tr = TraceDataEmpty();	
	BvhBoxSweepNoInvert(ray, sect, bvh, 0, &comp_transform->bounds, &tr);

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
}

void EntHandlerClose(EntityHandler *handler) {
	if(handler->ents) free(handler->ents);
}

void UpdateEntities(EntityHandler *handler, float dt) {
	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];

		if(!(ent->flags & ENT_ACTIVE))
			continue;

		if(update_fn[ent->behavior_id])	
			update_fn[ent->behavior_id](ent, dt);
	}
}

void RenderEntities(EntityHandler *handler) {
	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];

		if(!(ent->flags & ENT_ACTIVE))
			continue;

		if(draw_fn[ent->behavior_id])	
			draw_fn[ent->behavior_id](ent);
	}
}

void DrawEntsDebugInfo() {
	DrawText(TextFormat("diff: %.2f", ground_diff), 0, 340, 32, DARKPURPLE);
	DrawText(TextFormat("proj: %.2f", proj_y), 256, 340, 32, DARKPURPLE);
}

