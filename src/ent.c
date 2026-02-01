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
		BvhTracePointEx(ray, sect, bvh, 0, false, &tr);

		if(!tr.hit || tr.distance > Vector3Length(wish_move)) {
			pos = Vector3Add(pos, vel);
			break;
		}

		if(fabsf(tr.normal.y) >= 1.0f - EPSILON)  
			continue;

		float allowed = (tr.distance - 0.001f);

		//if(allowed < 0) allowed = 0;
		if(fabsf(allowed) < 0.01f) {
			allowed = 0;
		}

		pos = Vector3Add(pos, Vector3Scale(ray.direction, allowed));

		if(clip_count + 1 < MAX_CLIPS)
			clips[clip_count++] = tr.normal;

		vel = wish_move;
		for(short j = 0; j < clip_count; j++) {
			float into = Vector3DotProduct(vel, clips[j]);	
			if(into < 0) {
				vel = Vector3Subtract(vel, Vector3Scale(clips[j], into));	
			}
		}

		pos = Vector3Subtract(pos, Vector3Scale(tr.normal, 0.01f));
	}

	comp_transform->position = pos;
}

void ApplyGravity(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh, float gravity, float dt) {
	comp_transform->on_ground = CheckGround(comp_transform, sect, bvh, dt);

	if(!comp_transform->on_ground) 
		CheckCeiling(comp_transform, sect, bvh);

	comp_transform->velocity.y -= gravity * dt;
	comp_transform->position.y += comp_transform->velocity.y * dt;
}

short CheckGround(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh, float dt) {
	if(comp_transform->velocity.y >= 1) return 0;

	Vector3 offset = (Vector3) { comp_transform->velocity.x, 0, comp_transform->velocity.z };
	offset = Vector3Scale(offset, dt);
	offset = Vector3Zero();

	Ray ray = (Ray) { .position = comp_transform->position, .direction = DOWN };

	BvhTraceData tr = TraceDataEmpty();
	BvhBoxSweepNoInvert(ray, sect, bvh, 0, &comp_transform->bounds, &tr);

	// No surface
	if(tr.distance > 0.1f) 
		return 0;

	//float dot = Vector3DotProduct(tr.normal, DOWN);
	//if(dot > 0.0f) return 0;

	float diff = tr.point.y - comp_transform->position.y;
	if(diff < 0.0f) {
		comp_transform->velocity.y = 0;
		return 0;
	}

	// Surface found and is floor
	comp_transform->position.y = tr.point.y;
	comp_transform->velocity.y = 0;

	return 1;
}

short CheckCeiling(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh) {
	Ray ray = (Ray) { .position = comp_transform->position, .direction = UP };
	
	BvhTraceData tr = TraceDataEmpty();	
	BvhBoxSweepNoInvert(ray, sect, bvh, 0, &comp_transform->bounds, &tr);

	if(tr.distance > EPSILON)
		return 0;

	float dot = Vector3DotProduct(tr.normal, UP);
	if(dot > 0.0f) return 0;

	comp_transform->position.y = tr.point.y;
	comp_transform->velocity.y = 0;

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

