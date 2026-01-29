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

void ApplyMovement(comp_Transform *comp_transform, Vector3 wish_point, MapSection *sect, BvhTree *bvh, float dt) {
	float y_dir = (comp_transform->velocity.y >= 0) ? 1.5f : -1;
	float y_offset = -(BoxExtent(comp_transform->bounds).y * (0.235f * y_dir));

	Vector3 wish_move = Vector3Subtract(wish_point, comp_transform->position);
	Vector3 pos = comp_transform->position;
	Vector3 vel = wish_move;

	Vector3 clips[MAX_CLIPS] = {0};
	short clip_count = 0;

	for(short i = 0; i < SLIDE_STEPS; i++) {
		//Ray ray = (Ray) { .position = pos, .direction = Vector3Normalize(vel) };
		Ray ray = (Ray) { .position = Vector3Add(pos, Vector3Scale(UP, y_offset)), .direction = Vector3Normalize(vel) };

		BvhTraceData tr = TraceDataEmpty();
		BvhTracePointEx(ray, sect, bvh, 0, false, &tr);

		if(!tr.hit || tr.distance > Vector3Length(wish_move)) {
			pos = Vector3Add(pos, vel);
			break;
		}

		float allowed = (tr.distance - 0.001f);

		if(allowed < 0) allowed = 0;
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

		//pos = Vector3Subtract(pos, Vector3Scale(tr.normal, 0.01f));
	}

	
	//comp_transform->velocity = (Vector3) { vel.x, comp_transform->velocity.y, vel.z };
	comp_transform->position = pos;
}

void ApplyGravity(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh, float gravity, float dt) {
	comp_transform->on_ground = CheckGround(comp_transform, sect, bvh);

	comp_transform->velocity.y -= gravity * dt;
	comp_transform->position.y += comp_transform->velocity.y * dt;

	if(comp_transform->on_ground && fabsf(comp_transform->velocity.y) >= EPSILON) {
		comp_transform->velocity.y = 0;
	}
}

short CheckGround(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh) {
	if(comp_transform->velocity.y > 0) {
		CheckCeiling(comp_transform, sect, bvh);
		return 0;
	}

	float half_height = BoxExtent(comp_transform->bounds).y * 0.5f;
	float feet_y = BoxCenter(comp_transform->bounds).y - half_height;

	Ray ray = (Ray) { .position = comp_transform->position, .direction = Vector3Scale(UP, -1) };

	Vector3 ground_point = ray.position;
	float ground_dist = FLT_MAX;
	BvhTracePoint(ray, sect, bvh, 0, &ground_dist, &ground_point, false);

	if(ground_point.y > feet_y && ground_dist <= half_height) {
		if(comp_transform->velocity.y <= 0) {
			float delta = ground_point.y - feet_y;
			comp_transform->position.y = (ray.position.y + delta);
			return 1;
		}
	}
	
	return 0;
}

short CheckCeiling(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh) {
	float half_height = BoxExtent(comp_transform->bounds).y * 0.5f;
	float head_y = BoxCenter(comp_transform->bounds).y + half_height;

	Ray ray = (Ray) { .position = comp_transform->position, .direction = UP };

	Vector3 celing_point = ray.position;
	float ceiling_dist = FLT_MAX;
	BvhTracePoint(ray, sect, bvh, 0, &ceiling_dist, &celing_point, false);

	if(ceiling_dist < head_y && ceiling_dist <= half_height) {
		float delta = head_y - celing_point.y;
		
		if(comp_transform->velocity.y > 0) {
			comp_transform->position.y -= delta;
			comp_transform->velocity.y *= -0.5f; 
		}
		
		return 1;
	}

	return 0;
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

