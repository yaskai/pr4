#include <stdio.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "pm.h"

u8 bug_CheckGround(comp_Transform *ct, Vector3 position, MapSection *sect) {
	Ray ray = (Ray) { .position = ct->position, .direction = DOWN };	

	BvhTraceData tr = TraceDataEmpty();	
	BvhTracePointEx(ray, sect, &sect->bvh[2], 0, &tr, 1 + EPSILON);
	
	if(!tr.hit) {
		ct->ground_normal = Vector3Zero();
		return 0;
	}

	ct->ground_normal = tr.normal;
	pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.00001f, 0);
	if(fabsf(ct->velocity.y) < STOP_EPS) ct->velocity.y = 0;

	return 1;
}

void bug_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt, MapSection *sect) {
	*pm = (pmTraceData) { .start_in_solid = -1, .end_in_solid = -1, .origin = start, .block = 0 };

	// Check if inside solid before starting trace
	Ray start_ray = (Ray) { .position = start, .direction = Vector3Normalize(wish_vel) };
	BvhTraceData start_tr = TraceDataEmpty();
	
	float trace_max_dist = Vector3LengthSqr(wish_vel);
	trace_max_dist = Clamp(trace_max_dist, 0.33f, 2000.0f);

	BvhTracePointEx(start_ray, sect, &sect->bvh[BVH_BOX_SMALL], 0, &start_tr, trace_max_dist);
	if(start_tr.hit) {
		pm->start_in_solid = start_tr.hull_id;
	}

	Vector3 dest = start;
	Vector3 vel = wish_vel;

	pm->start_vel = wish_vel;

	float t_remain = dt;

	// Tracked clip planes
	Vector3 clips[MAX_CLIPS] = {0};
	u8 num_clips = 0;

	for(short i = 0; i < MAX_BUMPS; i++) {
		// End slide trace if velocity too low
		if(Vector3LengthSqr(vel) <= STOP_EPS)
			break;
		
		// Scale slide movement by time remaining
		Vector3 move = Vector3Scale(vel, t_remain);

		// Upate ray
		Ray ray = (Ray) { .position = dest, .direction = Vector3Normalize(move) };

		// Trace geometry 
		BvhTraceData tr = TraceDataEmpty();
		BvhTracePointEx(ray, sect, &sect->bvh[BVH_BOX_SMALL], 0, &tr, FLT_MAX);

		// Determine how much of movement was obstructed
		float fraction = (tr.distance / Vector3Length(move));
		fraction = Clamp(fraction, 0.0f, 1.0f);

		// Update destination
		dest = Vector3Add(dest, Vector3Scale(move, fraction));

		if(fraction < 1.0f)
			pm->end_in_solid = (tr.hit) ? pm_CheckHull(dest, tr.hull_id) : -1;

		// No obstruction, do full movement 
		if(fraction >= 1.0f) 
			break;

		// Add clip plane
		if(num_clips + 1 < MAX_CLIPS) {
			clips[num_clips++] = tr.normal;

			// Update velocity by each clip plane
			for(short j = 0; j < num_clips; j++) {
				float into = Vector3DotProduct(vel, clips[j]);

				if(into < 0) 
					pm_ClipVelocity(vel, clips[j], &vel, 1.0005f, pm->block);
			}

		} else 
			break;

		// Add small offset to prevent tunneling through surfaces
		dest = Vector3Add(dest, Vector3Scale(tr.normal, 0.01f));

		// Update remaining time
		t_remain *= (1 - fraction);
	}

	pm->move_dist = Vector3Distance(start, dest);
	pm->end_vel = vel;
	pm->end_pos = dest;
}

void BugInit(Entity *ent, EntityHandler *handler, MapSection *sect) {
	ent->model = LoadModel("resources/models/weapons/bug_00.glb");

	ent->comp_transform.bounds = (BoundingBox) {
		.min = (Vector3) { -4, -4, -4 },
		.max = (Vector3) {  4,  4,  4 }
	};

	ent->comp_ai.state = 0;
}

void BugUpdate(Entity *ent, EntityHandler *handler, MapSection *sect, float dt) {
	Entity *player_ent = &handler->ents[handler->player_id];

	comp_Transform *ct = &ent->comp_transform;
	comp_Ai *ai = &ent->comp_ai;

	if(ai->state == BUG_DEFAULT) {
		ct->position = player_ent->comp_transform.position;
		ct->velocity = Vector3Zero();
	}

	ct->bounds = BoxTranslate(ct->bounds, ct->position);

	if(ai->state == BUG_LAUNCHED) {
		ct->on_ground = bug_CheckGround(ct, ct->position, sect);
		if(!ct->on_ground) {
			ct->velocity.y -= 800.0f * dt;
		}

		pmTraceData pm = (pmTraceData) {0};
		pm_AirFriction(ct, dt);

		bug_TraceMove(ct, ct->position, ct->velocity, &pm, dt, sect);
		ent->comp_transform.velocity = pm.end_vel;
		ent->comp_transform.position = pm.end_pos;

		if(ct->on_ground) {
			ai->state = BUG_LANDED;
			ct->velocity = Vector3Zero();
		}
	}

	if(ai->state == BUG_LANDED) {
		ct->velocity = Vector3Zero();
		
		// Check if there is an enemy to disrupt
		if(!(ent->flags & BUG_DISRUPTED_ENEMY)) {
			EntGrid *grid = &handler->grid;
			Coords coords = Vec3ToCoords(ct->position, grid);
			i16 cell_id = CellCoordsToId(coords, grid);
			EntGridCell *cell = &grid->cells[cell_id];

			//printf("disrupt prox check\n");
			//printf("cell: %d\n", cell_id);
			//printf("ent count: %d\n", cell->ent_count);

			for(u8 i = 0; i < cell->ent_count; i++) {
				Entity *enemy_ent = &handler->ents[cell->ents[i]];

				if(enemy_ent->type == ENT_PLAYER)
					continue;

				if(enemy_ent->type == ENT_DISRUPTOR)
					continue;

				if(!enemy_ent->comp_ai.component_valid)
					continue;

				//printf("ent[%d] id: %d\n", i, cell->ents[i]);

				if(!CheckCollisionBoxes(ct->bounds, enemy_ent->comp_transform.bounds))
					continue;

				printf("hit!\n");

				DisruptEntity(handler, enemy_ent->id);	
			}

			ent->flags |= BUG_DISRUPTED_ENEMY;
		}
		
		// Pickup
		if(CheckCollisionBoxes(ct->bounds, player_ent->comp_transform.bounds)) {
			ai->state = BUG_DEFAULT;
		}
	}
}

void BugDraw(Entity *ent) {
	if(ent->comp_ai.state == 0)
		return;

	DrawModel(ent->model, ent->comp_transform.position, 3, WHITE);	
	//DrawBoundingBox(ent->comp_transform.bounds, GREEN);
}

void DisruptEntity(EntityHandler *handler, u16 ent_id) {
	//printf("dirsrupted entity [%d]\n", ent_id);
	Entity *ent = &handler->ents[ent_id];
	comp_Ai *ai = &ent->comp_ai;	

	ai->input_mask |= AI_INPUT_SELF_GLITCHED;

	AlertMaintainers(handler, ent_id);
}

