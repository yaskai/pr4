#include <float.h>
#include "raylib.h"
#include "raymath.h"
#include "lit.h"

Vector2 Bsp_FaceLightmapSize(Bsp_Data *bsp, Bsp_Face *face) {
	Bsp_Surface *surface = &bsp->surfaces[face->texinfo];

	float min_u = FLT_MAX, min_v = FLT_MAX;
	float max_u = -FLT_MAX, max_v = -FLT_MAX;

	for(int i = 0; i < face->edge_count; i++) {
		i32 ledge = bsp->ledges[face->first_edge + i];
		Vector3 vert = (ledge >= 0) ? bsp->verts[bsp->edges[ledge].v[0]] : bsp->verts[bsp->edges[ledge].v[1]];
		
		float u = Vector3DotProduct(vert, surface->vector_s) + surface->dist_s;
		float v = Vector3DotProduct(vert, surface->vector_t) + surface->dist_t;

		if(u < min_u)
			min_u = u;

		if(v < min_v)
			min_v = v;
		
		if(u > max_u)
			max_u = u;

		if(v > max_v)
			max_v = v;
	}
	
	return (Vector2) {
		.x = (max_u / 16) - (min_u / 16) + 1, 	// U
		.y = (max_v / 16) - (min_v / 16) + 1	// V
	};
}

Image MakeLightmapAtlas(Bsp_Data *bsp, Rectangle *lm_uvs) {
	Image atlas = (Image) {0};

	Rectangle frame_rec = (Rectangle) { 0 };

	for(int i = 0; i < bsp->num_faces; i++) {
		Bsp_Face *face = &bsp->faces[i];
		if(face->lightmap < 0)
			continue;

		Vector2 size = Bsp_FaceLightmapSize(bsp, face);

		if(size.x > frame_rec.width)
			frame_rec.width = size.x;
		
		if(size.y > frame_rec.height)
			frame_rec.height = size.y;
	}

	return atlas;
}

