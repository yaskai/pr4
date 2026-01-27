#include "../include/num_redefs.h"
#include "raylib.h"

#ifndef AUDIOPLAYER_H_
#define AUDIOPLAYER_H_

typedef struct {
	Camera3D *camera;

	u16 effect_count, effect_capacity;
	u16 track_count, track_capacity;

} AudioPlayer;

void AudioPlayerInit(AudioPlayer *ap, Camera3D *camera);
void AudioPlayerClose(AudioPlayer *ap);

#endif
