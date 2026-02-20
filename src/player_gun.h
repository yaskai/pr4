#include "../include/num_redefs.h"
#include "raylib.h"
#include "ent.h"
#include "v_effect.h"

#ifndef PLAYER_GUN_H_
#define PLAYER_GUN_H_

typedef struct {
	Model model;

	Camera3D cam;

	u16 current_gun;

} PlayerGun;

void PlayerGunInit(PlayerGun *player_gun, Entity *player, EntityHandler *handler, MapSection *sect, vEffect_Manager *effect_manager);
void PlayerGunUpdate(PlayerGun *player_gun, float dt);
void PlayerGunDraw(PlayerGun *player_gun);

void PlayerShoot(PlayerGun *player_gun, EntityHandler *handler, MapSection *sect);

#endif
