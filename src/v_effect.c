#include "raylib.h"
#include "v_effect.h"
#include "../include/num_redefs.h"

void vEffectsInit(vEffect_Manager *manager) {
	*manager = (vEffect_Manager) {0};
}

void vEffectsRun(vEffect_Manager *manager, float dt) {
	for(u8 i = 0; i < V_EFFECT_MAX_TRAILS; i++) {
		vEffect_Trail *trail = &manager->trails[i];

		if(!trail->active) 
			continue;

		trail->timer -= dt*1.5f;
		if(trail->timer <= 0) {
			trail->active = false;
			manager->trail_count--;
			continue;
		}

		DrawCylinderEx(trail->point_A, trail->point_B, 0.75f, 0.75f, 4, ColorAlpha(RAYWHITE, 0.99f * trail->timer));
	}

	for(u8 i = 0; i < manager->impact_decal_count; i++) {
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

	manager->trails[id] = new_trail;
	manager->trail_count++;
}

