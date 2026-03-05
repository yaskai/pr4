#include <stdio.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "pm.h"
#include "kbsp.h"

#define BUG_MAX_BOUNCES 10
#define BUG_MAX_VEL 500.0f
#define BUG_GRAV	970.0f

u8 bug_bounce = 0;
float launch_timer = 0;
bool big_bounce_used = false;

Model model_dead;

float bug_cooldown = 0;

bool bug_target_picked = false;

float bug_z_vel_prev = 0;

void BugBounce(Entity *bug_ent, comp_Transform *ct, MapSection *sect, EntityHandler *handler, u8 *bounce, float dt) {
	EntGrid *grid = &handler->grid;

	Coords coords = Vec3ToCoords(ct->position, grid);

	if(!bug_target_picked) {
		Coords cell_coords[] = {
			coords,
			(Coords) { coords.c - 1, coords.r - 1, coords.t, },
			(Coords) { coords.c + 0, coords.r - 1, coords.t, },
			(Coords) { coords.c + 1, coords.r - 1, coords.t, },
			(Coords) { coords.c - 1, coords.r + 0, coords.t, },
			(Coords) { coords.c + 1, coords.r + 0, coords.t, },
			(Coords) { coords.c - 1, coords.r + 1, coords.t, },
			(Coords) { coords.c + 0, coords.r + 1, coords.t, },
			(Coords) { coords.c + 1, coords.r + 1, coords.t, },
		};
		short adj_count = sizeof(cell_coords) / sizeof(cell_coords[0]);
		
		float closest = FLT_MAX;
		i16 enemy_id = -1;

		for(u8 j = 0; j < adj_count; j++) {
			if(!CoordsInBounds(cell_coords[j], grid))
				continue;

			i16 cell_id = CellCoordsToId(cell_coords[j], grid);
			EntGridCell *cell = &grid->cells[cell_id];

			for(u8 i = 0; i < cell->ent_count; i++) {
				Entity *enemy_ent = &handler->ents[cell->ents[i]];
				comp_Ai *enemy_ai = &enemy_ent->comp_ai;

				if(enemy_ent->type == ENT_DISRUPTOR)
					continue;

				if(enemy_ent->type == ENT_PLAYER)
					continue;

				if(enemy_ai->state == STATE_DEAD)
					continue;
				
				if(!enemy_ai->component_valid)
					continue;

				if(enemy_ai->input_mask & AI_INPUT_SELF_GLITCHED)
					continue;

				Vector3 to_enemy = Vector3Subtract(
					Vector3Add(
						Vector3Add(enemy_ent->comp_transform.position, enemy_ent->comp_health.bug_point),
						Vector3Scale(enemy_ent->comp_transform.velocity, 1)),
					ct->position
				);	

				float dist = Vector3Length(to_enemy);

				if(dist > 250.0f)
					continue;

				if(dist < closest) {
					closest = dist;
					enemy_id = enemy_ent->id;
					bug_target_picked = true;
				}
			}
		}

		bug_ent->comp_ai.task_data.target_entity = enemy_id;
	} 

	(*bounce)++;

	Vector3 hdir = (Vector3) { ct->velocity.x, ct->velocity.y, 0 };
	hdir = Vector3Normalize(hdir);
	ct->forward = hdir;	

	float angle = GetRandomValue(-70, 70);
	ct->forward = Vector3RotateByAxisAngle(ct->forward, UP, angle*DEG2RAD);

	if(!bug_target_picked) 
		return;

	Entity *enemy_ent = &handler->ents[bug_ent->comp_ai.task_data.target_entity];
	if(enemy_ent->comp_ai.state == STATE_DEAD) {
		bug_target_picked = false;
		bug_ent->comp_ai.task_data.target_entity = -1;
		*bounce = 0;
		*bounce = 0;
	}

	/*
	Vector3 to_enemy = Vector3Subtract(enemy_ent->comp_transform.position, ct->position);	
	Vector3 to_bug = Vector3Subtract(ct->position, enemy_ent->comp_transform.position);	
	float ndot = Vector3DotProduct(Vector3Normalize(enemy_ent->comp_transform.velocity), to_bug);
	*/
	
	/*
	if(ndot < 0) {
		to_enemy = Vector3Subtract(
			Vector3Add(
				Vector3Add(enemy_ent->comp_transform.position, enemy_ent->comp_health.bug_point),
				Vector3Scale(enemy_ent->comp_transform.velocity, 1)),
			ct->position
		);	
	}
	*/

	Vector3 to_enemy = Vector3Subtract( enemy_ent->comp_transform.position, ct->position );	
	float d = Vector3Length(to_enemy);
	to_enemy.z = 0;
	to_enemy = Vector3Normalize(to_enemy);

	if(d > 120 && (fabsf(enemy_ent->comp_transform.position.z - ct->position.z) <= 48)) {
		ct->velocity.x = to_enemy.x * d * (1.2f + (GetRandomValue(0, 5) * 0.1f));	
		ct->velocity.y = to_enemy.y * d * (1.2f + (GetRandomValue(0, 5) * 0.1f));	
	} else {
		ct->velocity.x = to_enemy.x * d;	
		ct->velocity.y = to_enemy.y * d;	
	}

	ct->velocity.z += (d*0.05f);

	if(d <= 265.0f) {
		if(enemy_ent->id != handler->player_id) {
			ct->velocity.z += 300 + (1.5f*(*bounce));
			if(*bounce >= BUG_MAX_BOUNCES && !big_bounce_used) {
				(*bounce)--;
				big_bounce_used = true;
			}
		} else {
			ct->velocity.z += 150 + (0.1f*(*bounce));
		}
		
		/*
		if(fabsf(enemy_ent->comp_transform.position.z - ct->position.z) <= 16) {
			ct->position.z = enemy_ent->comp_transform.position.z;
		}
		*/
	}

	/*
	if(ct->velocity.z > 350)
		ct->velocity.z = 350;
	*/
}

u8 bug_CheckGround(Entity *ent, comp_Transform *ct, Vector3 position, MapSection *sect, u8 *bounce, EntityHandler *handler, float dt) {
	Ray ray = (Ray) { .position = ct->position, .direction = DOWN };	

	BvhTraceData tr = TraceDataEmpty();	
	BvhTracePointEx(ray, sect, &sect->bvh[2], 0, &tr, 1 + EPSILON);
	
	if(!tr.hit) {
		ct->ground_normal = Vector3Zero();
		return 0;
	}

	/*
	Vector3 feet_pos = Vector3Add(ct->position, Vector3Scale(UP, 17));
	//Vector3 down_pos = Vector3Add(feet_pos, Vector3Scale(DOWN, 1));
	Bsp_TraceData tr = Bsp_TraceDataEmpty();
	Bsp_RecursiveTraceEx(&sect->bsp[0], sect->bsp[0].first_node, 0, 1, feet_pos, Vector3Add(feet_pos, Vector3Scale(DOWN, 0.1f)), &tr);

	if(tr.plane.normal[2] < 0.7f) {
		ct->ground_normal = Vector3Zero();
		return 0;
	}
	*/

	if(tr.fraction >= 1.0f) {
		ct->ground_normal = Vector3Zero();
		return 0;
	}

	if(handler->ents[handler->player_id].comp_transform.position.z - ct->position.z > 700 && launch_timer <= 0) {
		ent->comp_health.amount = 0;
		ent->comp_ai.state = STATE_DEAD;
		bug_cooldown = 5;
		return 0;
	}

	ct->ground_normal = tr.normal;
	//ct->ground_normal = (Vector3) { tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2] };
	
	//pm_ClipVelocity(ct->velocity, ct->ground_normal, &ct->velocity, 1.50001f, 0);
	//if(fabsf(ct->velocity.y) <= 2.0f) {
	if(*bounce >= BUG_MAX_BOUNCES) {
		ct->velocity.z = 0;
		return 1;
	}
	(*bounce)++;

	BugBounce(ent, ct, sect, handler, bounce, dt);

	return 0;
}

void bug_TraceMove(comp_Transform *ct, Vector3 start, Vector3 wish_vel, pmTraceData *pm, float dt, MapSection *sect, EntityHandler *handler) {
	*pm = (pmTraceData) { .start_in_solid = -1, .end_in_solid = -1, .origin = start, .block = 0 };

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
		//float fraction = bsp_tr.fraction;
		fraction = Clamp(fraction, 0.0f, 1.0f);

		EntTraceData ent_tr = { .dist = Vector3Length(move), .hit_ent = -1, .point = dest, .normal = Vector3Zero() };
		Vector3 ent_point = TraceEntities(ray, handler, Vector3Length(move), handler->bug_id, &ent_tr);

		float ent_frac = 1.0f ;
		bool use_ent = (ent_tr.hit_ent > -1 && ent_tr.hit_ent < handler->count);
		if(use_ent) {
			ent_frac = (ent_tr.dist / Vector3Length(move));
			ent_frac = Clamp(ent_frac, 0.0f, 1.0f);
			fraction = ent_frac;
		}

		pm->fraction = fraction;

		// Update destination
		dest = Vector3Add(dest, Vector3Scale(move, fraction));

		// No obstruction, do full movement 
		if(fraction >= 1.0f) 
			break;

		if(use_ent) {
			fraction -= 0.01f;
			if(fraction < 0) fraction = 0;
		}

		// Add clip plane
		if(num_clips + 1 < MAX_CLIPS) {
			//clips[num_clips++] = tr.normal;
			//clips[num_clips++] = (Vector3) { bsp_tr.plane.normal[0], bsp_tr.plane.normal[1], bsp_tr.plane.normal[2] };
			clips[num_clips++] = (use_ent) ? ent_tr.normal : tr.normal; 

			// Update velocity by each clip plane
			for(short j = 0; j < num_clips; j++) {
				float into = Vector3DotProduct(vel, clips[j]);
				float clip_bounce = (use_ent && j == num_clips - 1) ? 1.8f : 1.5005f;

				if(into < 0) 
					pm_ClipVelocity(vel, clips[j], &vel, clip_bounce, pm->block);
			}

		} else 
			break;

		// Update remaining time
		t_remain *= (1 - fraction);
	}

	pm->move_dist = Vector3Distance(start, dest);
	pm->end_vel = vel;
	pm->end_pos = dest;
}

void BugInit(Entity *ent, EntityHandler *handler, MapSection *sect) {
	ent->model = LoadModel("resources/models/weapons/bug_00.glb");
	model_dead = LoadModel("resources/models/weapons/bug_dead_00.glb");

	//ent->model.transform = MatrixRotateZ(90*DEG2RAD);
	//model_dead.transform = MatrixRotateZ(90*DEG2RAD);

	ent->comp_transform.bounds = (BoundingBox) {
		.min = (Vector3) { -4, -4, -4 },
		.max = (Vector3) {  4,  4,  4 }
	};

	ent->comp_ai.component_valid = false;
	ent->comp_ai.task_data.target_entity = -1;
	ent->comp_ai.state = 0;
}

void BugUpdate(Entity *ent, EntityHandler *handler, MapSection *sect, float dt) {
	Entity *player_ent = &handler->ents[handler->player_id];

	comp_Transform *ct = &ent->comp_transform;
	comp_Ai *ai = &ent->comp_ai;

	bug_z_vel_prev = ct->velocity.z;

	//ent->cell_id = CellCoordsToId(Vec3ToCoords(ct->position, &handler->grid), &handler->grid);

	if(ai->state == BUG_DEFAULT) {
		ct->position = player_ent->comp_transform.position;
		ct->velocity = Vector3Zero();

		ent->flags &= ~ENT_COLLIDERS;	
		ent->flags &= ~BUG_DISRUPTED_ENEMY; 

		ct->ground_normal = Vector3Zero();
		bug_bounce = 0;
		big_bounce_used = false;
		launch_timer = 0.5f;

		bug_cooldown = 10;

		ent->comp_health.amount = 5;

		bug_target_picked = false;
		ent->comp_ai.task_data.target_entity = -1;
	}

	ct->bounds = BoxTranslate(ct->bounds, ct->position);

	if(ai->state == BUG_LAUNCHED) {
		launch_timer -= dt;

		ct->on_ground = bug_CheckGround(ent, ct, ct->position, sect, &bug_bounce, handler, dt);
		// Apply gravity
		if(!ct->on_ground) 
			ct->velocity.z -= BUG_GRAV * dt;

		pmTraceData pm = (pmTraceData) {0};
		//pm_AirFriction(ct, dt);
	
		Vector3 prev_pos = ct->position;
		bug_TraceMove(ct, ct->position, ct->velocity, &pm, dt, sect, handler);
		ct->velocity = pm.end_vel;
		ct->position = pm.end_pos;
		
		ct->velocity.z = Clamp(ct->velocity.z, -BUG_MAX_VEL, BUG_MAX_VEL);
		//ct->velocity = Vector3ClampValue(ct->velocity, -BUG_MAX_VEL, BUG_MAX_VEL);

		EntGrid *grid = &handler->grid;
		Coords coords = Vec3ToCoords(ct->position, grid);
		i16 cell_id = CellCoordsToId(coords, grid);
		EntGridCell *cell = &grid->cells[cell_id];
		for(u8 i = 0; i < cell->ent_count; i++) {
			Entity *enemy_ent = &handler->ents[cell->ents[i]];

			if(enemy_ent->type == ENT_PLAYER)
				continue;

			if(enemy_ent->type == ENT_DISRUPTOR)
				continue;

			if(enemy_ent->comp_ai.input_mask & AI_INPUT_SELF_GLITCHED)
				continue;

			if(enemy_ent->comp_ai.state == STATE_DEAD)
				continue;

			if(CheckCollisionBoxes(ct->bounds, enemy_ent->comp_transform.bounds) && ct->position.z >= enemy_ent->comp_transform.position.z) {
				ct->on_ground = true;
				ct->position = BoxCenter(enemy_ent->comp_health.bug_box);
				ai->task_data.target_entity = enemy_ent->id;
				ct->forward = enemy_ent->comp_transform.forward;
				ent->comp_health.damage_cooldown = 10;
				ct->velocity = Vector3Zero();

				break;
			}
		}

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

			for(u8 i = 0; i < cell->ent_count; i++) {
				Entity *enemy_ent = &handler->ents[cell->ents[i]];

				if(enemy_ent->type == ENT_PLAYER)
					continue;

				if(enemy_ent->type == ENT_DISRUPTOR)
					continue;

				if(!enemy_ent->comp_ai.component_valid)
					continue;

				if(!CheckCollisionBoxes(ct->bounds, enemy_ent->comp_transform.bounds))
					continue;

				DisruptEntity(handler, enemy_ent->id, sect);	
				ent->flags |= BUG_DISRUPTED_ENEMY;
			}
		}

		if((ent->flags & BUG_DISRUPTED_ENEMY) && ai->task_data.target_entity > -1 && ai->task_data.target_entity < handler->count) {
			Entity *stick_ent = &handler->ents[ai->task_data.target_entity];			
			ct->position = Vector3Add(stick_ent->comp_transform.position, stick_ent->comp_health.bug_point);

			if(stick_ent->comp_ai.state == STATE_DEAD) {
				ai->state = BUG_LAUNCHED;
				ai->task_data.target_entity = -1;
				

				ent->flags &= ~BUG_DISRUPTED_ENEMY;
				bug_bounce = 0;
				bug_target_picked = false;

				BugBounce(ent, ct, sect, handler, &bug_bounce, dt);

				if(ai->task_data.target_entity == -1) {
					/*
					ct->velocity.x += GetRandomValue(-200, 200);
					ct->velocity.y += GetRandomValue(-200, 200);
					ct->velocity.z += 400;
					*/

					ai->task_data.target_entity = handler->player_id;
					bug_target_picked = true;
					BugBounce(ent, ct, sect, handler, &bug_bounce, dt);
				}
			}
		}

		launch_timer -= dt;
	}

	// Pickup
	if(CheckCollisionBoxes(ct->bounds, player_ent->comp_transform.bounds) && launch_timer <= 0) {
		ai->state = BUG_DEFAULT;

	}

	if(ai->state == STATE_DEAD) {
		// Pickup dead
		if(CheckCollisionBoxes(ct->bounds, player_ent->comp_transform.bounds)) {
			ai->state = BUG_DEFAULT;
		}

		bug_cooldown -= dt;
		if(bug_cooldown <= 0) 
			ai->state = BUG_DEFAULT;
	}
}

void BugDraw(Entity *ent) {
	if(ent->comp_ai.state == 0)
		return;

	if(launch_timer >= 0.4725f)
		return;

	Vector3 pos = ent->comp_transform.position;
	//pos.z -= 16;

	float angle = atan2f(-ent->comp_transform.forward.x, ent->comp_transform.forward.y);
	ent->model.transform = MatrixRotateY(angle);
	ent->model.transform = MatrixMultiply(ent->model.transform, MatrixRotateX(90*DEG2RAD));

	if(ent->comp_ai.state == STATE_DEAD) {
		model_dead.transform = ent->model.transform;
		DrawModel(model_dead, pos, 3, WHITE);	
 	} else {
		DrawModel(ent->model, pos, 3, WHITE);	
	}

	//DrawBoundingBox(ent->comp_transform.bounds, GREEN);
}

void DisruptEntity(EntityHandler *handler, u16 ent_id, MapSection *sect) {
	//printf("dirsrupted entity [%d]\n", ent_id);
	Entity *ent = &handler->ents[ent_id];
	comp_Ai *ai = &ent->comp_ai;	
	comp_Transform *ct = &ent->comp_transform;

	if(ai->input_mask & AI_INPUT_SELF_GLITCHED)
		return;

	ai->input_mask |= AI_INPUT_SELF_GLITCHED;

	// * NOTE: 
	// Magic number, change later based on entity type maybe??
	switch(ent->type) {
		case ENT_TURRET: {
			ai->disrupt_timer = 100;
			ai->task_data.timer = 0;
			ent->comp_weapon.ammo = 60;
			ent->comp_weapon.cooldown = 0;
			ai->task_data.task_id = TASK_FIRE_WEAPON;

			ct->forward = ct->start_forward;
			ai->task_data.known_target_position = Vector3Add(ct->position, ct->forward);
			ai->task_data.target_position = Vector3Add(ct->position, ct->forward);

		} break;
	}

	//AiDoSchedule(ent, handler, sect, ai, &ai->task_data, 1);

	AlertMaintainers(handler, ent_id);
}

void OnHitBug(Entity *ent, short damage) {
}

