#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "map.h"
#include "ent.h"

void ProcessEntity(EntSpawn *spawn_point, EntityHandler *handler, NavGraph *nav_graph) {
	if(!strcmp(spawn_point->tag, "worldspawn")) {
		return;
	}

	if(nav_graph) {
		if(!strcmp(spawn_point->tag, "nav_node")) {
			if(nav_graph->node_count + 1 >= nav_graph->node_cap) {
				nav_graph->node_cap = nav_graph->node_cap << 1;
				nav_graph->nodes = realloc(nav_graph->nodes, sizeof(NavNode) * nav_graph->node_cap);
			}

			NavNode node = (NavNode) {
				.position = spawn_point->position,
				.id = nav_graph->node_count,
				.edge_count = 0
			};
			memset(node.edges, 0, sizeof(u16) * MAX_EDGES_PER_NODE);
			nav_graph->nodes[nav_graph->node_count] = node;
			nav_graph->node_count++;

			return;
		}
	}

	if(!strcmp(spawn_point->tag, "info_player_start")) {
		puts("player_start");

		handler->player_start = spawn_point->position;
		handler->player_start.z += BODY_VOLUME_MEDIUM.z * 0.5f;

		u16 player_id = handler->count++;
		handler->player_id = player_id;

		u16 bug_id = handler->count++;
		handler->bug_id = bug_id;

		return;
	}

	if(!strcmp(spawn_point->tag, "func_group")) {
		puts("skip func_group");
		return;
	}

	if(!spawn_point->ent_type)
		return;

	handler->ents[handler->count] = SpawnEntity(spawn_point, handler);
}

Entity SpawnEntity(EntSpawn *spawn_point, EntityHandler *handler) {
	Entity ent = (Entity) { .id = handler->count, .cell_id = -1 };

	ent.comp_transform.position = spawn_point->position;

	/*
	ent.comp_transform.forward.x = sinf(-spawn_point->angle * DEG2RAD);
	ent.comp_transform.forward.y = 0;
	ent.comp_transform.forward.z = -cosf(-spawn_point->angle * DEG2RAD);
	ent.comp_transform.forward = Vector3Normalize(ent.comp_transform.forward);
	*/

	ent.comp_transform.start_angle = spawn_point->angle;
	float rad = (-spawn_point->angle) * DEG2RAD;

	ent.comp_transform.forward.x = sinf(rad);
	ent.comp_transform.forward.y = cosf(rad);
	ent.comp_transform.forward.z = 0;
	ent.comp_transform.forward = Vector3Normalize(ent.comp_transform.forward);

	ent.comp_ai = (comp_Ai) {0};
	ent.comp_ai.component_valid = false;

	ent.comp_health = (comp_Health) {0};
	ent.comp_health.amount = 100;

	// * TODO:
	// Entity type specific stuff
	ent.type = spawn_point->ent_type;
	switch(ent.type) {
		case ENT_TURRET: {
			ent.model = handler->base_ent_models[ENT_TURRET];

			//ent.comp_transform.position.y -= 20;
			ent.comp_transform.position.z -= 20;

			ent.comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  1.0f);
			ent.comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -1.0f);
			ent.comp_transform.bounds = BoxTranslate(ent.comp_transform.bounds, ent.comp_transform.position);

			// * NOTE:
			// Modify later as needed
			ent.comp_health.hit_box = ent.comp_transform.bounds;

			float angle = atan2f(ent.comp_transform.forward.z, ent.comp_transform.forward.x);
			//ent.model.transform = MatrixRotateY(-(angle+90)*DEG2RAD);
			//ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateX(90*DEG2RAD));

			ent.model.transform = MatrixRotateX(90*DEG2RAD);
			ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateZ(-spawn_point->angle*DEG2RAD));

			ent.comp_ai.component_valid = true;
			ent.comp_ai.sight_cone = 0.55f;

			ent.comp_ai.curr_schedule = SCHED_SENTRY;
			ent.comp_ai.task_data.task_id = TASK_LOOK_AT_ENTITY;

			ent.comp_transform.targ_look = ent.comp_transform.forward;
			
			ent.comp_weapon = (comp_Weapon) {
				.travel_type = WEAPON_TRAVEL_HITSCAN,
				.id = WEAP_TURRET,
				.cooldown = 1,
			};

			ent.comp_health.amount = 100;
			ent.comp_health.on_hit = 1;

			ent.comp_health.bug_point = BUG_POINT_TURRET;

		} break;

		case ENT_MAINTAINER: {
			ent.model = handler->base_ent_models[ENT_MAINTAINER];
			//ent.model = LoadModelFromMesh(base_ent_models[ENT_MAINTAINER].meshes[0]);
			//ent.animations = handler->base_ent_anims[ENT_MAINTAINER];

			ent.curr_anim = 0;
			//ent.anim_frame = GetRandomValue(0, 200);

			ent.comp_transform.position.z += 20;

			ent.comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  0.5f);
			ent.comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -0.5f);
			ent.comp_transform.bounds = BoxTranslate(ent.comp_transform.bounds, ent.comp_transform.position);
			
			//float angle = atan2f(ent.comp_transform.forward.x, ent.comp_transform.forward.z);
			float angle = (spawn_point->angle-90) * DEG2RAD;
			//ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateY(angle+90*DEG2RAD));
			ent.model.transform = MatrixRotateX(90*DEG2RAD);
			ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateZ(angle));

			ent.comp_ai.component_valid = true;
			ent.comp_ai.sight_cone = 0.25f;
			//ent.comp_ai.curr_schedule = SCHED_CHASE_PLAYER;
			//ent.comp_ai.curr_schedule = SCHED_PATROL;
			//ent.comp_ai.task_data.task_id = TASK_MAKE_PATROL_PATH;
			//ent.comp_ai.task_data.task_id = TASK_WAIT_TIME;
			//ent.comp_ai.task_data.timer = 0.1f;

			//ent.comp_ai.curr_schedule = SCHED_MAINTAINER_ATTACK;
			ent.comp_ai.curr_schedule = SCHED_IDLE;
			ent.comp_ai.task_data.task_id = TASK_WAIT_TIME;

			ent.comp_health.amount = 3;
			ent.comp_health.on_hit = 2;

			ent.comp_health.bug_point = BUG_POINT_MAINTAINER;
		
			// * NOTE:
			// Modify later as needed
			ent.comp_health.hit_box = ent.comp_transform.bounds;
			
			//ent.comp_ai.wish_dir = (Vector3) { -1, 0, 0 };

		} break;

		case ENT_REGULATOR: {

		} break;
	}

	ent.comp_health.bug_box = (BoundingBox) {
		.min = Vector3Scale(BODY_VOLUME_SMALL, -0.75f),	
		.max = Vector3Scale(BODY_VOLUME_SMALL,  0.75f)
	};

	ent.comp_transform.start_forward = ent.comp_transform.forward;
	ent.flags = (ENT_ACTIVE | ENT_COLLIDERS);

	ent.comp_ai.navgraph_id = -1;
	ent.comp_ai.speed = 50;
	ent.comp_ai.wish_dir = Vector3Zero();

	ent.cell_id = -1;

	handler->count++;

	return ent;
}

