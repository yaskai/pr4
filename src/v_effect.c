#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "v_effect.h"
#include "../include/num_redefs.h"
#include "geo.h"

void vEffectsInit(vEffect_Manager *manager) {
	*manager = (vEffect_Manager) {0};

	manager->trail_model = LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, 5));
	manager->trail_model.transform = MatrixIdentity();

	manager->trail_material = LoadMaterialDefault();
}

void vEffectsRun(vEffect_Manager *manager, float dt) {
	for(u8 i = 0; i < V_EFFECT_MAX_TRAILS; i++) {
		vEffect_Trail *trail = &manager->trails[i];

		if(!trail->active) 
			continue;

		trail->timer -= dt*1.75f;
		if(trail->timer <= 0) {
			trail->active = false;
			manager->trail_count--;
			continue;
		}

		float alpha = trail->timer;
		alpha = Clamp(alpha, 0.0f, 0.5f);

		trail->point_A.y += trail->timer * dt;
		trail->point_B.y += trail->timer * dt;

		Vector3 axis = Vector3Zero();
		float angle = 0;
		QuaternionToAxisAngle(trail->q, &axis, &angle);
	
		Vector3 scale = (Vector3) { 0.7f, trail->length, 0.7f };

		DrawModelEx(manager->trail_model, trail->point_A, axis, angle*RAD2DEG, scale, ColorAlpha(RAYWHITE, alpha));
	}

	for(u8 i = 0; i < V_EFFECT_MAX_IMPACT_DECALS; i++) {
		vEffect_ImpactDecal *impact_decal = &manager->impact_decals[i];
		
		if(!impact_decal->active)
			continue;
	}
}

void vEffectsAddTrail(vEffect_Manager *manager, Vector3 start, Vector3 end) {
	u8 id = 0;
	for(u8 i = 0; i < V_EFFECT_MAX_TRAILS; i++) {
		vEffect_Trail *trail = &manager->trails[i];

		if(!trail->active) { 
			id = i;
			break;
		}
	}

	vEffect_Trail new_trail = (vEffect_Trail) {
		.point_A = start,
		.point_B = end,
		.timer = 1,
		//.timer = 10,
		.active = true
	}; 	

	Vector3 d = Vector3Subtract(end, start);
	float length = Vector3Length(d);	
	new_trail.length = length;
	Vector3 dir = Vector3Normalize(d);
	new_trail.q = QuaternionFromVector3ToVector3(UP, dir);

	manager->trails[id] = new_trail;
	manager->trail_count++;
}

