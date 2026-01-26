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

void ApplyMovement(comp_Transform *comp_transform, Vector3 wish_point, MapSection *sect, float dt) {
	Vector3 destination = wish_point;

	Vector3 direction = Vector3Subtract(wish_point, comp_transform->position);
	float full_travel_dist = Vector3Length(direction);

	Ray ray = (Ray) { .position = comp_transform->position, .direction = Vector3Normalize(direction) };
	
	float d = FLT_MAX;
	BvhTracePoint(ray, sect, 0, &d, &destination, false);

	float partial_dist = Vector3Distance(comp_transform->position, destination);
	float delta = full_travel_dist - partial_dist;

	if(delta <= EPSILON) {
		comp_transform->position = wish_point;
		return;
	}

	Vector3 extent = BoxExtent(comp_transform->bounds);
	float radius = extent.x * 3.75f;

	float new_travel_dist = partial_dist - radius; 	
	new_travel_dist = fmaxf(new_travel_dist, 0.0f);

	destination = Vector3Add(comp_transform->position, Vector3Scale(direction, new_travel_dist));
}

void ApplyGravity(comp_Transform *comp_transform, MapSection *sect, float gravity, float dt) {
	
	comp_transform->on_ground = CheckGround(comp_transform, sect);

	if(comp_transform->on_ground) {
		comp_transform->velocity.y = 0;
	}

	comp_transform->velocity.y -= gravity * dt;
	comp_transform->position.y += comp_transform->velocity.y * dt;
}

short CheckGround(comp_Transform *comp_transform, MapSection *sect) {
	if(comp_transform->velocity.y > 0) {
		CheckCeiling(comp_transform, sect);
		return 0;
	}

	float half_height = BoxExtent(comp_transform->bounds).y * 0.5f;
	float feet_y = BoxCenter(comp_transform->bounds).y - half_height;

	Ray ray = (Ray) { .position = comp_transform->position, .direction = Vector3Scale(UP, -1) };

	Vector3 ground_point = ray.position;
	float ground_dist = FLT_MAX;
	BvhTracePoint(ray, sect, 0, &ground_dist, &ground_point, false);

	if(ground_point.y > feet_y && ground_dist <= half_height) {
		if(comp_transform->velocity.y < 0) {
			float delta = ground_point.y - feet_y;
			comp_transform->position.y += delta;
		}

		return 1;
	}
	
	return 0;
}

short CheckCeiling(comp_Transform *comp_transform, MapSection *sect) {
	float half_height = BoxExtent(comp_transform->bounds).y * 0.5f;
	float head_y = BoxCenter(comp_transform->bounds).y + half_height;

	Ray ray = (Ray) { .position = comp_transform->position, .direction = UP };

	Vector3 celing_point = ray.position;
	float ceiling_dist = FLT_MAX;
	BvhTracePoint(ray, sect, 0, &ceiling_dist, &celing_point, false);

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

