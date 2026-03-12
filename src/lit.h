#include "raylib.h"
#include "kbsp.h"

#ifndef LIT_H_
#define LIT_H_

Vector2 Bsp_FaceLightmapSize(Bsp_Data *bsp, Bsp_Face *face);
Image MakeLightmapAtlas(Bsp_Data *bsp, Rectangle *lm_uvs);

#endif
