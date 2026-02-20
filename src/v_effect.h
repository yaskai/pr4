#include "raylib.h"
#include "../include/num_redefs.h"

#ifndef V_EFFECT_H_
#define V_EFFECT_H_

#define V_EFFECT_MAX_TRAILS	32
typedef struct {
	Vector3 point_A;
	Vector3 point_B;

	float timer;

	bool active;

} vEffect_Trail;

#define V_EFFECT_MAX_IMPACT_DECALS 64
typedef struct {
	Vector3 position;
	Vector3 normal;

	u8 texture_id;

	bool active;

} vEffect_ImpactDecal;

typedef struct {
	vEffect_Trail trails[V_EFFECT_MAX_TRAILS];
	vEffect_ImpactDecal impact_decals[V_EFFECT_MAX_IMPACT_DECALS];

	u8 trail_count;
	u8 impact_decal_count;

} vEffect_Manager;

void vEffectsInit(vEffect_Manager *manager);
void vEffectsRun(vEffect_Manager *manager, float dt);

void vEffectsAddTrail(vEffect_Manager *manager, Vector3 start, Vector3 end);

#endif // !V_EFFECT_H_
