#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "ent.h"
#include "geo.h"
#include "ai.h"
#include "../include/log_message.h"
#include "../include/sort.h"

#define MAX_CLIPS 6
#define SLIDE_STEPS 4

#define STEP_SIZE 8.0f

// *
float ground_diff = 0;
float proj_y;

Model base_ent_models[16] = {0};
void LoadEntityBaseModels() {
	char *prefix = "resources/models";
	base_ent_models[ENT_TURRET] = LoadModel(TextFormat("%s/enemies/turret.glb", prefix));	 
	base_ent_models[ENT_MAINTAINER] = LoadModel(TextFormat("%s/enemies/maintainer.glb", prefix));	 
}

int base_ent_anims_count[16] = {0};
ModelAnimation *base_ent_anims[16] = {0};
void LoadEntityBaseAnims() {
	char *prefix = "resources/models";
	base_ent_anims[ENT_MAINTAINER] = LoadModelAnimations(TextFormat("%s/enemies/maintainer.glb", prefix), &base_ent_anims_count[ENT_MAINTAINER]);	 
}

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
		//BvhTracePointEx(ray, sect, bvh, 0, &tr);
		BvhBoxSweep(ray, sect, &sect->bvh[1], 0, comp_transform->bounds, &tr);

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
					Vector3Subtract(comp_transform->velocity, Vector3Scale(clips[j], (into * 1 + EPSILON) * dt));	
					//Vector3Subtract(comp_transform->velocity, Vector3Scale(comp_transform->velocity, 0.1f * dt));	
			}
		}

		pos = Vector3Add(pos, Vector3Scale(tr.normal, 0.01f));
	}

	comp_transform->position = pos;
	comp_transform->on_ground = CheckGround(comp_transform, pos, sect, &sect->bvh[1], dt);
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
	if(comp_transform->velocity.y >= EPSILON) return 0;

	Vector3 h_vel = (Vector3) { comp_transform->velocity.x, 0, comp_transform->velocity.z };
	Vector3 offset = Vector3Scale(h_vel, dt);

	float ent_height = BoxExtent(comp_transform->bounds).y;
	float feet = (ent_height * 0.5f) - (1 + 0.001f);

	Ray ray = (Ray) { .position = pos, .direction = DOWN };
	ray.position = comp_transform->position;
	//ray.position = Vector3Add(ray.position, offset);

	BvhTraceData tr = TraceDataEmpty();
	BvhBoxSweep(ray, sect, &sect->bvh[1], 0, comp_transform->bounds, &tr);
	//BvhTracePointEx(ray, sect, &sect->bvh[1], 0, &tr);

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
	//comp_transform->position.y = (tr.point.y + ent_height*2) - slope_y;

	comp_transform->last_ground_surface = tr.tri_id; 
	//comp_transform->position.y += change;

	return 1;
}

short CheckCeiling(comp_Transform *comp_transform, MapSection *sect, BvhTree *bvh) {
	if(comp_transform->velocity.y < 0) return 0;

	Ray ray = (Ray) { .position = comp_transform->position, .direction = UP };
	
	BvhTraceData tr = TraceDataEmpty();	
	//BvhBoxSweepNoInvert(ray, sect, bvh, 0, &comp_transform->bounds, &tr);

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
	handler->player_id = 0;

	LoadEntityBaseModels();
	LoadEntityBaseAnims();

	handler->ai_tick = 0;
}

void EntHandlerClose(EntityHandler *handler) {
	if(handler->ents) free(handler->ents);

	for(int i = 0; i < 16; i++) {
		UnloadModel(base_ent_models[i]);
		UnloadModelAnimations(base_ent_anims[i], base_ent_anims[i]->frameCount);
	}
}

// **
// This struct stores IDs of entities to draw
#define MAX_RENDERED_ENTS	128
#define MIN_VIEW_RADIUS		(40.0*40.0)
#define MAX_VIEW_DOT		(-0.107f * DEG2RAD)
typedef struct {
	u16 ids[MAX_RENDERED_ENTS];
	u16 count;	

} RenderList;
RenderList render_list = {0};

void UpdateEntities(EntityHandler *handler, MapSection *sect, float dt) {
	Entity *player_ent = &handler->ents[handler->player_id];
	PlayerUpdate(player_ent, dt);

	render_list.count = 0;
	Vector3 view_dir = player_ent->comp_transform.forward;

	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];

		if(!ent->type)
			continue;

		if(!(ent->flags & ENT_ACTIVE))
			continue;

		switch(ent->type) {
			case ENT_MAINTAINER: {
				MaintainerUpdate(ent, dt);
			} break;
		}

		AiCheckInputs(ent, handler, sect);

		// *** Render visibility checking ***

		Vector3 view_pos = player_ent->comp_transform.position;
		Vector3 to_player = Vector3Subtract(view_pos, ent->comp_transform.position);

		float dist = Vector3LengthSqr(to_player);
		to_player = Vector3Normalize(to_player);

		// Entities that are very close will always be rendered
		if(dist <= MIN_VIEW_RADIUS) {
			render_list.ids[render_list.count++] = i;
			continue;
		}

		// Cull behind camera
		float vis_dot = Vector3DotProduct(to_player, view_dir);
		if(vis_dot > MAX_VIEW_DOT) 
			continue;

		Vector3 right = Vector3CrossProduct(view_dir, UP);

		short visible = 3;
		for(short j = 0; j < 3; j++) {
			short offset = (j & 1) ? -1 : 1;
			if(j == 0) offset = 0;

			Vector3 test_point = Vector3Subtract(ent->comp_transform.position, Vector3Scale(right, 72 * offset));
			if(view_pos.y > ent->comp_transform.position.y) test_point.y = ent->comp_transform.bounds.max.y;

			to_player = Vector3Normalize(Vector3Subtract(view_pos, test_point));
				
			Ray ray = (Ray) { .position = view_pos, .direction = Vector3Negate(to_player) };

			BvhTraceData tr = TraceDataEmpty();
			BvhTracePointEx(ray, sect, &sect->bvh[0], 0, &tr, dist);

			if(Vector3DistanceSqr(ray.position, tr.point) < (dist + (MIN_VIEW_RADIUS*0.25f)))
				visible--;
		}

		if(visible <= 0)
			continue;

		render_list.ids[render_list.count++] = i;
	}

	handler->ai_tick -= dt;
	if(handler->ai_tick <= 0.0f) {
		AiSystemUpdate(handler, sect, dt);
		handler->ai_tick = 0.166f;
	}
}


void RenderEntities(EntityHandler *handler, float dt) {
	for(u16 i = 0; i < render_list.count; i++) {
		Entity *ent = &handler->ents[render_list.ids[i]];

		if(!(ent->flags & ENT_ACTIVE)) 
			continue;

		switch(ent->type) {
			case ENT_TURRET:
				TurretDraw(ent);
				break;

			case ENT_MAINTAINER:
				MaintainerDraw(ent, dt);
				break;
		}
	}
}

void DrawEntsDebugInfo() {
	DrawText(TextFormat("diff: %.2f", ground_diff), 0, 340, 32, DARKPURPLE);
	DrawText(TextFormat("proj: %.2f", proj_y), 256, 340, 32, DARKPURPLE);
}

void ProcessEntity(EntSpawn *spawn_point, EntityHandler *handler, NavGraph *nav_graph) {
	if(!strcmp(spawn_point->tag, "worldspawn")) {
		return;
	}

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

	if(!strcmp(spawn_point->tag, "player_start")) {
		handler->player_start = spawn_point->position;
		handler->player_start.y += BODY_VOLUME_MEDIUM.y * 0.5f;
		return;
	}

	handler->ents[handler->count++] = SpawnEntity(spawn_point, handler);
}

Entity SpawnEntity(EntSpawn *spawn_point, EntityHandler *handler) {
	Entity ent = (Entity) {0};

	ent.comp_transform.position = spawn_point->position;

	ent.comp_transform.forward.x = sinf(-spawn_point->angle * DEG2RAD);
	ent.comp_transform.forward.y = 0;
	ent.comp_transform.forward.z = -cosf(-spawn_point->angle * DEG2RAD);
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
			ent.model = base_ent_models[ENT_TURRET];

			ent.comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  0.5f);
			ent.comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -0.5f);
			ent.comp_transform.bounds = BoxTranslate(ent.comp_transform.bounds, ent.comp_transform.position);
			
			float angle = atan2f(ent.comp_transform.forward.x, ent.comp_transform.forward.z);
			ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateY(angle + 90 * DEG2RAD));

			ent.comp_ai.component_valid = true;
			ent.comp_ai.sight_cone = 0.25f;

		} break;

		case ENT_MAINTAINER: {
			ent.model = base_ent_models[ENT_MAINTAINER];
			ent.animations = base_ent_anims[ENT_MAINTAINER];

			ent.curr_anim = 0;
			//ent.anim_frame = GetRandomValue(0, 200);

			ent.comp_transform.position.y -= 4;

			ent.comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  0.5f);
			ent.comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -0.5f);
			ent.comp_transform.bounds = BoxTranslate(ent.comp_transform.bounds, ent.comp_transform.position);
			
			float angle = atan2f(ent.comp_transform.forward.x, ent.comp_transform.forward.z);
			ent.model.transform = MatrixMultiply(ent.model.transform, MatrixRotateY(angle + 90 * DEG2RAD));

			ent.comp_ai.component_valid = true;
			ent.comp_ai.sight_cone = 0.25f;

		} break;
	}

	ent.flags |= ENT_ACTIVE;

	return ent;
}

void TurretUpdate(Entity *ent, float dt) {
}

void TurretDraw(Entity *ent) {
	//DrawBoundingBox(ent->comp_transform.bounds, PURPLE);
	DrawModel(ent->model, ent->comp_transform.position, 1.0f, LIGHTGRAY);

	/*
	Vector3 center = BoxCenter(ent->comp_transform.bounds);
	center.y += 10;
	Vector3 forward = ent->comp_transform.forward;

	DrawLine3D(center, Vector3Add(center, Vector3Scale(forward, 60)), PURPLE);
	*/
}

void MaintainerUpdate(Entity *ent, float dt) {
	ent->anim_frame = (ent->anim_frame + 1) % ent->animations[ent->curr_anim].frameCount;
}

void MaintainerDraw(Entity *ent, float dt) {
	//float angle = atan2f(ent->comp_transform.forward.x, ent->comp_transform.forward.z);
	//ent->model.transform = MatrixMultiply(ent->model.transform, MatrixRotateY(angle * DEG2RAD));

	//DrawBoundingBox(ent->comp_transform.bounds, PURPLE);
	//DrawModel(ent->model, ent->comp_transform.position, 0.1f, LIGHTGRAY);
	ent->anim_timer -= dt;
	if(ent->anim_timer <= 0) {
		UpdateModelAnimation(ent->model, ent->animations[ent->curr_anim], ent->anim_frame);
		ent->anim_timer = (0.125f);
	}
	//UpdateModelAnimation(ent->model, ent->animations[ent->curr_anim], ent->anim_frame);
	DrawModel(ent->model, ent->comp_transform.position, 0.75f, LIGHTGRAY);

	Vector3 center = BoxCenter(ent->comp_transform.bounds);
	center.y += 10;
	Vector3 forward = ent->comp_transform.forward;

	//DrawLine3D(center, Vector3Add(center, Vector3Scale(forward, 30)), PURPLE);

	if(ent->comp_ai.input_mask & AI_INPUT_SEE_PLAYER) {
		//DrawBoundingBox(ent->comp_transform.bounds, GREEN);
		//DrawLine3D(center, Vector3Add(center, Vector3Scale(forward, 30)), GREEN);
	}
}

void AiComponentUpdate(EntityHandler *handler, comp_Ai *ai, Ai_TaskData *task_data, MapSection *sect, float dt) {
	// Handle interrupts
	for(u32 i = 0; i < 32; i++) {
		u32 mask = (1 << i);
		if(task_data->interrupt_mask & mask) {

		}
	}
}

void AiSystemUpdate(EntityHandler *handler, MapSection *sect, float dt) {
	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];

		if(!ent->comp_ai.component_valid)
			continue;

		comp_Ai *ai = &ent->comp_ai;
		AiComponentUpdate(handler, ai, NULL, sect, dt);
	}
}

// Update senses inputs for an entitie's AI component,
// executed once per frame for every entity with a valid component.
void AiCheckInputs(Entity *ent, EntityHandler *handler, MapSection *sect) {
	comp_Ai *ai = &ent->comp_ai;

	// Do not check inputs on non-valid ai components 
	if(!ai->component_valid) 
		return;

	// Clear inputs
	ai->input_mask = 0;

	comp_Transform *ct = &ent->comp_transform;

	BvhTree *bvh = &sect->bvh[0];

	// ** Check if player is visible **	
	//
	Entity *player_ent = &handler->ents[handler->player_id];
	Vector3 to_player = Vector3Normalize(Vector3Subtract(player_ent->comp_transform.position, ct->position));

	// Player is in ai's sight cone
	if(Vector3DotProduct(ct->forward, to_player) > ai->sight_cone) { 
		// Check for obstructions
		Ray ray = (Ray) { .position = ct->position, .direction = to_player };
		RayCollision player_coll = GetRayCollisionBox(ray, player_ent->comp_transform.bounds);

		// Trace map geometry
		// Small affordance to account for spatial partition structure (+32)
		BvhTraceData tr = TraceDataEmpty();
		BvhTracePointEx(ray, sect, bvh, 0, &tr, player_coll.distance + 32);

		// Player hitbox collision closer than possible surface collision.
		// No obstruction, player is visible 
		if(player_coll.distance < tr.distance)
			ai->input_mask |= AI_INPUT_SEE_PLAYER;
	}
	
	// ***
}

void AiDoSchedule(Entity *ent, comp_Ai *ai, Ai_TaskData *task_data, float dt) {
}

void AiDoState(Entity *ent, comp_Ai *ai, Ai_TaskData *task_data, float dt) {
}

#define BREAK_RADIUS (8.0f*8.0f)
int FindClosestNavNode(Vector3 ent_position, MapSection *sect) {
	int id = -1;

	float closest_dist = FLT_MAX;

	for(u16 i = 0; i < sect->base_navgraph.node_count; i++) {
		NavNode *node = &sect->base_navgraph.nodes[i];

		float dist = Vector3DistanceSqr(node->position, ent_position);	
		if(dist > closest_dist) 
			continue;

		if(!CheckCollisionSpheres(ent_position, 32, node->position, 32))
			continue;

		closest_dist = dist;
		id = node->id;

		if(dist < BREAK_RADIUS)
			break;
	}

	return id;
}

void AiNavSetup(EntityHandler *handler, MapSection *sect) {
	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];	

		comp_Ai *ai = &ent->comp_ai;
		if(!ai->component_valid) continue;

		comp_Transform *ct = &ent->comp_transform;

		for(u16 j = 0; j < sect->navgraph_count; j++) {
			NavGraph *graph = &sect->navgraphs[j];

			int closest_node = FindClosestNavNodeInGraph(ct->position, graph);
			if(closest_node > -1) {
				ai->navgraph_id = j;
				ai->curr_navnode_id = closest_node;

				NavNode *node = &graph->nodes[closest_node];
				ct->position.x = node->position.x;
				ct->position.z = node->position.z;

				break;
			}
		}
	}

	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];	
		comp_Ai *ai = &ent->comp_ai;

		if(ent->type == ENT_MAINTAINER) {
			printf("graph: %d\n", ai->navgraph_id);
			printf("node: %d\n", ai->curr_navnode_id);

			MakeNavPath(ent, &sect->navgraphs[ent->comp_ai.navgraph_id], 6);
		}
	}
}

void MakeNavPath(Entity *ent, NavGraph *graph, u16 target_id) {
	comp_Ai *ai = &ent->comp_ai;
	Ai_TaskData *task_data = &ai->task_data;

	NavPath *path = &task_data->path;

	NavNode *node = &graph->nodes[ai->curr_navnode_id];
	NavNode *targ_node = &graph->nodes[target_id];

	u16 start = node->id;

	bool dest_found = false;
	path->count = 0;

	bool traveled[graph->node_count];
	memset(traveled, false, sizeof(bool) * graph->node_count);

	u16 prev = node->id;

	path->curr = node->id;
	path->count = 0;
	memset(path, node->id, sizeof(u16) * MAX_PATH_NODES);
	
	while(!dest_found) {
		u8 next_count = 0;	
		u16 next_nodes[MAX_EDGES_PER_NODE] = { 0 };
		GetConnectedNodes(node, next_nodes, &next_count, graph);

		if(next_count == 1 && traveled[prev]) {
			/*
			memset(traveled, false, sizeof(bool) * graph->node_count);
			traveled[node->id] = true;
			traveled[prev] = true;

			traveled[start] = false;
			node = &graph->nodes[start];

			prev = node->id;
			
			_path_node_count = 0;
			*/

			u16 back = node->id;
			bool back_found = false;
			while(!back_found) {
				//node = &graph->nodes[_path[_path_node_count--]];
				node = &graph->nodes[path->nodes[path->count--]];

				u8 adj_count = 0;
				u16 adj[MAX_EDGES_PER_NODE] = { node->id };
				GetConnectedNodes(node, adj, &adj_count, graph);

				for(u8 j = 0; j < adj_count; j++) {
					if(!traveled[adj[j]]) {
						back = adj[j];				
						back_found = true;
						break;
					}
				}
			}

			node = &graph->nodes[back];

			continue;
		}

		float costs[next_count];
		memset(costs, FLT_MAX, sizeof(float) * next_count);
		for(u8 i = 0; i < next_count; i++) {
			NavNode *n = &graph->nodes[next_nodes[i]]; 	
			float cost = Vector3DistanceSqr(targ_node->position, n->position);
			costs[i] = cost;
		}

		for(u8 i = 0; i < next_count; i++) {
			for(u8 j = i+1; j < next_count-1; j++) {
				if(costs[i] > costs[j]) {
					u16 temp_id = next_nodes[i];
					next_nodes[i] = next_nodes[j];
					next_nodes[j] = temp_id;

					float temp_cost = costs[i];
					costs[i] = costs[j];
					costs[j] = temp_cost; 
				}
			}
		}

		u16 next_id = next_nodes[0];
		for(u8 i = 0; i < next_count; i++) {
			if(traveled[next_nodes[i]] == true)
				continue;
			
			next_id = next_nodes[i];
			break;
		}

		prev = node->id;

		path->nodes[path->count++] = node->id;
		traveled[prev] = true;

		if(next_id == targ_node->id) {
			path->nodes[path->count++] = next_id;
			dest_found = true;
			break;
		}

		node = &graph->nodes[next_id];
	}

	for(u16 i = 0; i < path->count-1; i++) {
		printf("%d -> %d\n", path->nodes[i], path->nodes[i+1]);
	}
}

