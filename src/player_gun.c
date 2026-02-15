#include "raylib.h"
#include "raymath.h"
#include "player_gun.h"
#include "geo.h"

Vector3 gun_pos = {0};
float gun_rot = 0;
float p, y, r;
Matrix mat = {0};

#define REVOLVER_REST (Vector3) { -0.75f, -2.35f, 6.25f }
#define REVOLVER_ANGLE_REST 2.5f

float recoil = 0;
bool recoil_add = false;

float friction = 0;

void PlayerGunInit(PlayerGun *player_gun) {
	player_gun->cam = (Camera3D) {
		.position = (Vector3) { 0, 0, -1 },
		.target = (Vector3) { 0, 0, 1 },
		.up = (Vector3) {0, 1, 0},
		.fovy = 54,
		.projection = CAMERA_PERSPECTIVE
	};

	player_gun->model = LoadModel("resources/models/weapons/rev_00.glb");
	mat = player_gun->model.transform;

	gun_pos = REVOLVER_REST;
	gun_rot = REVOLVER_ANGLE_REST;
}

void PlayerGunUpdate(PlayerGun *player_gun, float dt) {
	player_gun->model.transform = mat;

	float recoil_angle = Clamp(recoil + gun_rot, -30, 90.0f);
	mat = MatrixRotateX(-recoil_angle * DEG2RAD);
	mat = MatrixMultiply(mat, MatrixRotateY(REVOLVER_ANGLE_REST * DEG2RAD));

	friction = (recoil > 40) ? 4.9f : 10.5f;
	//if(recoil >= 100) friction = friction - (recoil * 0.033f);

	//recoil -= ((recoil * (1 - EPSILON)) * 11.5f) * dt; 
	recoil -= (recoil * friction) * dt; 
	if(recoil <= -EPSILON) recoil = 0;

	if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && recoil <= 1.0f) {
		recoil_add = false;
		recoil += 130 + (GetRandomValue(10, 30) * 0.1f);
	}

	//gun_pos.z = REVOLVER_REST.z - (recoil * 0.0485f);
	gun_pos.z = REVOLVER_REST.z - (recoil * 0.05f);
	//gun_pos.z = Lerp(gun_pos.z, REVOLVER_REST.z - (recoil * 0.085f), 20*dt);
}

void PlayerGunDraw(PlayerGun *player_gun) {
	BeginMode3D(player_gun->cam);
	
	DrawModel(player_gun->model, gun_pos, 1.0f, WHITE);
	//DrawModelEx(player_gun->model, gun_pos, UP, gun_rot, Vector3Scale(Vector3One(), 1.0f), WHITE);

	EndMode3D();
}

