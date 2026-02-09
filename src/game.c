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

void VirtCameraControls(Camera3D *cam, float dt);

float *plr_accel;

Color colors[] = {
	PINK,
	BLUE,
	GREEN,
	SKYBLUE,
	GRAY,
	MAGENTA
};

PlayerDebugData player_data = {0};

Material mat_default;

BrushPool brush_pool = (BrushPool) {0};
BrushPool brush_pool_exp = (BrushPool) {0};

void GameInit(Game *game, Config *conf) {
	game->conf = conf;	

	InputInit(&game->input_handler);

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
		.fovy = 10,
		.projection = CAMERA_PERSPECTIVE
	};

	// Load render textures
	game->render_target3D = LoadRenderTexture(game->conf->window_width, game->conf->window_height);
	game->render_target2D = LoadRenderTexture(game->conf->window_width, game->conf->window_height);
	game->render_target_debug = LoadRenderTexture(game->conf->window_width * 0.35f, game->conf->window_height * 0.35f);

	game->player_gun = (PlayerGun) {0};
	PlayerGunInit(&game->player_gun);

	EntHandlerInit(&game->ent_handler);

	mat_default = LoadMaterialDefault();
	mat_default.maps[MATERIAL_MAP_DIFFUSE].color = ColorAlpha(BLUE, 0.25f);
}

void GameLoadTestScene(Game *game, char *path) {
	if(!DirectoryExists(path)) {
		MessageError("Missing directory", path);
		return;
	}

	FilePathList path_list = LoadDirectoryFiles(path);

	short model_id = -1;
	for(short i = 0; i < path_list.count; i++) {
		if(strcmp(GetFileExtension(path_list.paths[i]), ".glb") == 0) 
			model_id = i;
	}

	if(model_id == -1) {
		MessageError("Missing model", NULL);
		return;
	}

	Model model = LoadModel(path_list.paths[model_id]);

	game->test_section = (MapSection) {0};
	MapSectionInit(&game->test_section, model);

	short mpf_id = -1;
	for(short i = 0; i < path_list.count; i++) {
		if(strcmp(GetFileExtension(path_list.paths[i]), ".map") == 0) 
			mpf_id = i;
	}
	if(mpf_id > -1) LoadMapFile(&brush_pool, path_list.paths[mpf_id], &model);

	brush_pool_exp = ExpandBrushes(&brush_pool, BODY_VOLUME_MEDIUM);

	PlayerInit(&game->camera, &game->input_handler, &game->test_section, &player_data);

	Entity player = (Entity) {
		.comp_transform = (comp_Transform) {0},
		.comp_health = (comp_Health) {0},
		.comp_weapon = (comp_Weapon) {0},
		.behavior_id = 0,
		.flags = (ENT_ACTIVE)
	};

	player.comp_transform.bounds.max = BODY_VOLUME_MEDIUM;
	player.comp_transform.bounds.min = Vector3Scale(player.comp_transform.bounds.max, -1);
	player.comp_transform.radius = BoundsToRadius(player.comp_transform.bounds);
	player.comp_transform.position.y = 70;
	player.comp_transform.on_ground = true;
	player.comp_transform.air_time = 0;
	game->ent_handler.ents[game->ent_handler.count++] = player;

	/*
	hull_point_count = 0;
	for(u16 i = 0; i < game->test_section.hull_count; i++) {
		Hull *hull = &game->test_section.hulls[i];
		hull_point_count += hull->vertex_count;
	}

	hull_point_meshes = calloc(hull_point_count, sizeof(Model));
	hull_point_count = 0;

	for(u16 i = 0; i < game->test_section.hull_count; i++) {
		Hull *hull = &game->test_section.hulls[i];

		for(short j = 0; j < hull->vertex_count; j++) {
			Vector3 v = hull->vertices[j];

			Mesh mesh = GenMeshSphere(2, 8, 16);
			Model model = LoadModelFromMesh(mesh);
			model.transform = MatrixTranslate(v.x, v.y, v.z);	
			hull_point_meshes[hull_point_count++] = model;
		}
	}
	*/
}

void GameUpdate(Game *game, float dt) {
	if(IsKeyPressed(KEY_ESCAPE))
		game->flags |= FLAG_EXIT_REQUEST;

	VirtCameraControls(&game->camera_debug, dt);

	PollInput(&game->input_handler);
	PlayerGunUpdate(&game->player_gun, dt);

	UpdateEntities(&game->ent_handler, dt);
}

void GameDraw(Game *game) {
	rlDisableBackfaceCulling();

	// 3D Rendering, main
	BeginDrawing();
	BeginTextureMode(game->render_target3D);
	ClearBackground(BLACK);
		BeginMode3D(game->camera);

			DrawModel(game->test_section.model, Vector3Zero(), 1, WHITE);
			//DrawModel(game->test_section.model, Vector3Zero(), 1, ColorAlpha(WHITE, 0.95f));
			//DrawModelWires(game->test_section.model, Vector3Zero(), 1, GREEN);

			//PlayerDisplayDebugInfo(&game->ent_handler.ents[0]);
			RenderEntities(&game->ent_handler);

			BrushTestView(&brush_pool, SKYBLUE);
			BrushTestView(&brush_pool_exp, RED);

			/*
			for(u16 i = 0; i < game->test_section.tri_count; i++) {
				Tri *tri = &game->test_section.tris[i];

				Color color = colors[tri->hull_id % 6];
				//DrawTriangle3D(tri->vertices[0], tri->vertices[1], tri->vertices[2], ColorAlpha(color, 0.5f));
			}
			*/

			/*
			for(u16 i = 0; i < hull_point_count; i++) {
				DrawModel(hull_point_meshes[i], Vector3Zero(), 1, BLUE);	
			}
			*/

			for(u16 i = 0; i < game->test_section.hull_count; i++) {
				Hull *hull = &game->test_section.hulls[i];
		
				DrawMesh(game->test_section.model.meshes[i], mat_default, game->test_section.model.transform);
			}

			//DrawModelPoints(game->test_section.model, Vector3Zero(), 1, BLUE);
			
		EndMode3D();

	EndTextureMode();

	BeginTextureMode(game->render_target2D);
	ClearBackground(BLANK);
		PlayerGunDraw(&game->player_gun);
	EndTextureMode();
	
	// 3D Rendering, debug
	BeginTextureMode(game->render_target_debug);
	ClearBackground(ColorAlpha(BLACK, 0.95f));
		BeginMode3D(game->camera_debug);
			//DrawModel(game->test_section.model, Vector3Zero(), 1, ColorAlpha(DARKGRAY, 0.1f));
			DrawModelWires(game->test_section.model, Vector3Zero(), 1, BLUE);
			//DrawBoundingBox(game->test_section.bvh.nodes[0].bounds, WHITE);

			PlayerDisplayDebugInfo(&game->ent_handler.ents[0]);
			RenderEntities(&game->ent_handler);
			BrushTestView(&brush_pool, SKYBLUE);
			BrushTestView(&brush_pool_exp, RED);

			DrawRay((Ray){.position = Vector3Zero(), .direction = (Vector3) {1, 0, 0} }, RED);
			DrawRay((Ray){.position = Vector3Zero(), .direction = UP}, GREEN);
			DrawRay((Ray){.position = Vector3Zero(), .direction = (Vector3) {0, 0, 1} }, SKYBLUE);

		EndMode3D();

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

	// Debug
	rt_src = (Rectangle) { 0, 0, game->render_target_debug.texture.width, -game->render_target_debug.texture.height };
	rt_dst = (Rectangle) { 0, 0, game->render_target_debug.texture.width,  game->render_target_debug.texture.height };
	DrawTexturePro(game->render_target_debug.texture, rt_src, rt_dst, Vector2Zero(), 0, WHITE);

	int fps = GetFPS();
	DrawText(TextFormat("fps: %d", fps), 4, 4, 32, RAYWHITE);

	EndDrawing();
}

void VirtCameraControls(Camera3D *cam, float dt) {
	Vector3 forward = Vector3Normalize(Vector3Subtract(cam->target, cam->position)); 
	Vector3 right = Vector3CrossProduct(forward, cam->up);
	
	Vector3 movement = Vector3Zero();	

	movement = Vector3Add(movement, Vector3Scale(forward, GetMouseWheelMove() * 10));

	if(IsKeyDown(KEY_UP)) 		movement = Vector3Add(movement, cam->up);
	if(IsKeyDown(KEY_RIGHT)) 	movement = Vector3Add(movement, right);
	if(IsKeyDown(KEY_DOWN))		movement = Vector3Subtract(movement, cam->up);
	if(IsKeyDown(KEY_LEFT))		movement = Vector3Subtract(movement, right);

	movement = Vector3Scale(movement, 300 * dt);
	
	cam->position = Vector3Add(cam->position, movement);
	cam->target = Vector3Add(cam->target, movement);
}

