#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "game.h"
#include "input_handler.h"
#include "geo.h"
#include "../include/log_message.h"
#include "map.h"

void VirtCameraControls(Camera3D *cam, float dt, Vector3 target_point);

#define VIRT_W (1920)
#define VIRT_H (1080)

float *plr_accel;

Color colors[] = {
	PINK,
	BLUE,
	GREEN,
	SKYBLUE,
	PURPLE,
	ORANGE,
	RED
};

PlayerDebugData player_data = {0};

Material mat_default;

BrushPool brush_pool = (BrushPool) {0};
BrushPool brush_pool_exp = (BrushPool) {0};

u16 tri_count = 0;
Tri *tris;

Model sphere_model;

void GameInit(Game *game, Config *conf) {
	game->conf = conf;	

	InputInit(&game->input_handler);

	SetLogState(1);
}

void GameClose(Game *game) {
	// Unload render textures
	if(IsTextureValid(game->render_target3D.texture))
		UnloadRenderTexture(game->render_target3D);

	if(IsTextureValid(game->render_target2D.texture))
		UnloadRenderTexture(game->render_target2D);
}

void GameRenderSetup(Game *game) {
	// Initalize 3D camera
	game->camera = (Camera3D) {
		.position = (Vector3) { 30, 30, 30 },
		.target = (Vector3) { 0, 0, 0 },
		.up = (Vector3) {0, 1, 0},
		.fovy = 90,
		.projection = CAMERA_PERSPECTIVE
	};

	game->camera_debug = (Camera3D) {
		.position = (Vector3) { -1500, 1000, -1500 },
		.target = (Vector3) { 0, 0, 0 },
		.up = (Vector3) {0, 1, 0},
		.fovy = 90,
		.projection = CAMERA_PERSPECTIVE
	};

	// Load render textures
	//game->render_target3D = LoadRenderTexture(game->conf->window_width, game->conf->window_height);
	//game->render_target2D = LoadRenderTexture(game->conf->window_width, game->conf->window_height);
	//game->render_target_debug = LoadRenderTexture(game->conf->window_width * 0.5f, game->conf->window_height * 0.5f);

	game->render_target3D = LoadRenderTexture(VIRT_W, VIRT_H);
	game->render_target2D = LoadRenderTexture(VIRT_W, VIRT_H);
	//game->render_target_debug = LoadRenderTexture(game->conf->window_width * 0.5f, game->conf->window_height * 0.5f);
	game->render_target_debug = LoadRenderTexture(VIRT_W, VIRT_H);
	SetTextureFilter(game->render_target_debug.texture, TEXTURE_FILTER_TRILINEAR);

	EntHandlerInit(&game->ent_handler);

	mat_default = LoadMaterialDefault();
	mat_default.maps[MATERIAL_MAP_DIFFUSE].color = ColorAlpha(BLUE, 0.25f);

	sphere_model = LoadModelFromMesh(GenMeshSphere(2, 16, 8));
}

void GameLoadTestScene1(Game *game, char *path) {
	SpawnList spawn_list = (SpawnList) {0}; 
	game->test_section = BuildMapSect(path, &spawn_list);

	game->test_section.navgraphs = malloc(sizeof(NavGraph) * 16);
	
	/*
	game->test_section.navgraphs[0] = (NavGraph) {
		.node_count = 0,
		.node_cap = 128,
		.edge_count = 0,
		.edge_cap = 256
	};
	game->test_section.navgraphs[0].nodes = calloc(game->test_section.navgraphs[0].node_cap, sizeof(NavNode));
	game->test_section.navgraphs[0].edges = calloc(game->test_section.navgraphs[0].edge_cap, sizeof(NavEdge));
	*/
	game->test_section.base_navgraph = (NavGraph) {
		.node_count = 0,
		.node_cap = 128,
		.edge_count = 0,
		.edge_cap = 256
	};
	game->test_section.base_navgraph.nodes = calloc(game->test_section.base_navgraph.node_cap, sizeof(NavNode));
	game->test_section.base_navgraph.edges = calloc(game->test_section.base_navgraph.edge_cap, sizeof(NavEdge));

	PlayerInit(&game->camera, &game->input_handler, &game->test_section, &player_data, &game->ent_handler);
	Entity player = (Entity) {
		.comp_transform = (comp_Transform) {0},
		.comp_health = (comp_Health) {0},
		.comp_weapon = (comp_Weapon) {0},
		.type = 0,
		.flags = (ENT_ACTIVE)
	};
	player.comp_transform.bounds.max = Vector3Scale(BODY_VOLUME_MEDIUM,  0.5f);
	player.comp_transform.bounds.min = Vector3Scale(BODY_VOLUME_MEDIUM, -0.5f);
	player.comp_transform.on_ground = true;

	player.comp_health = (comp_Health) {0};
	player.comp_health.amount = 100;

	game->ent_handler.count = spawn_list.count;
	for(int i = 0; i < spawn_list.count; i++) 
		ProcessEntity(&spawn_list.arr[i], &game->ent_handler, &game->test_section.base_navgraph);

	BuildNavGraph(&game->test_section);


	for(u16 i = 0; i < game->test_section.base_navgraph.node_count; i++) {
		NavNode *node = &game->test_section.base_navgraph.nodes[i];

		//printf("%d\n", node->id);
		//printf("%d\n", node->edge_count);
	}

	player.comp_transform.position = game->ent_handler.player_start;
	game->ent_handler.ents[0] = player;

	game->player_gun = (PlayerGun) {0};
	PlayerGunInit(&game->player_gun, &game->ent_handler.ents[0]);
}

void GameUpdate(Game *game, float dt) {
	if(IsKeyPressed(KEY_ESCAPE))
		game->flags |= FLAG_EXIT_REQUEST;

	VirtCameraControls(&game->camera_debug, dt, game->ent_handler.ents[0].comp_transform.position);

	PollInput(&game->input_handler);
	PlayerGunUpdate(&game->player_gun, dt);

	UpdateEntities(&game->ent_handler, &game->test_section, dt);
}

#define DEBUG_ENABLE			0x01
#define DEBUG_DRAW_HULLS 		0x02
#define DEBUG_DRAW_BIG	 		0x04
#define DEBUG_DRAW_FULL_MODEL	0x08
#define DEBUG_DRAW_BVH			0x10
u8 debug_draw_flags = 1;

void GameDraw(Game *game) {
	// 3D Rendering, main
	BeginDrawing();
	BeginTextureMode(game->render_target3D);
	ClearBackground(BLACK);
		BeginMode3D(game->camera);

			DrawModel(game->test_section.model, Vector3Zero(), 1, WHITE);
			//DrawModel(game->test_section.model, Vector3Zero(), 1, ColorAlpha(WHITE, 0.95f));
			//DrawModelWires(game->test_section.model, Vector3Zero(), 1, GREEN);

			//PlayerDisplayDebugInfo(&game->ent_handler.ents[0]);
			RenderEntities(&game->ent_handler, GetFrameTime());

			/*
			for(u16 i = 0; i < tri_count; i++) {
				Tri *tri = &tris[i];

				Color color = colors[i % 6];
				DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(color, 1.00f));
			}
			*/

			/*
			for(u16 j = 0; j < game->test_section.bvh[1].tris.count; j++) {
				//Tri *tri = &game->test_section.bvh[1].tris.arr[j];
				Tri *tri = &game->test_section.bvh[1].tris.arr[j];
				Color color = colors[j % 6];
				DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(color, 0.5f));
			}
			*/
			
			if(debug_draw_flags & DEBUG_DRAW_HULLS) { 
				for(u16 j = 0; j < game->test_section.bvh[1].tris.count; j++) {
					Tri *tri = &game->test_section.bvh[1].tris.arr[j];
					//Color color = colors[tri->hull_id % 7];
					Color color = {
						.r = tri->normal.x * 255,
						.g = tri->normal.y * 255,
						.b = tri->normal.z * 255,
						255
					};
					DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(color, 0.25f));

					Vector3 c = TriCentroid(*tri);
					DrawLine3D(Vector3Subtract(c, Vector3Scale(tri->normal, 10)), Vector3Add(c, Vector3Scale(tri->normal, 10)), color);
				}

				/*
				for(u16 j = 0; j < game->test_section._hulls[1].count; j++) {
					Hull *hull = &game->test_section._hulls[1].arr[j];
					DrawBoundingBox(hull->aabb, colors[j % 7]);

					for(short k = 0; k < hull->plane_count; k++) {
						Plane *pl = &hull->planes[k];

						DrawLine3D(hull->center, Vector3Add(hull->center, Vector3Scale(pl->normal, 35)), SKYBLUE);
					}
				}
				*/
			}
			RenderEntities(&game->ent_handler, GetFrameTime());
			//DebugDrawNavGraphs(&game->test_section, sphere_model);

		EndMode3D();

	EndTextureMode();

	BeginTextureMode(game->render_target2D);
	ClearBackground(BLANK);
		PlayerGunDraw(&game->player_gun);
	EndTextureMode();

	if(debug_draw_flags & DEBUG_ENABLE) {
		// 3D Rendering, debug
		BeginTextureMode(game->render_target_debug);

		float clear_alpha = (debug_draw_flags & DEBUG_DRAW_BIG) ? 1 : 0.95f;
		ClearBackground(ColorAlpha(BLACK, clear_alpha));

			BeginMode3D(game->camera_debug);
				if(IsKeyPressed(KEY_U)) debug_draw_flags ^= DEBUG_DRAW_FULL_MODEL;
				if(debug_draw_flags & DEBUG_DRAW_FULL_MODEL) DrawModel(game->test_section.model, Vector3Zero(), 1, ColorAlpha(DARKGRAY, 1.0f));

				//DrawModelWires(game->test_section.model, Vector3Zero(), 1, RAYWHITE);
				//DrawBoundingBox(game->test_section.bvh.nodes[0].bounds, WHITE);

				PlayerDisplayDebugInfo(&game->ent_handler.ents[0]);
				RenderEntities(&game->ent_handler, GetFrameTime());
				//BrushTestView(&brush_pool, SKYBLUE);
				//BrushTestView(&brush_pool_exp, RED);

				/*
				DrawRay((Ray){.position = Vector3Zero(), .direction = (Vector3) {1, 0, 0} }, RED);
				DrawRay((Ray){.position = Vector3Zero(), .direction = UP}, GREEN);
				DrawRay((Ray){.position = Vector3Zero(), .direction = (Vector3) {0, 0, 1} }, BLUE);
				*/

				if(IsKeyPressed(KEY_H)) debug_draw_flags ^= DEBUG_DRAW_HULLS;
				if(debug_draw_flags & DEBUG_DRAW_HULLS) { 
					for(u16 j = 0; j < game->test_section.bvh[1].tris.count; j++) {
						//Tri *tri = &game->test_section.bvh[1].tris.arr[j];
						Tri *tri = &game->test_section.bvh[1].tris.arr[j];
						Color color = colors[tri->hull_id % 7];
						//DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorTint(color, BROWN));
					}
					for(u16 j = 0; j < game->test_section._hulls[1].count; j++) {
						Hull *hull = &game->test_section._hulls[1].arr[j];
						DrawBoundingBox(hull->aabb, colors[j % 7]);
					}
				}

				if(IsKeyPressed(KEY_B)) debug_draw_flags ^= DEBUG_DRAW_BVH;
				if(debug_draw_flags & DEBUG_DRAW_BVH) { 
					for(u16 j = 0; j < game->test_section.bvh[0].count; j++) {
						BvhNode *node = &game->test_section.bvh->nodes[j];

						Color color = colors[j % 7];
						bool leaf = (node->tri_count > 0);
						if(!leaf) color = ColorAlpha(GRAY, 0.5f);

						DrawBoundingBox(node->bounds, color);
					}
				}

			DebugDrawNavGraphs(&game->test_section, sphere_model);

			EndMode3D();

			Vector2 dbg_window_size = (Vector2) { .x = game->render_target_debug.texture.width, .y = game->render_target_debug.texture.height };
			DebugDrawNavGraphsText(&game->test_section, game->camera_debug, dbg_window_size);
		}

			// 2D
			//DrawText(TextFormat("accel: %.02f", player_data.accel), 0, 40, 32, RAYWHITE);
			//DrawEntsDebugInfo();

		EndTextureMode();
	// 2D Rendering

	// Draw to buffers:
	// Main
	Rectangle rt_src = (Rectangle) { 0, 0, game->render_target3D.texture.width, -game->render_target3D.texture.height };
	Rectangle rt_dst = (Rectangle) { 0, 0, game->conf->window_width, game->conf->window_height };
	DrawTexturePro(game->render_target3D.texture, rt_src, rt_dst, Vector2Zero(), 0, WHITE);

	rt_src = (Rectangle) { 0, 0, game->render_target2D.texture.width, -game->render_target2D.texture.height };
	rt_dst = (Rectangle) { 0, 0, game->conf->window_width, game->conf->window_height };
	DrawTexturePro(game->render_target2D.texture, rt_src, rt_dst, Vector2Zero(), 0, WHITE);

	PlayerDebugText(&game->ent_handler.ents[0]);

	if(IsKeyPressed(KEY_T))
		debug_draw_flags ^= DEBUG_DRAW_BIG;

	/*
	Vector2 debug_wh = (debug_draw_flags & DEBUG_DRAW_BIG) 
		? (Vector2) { 1920, 1080 } 
		: (Vector2) {game->render_target_debug.texture.width, game->render_target_debug.texture.height };
	*/

	Vector2 debug_wh = (debug_draw_flags & DEBUG_DRAW_BIG) 
		? (Vector2) { VIRT_W, VIRT_H } 
		: (Vector2) { VIRT_W * 0.5f, VIRT_H * 0.5f };

	// Debug
	rt_src = (Rectangle) { 0, 0, game->render_target_debug.texture.width, -game->render_target_debug.texture.height };
	rt_dst = (Rectangle) { 0, 0, debug_wh.x, debug_wh.y };
	DrawTexturePro(game->render_target_debug.texture, rt_src, rt_dst, Vector2Zero(), 0, WHITE);

	int fps = GetFPS();
	DrawText(TextFormat("fps: %d", fps), 4, 4, 32, RAYWHITE);

	//EntDebugText();

	EndDrawing();
}

void VirtCameraControls(Camera3D *cam, float dt, Vector3 target_point) {
	Vector3 forward = Vector3Normalize(Vector3Subtract(cam->target, cam->position)); 
	Vector3 right = Vector3CrossProduct(forward, cam->up);
	
	Vector3 movement = Vector3Zero();	

	movement = Vector3Add(movement, Vector3Scale(forward, GetMouseWheelMove() * 10));

	if(IsKeyDown(KEY_UP)) 		movement = Vector3Add(movement, cam->up);
	if(IsKeyDown(KEY_RIGHT)) 	movement = Vector3Add(movement, right);
	if(IsKeyDown(KEY_DOWN))		movement = Vector3Subtract(movement, cam->up);
	if(IsKeyDown(KEY_LEFT))		movement = Vector3Subtract(movement, right);

	movement = Vector3Scale(movement, 300 * dt);
	
	if(IsKeyDown(KEY_Q)) {
		movement = Vector3Scale(right, -300 * dt);
		cam->position = Vector3Add(cam->position, movement);
		//cam->target = Vector3Subtract(cam->target, Vector3Scale(movement, 1.0f));
		return;
	}

	if(IsKeyDown(KEY_E)) {
		movement = Vector3Scale(right,  300 * dt);
		cam->position = Vector3Add(cam->position, movement);
		//cam->target = Vector3Subtract(cam->target, Vector3Scale(movement, 1.0f));
		return;
	}

	cam->position = Vector3Add(cam->position, movement);
	cam->target = Vector3Add(cam->target, movement);
	
	if(IsKeyDown(KEY_M))
		cam->target = target_point;
}

