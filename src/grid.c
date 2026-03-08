#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "ent.h"

void EntGridInit(EntityHandler *handler) {
	//printf("grid init\n");

	EntGrid grid = (EntGrid) {0};

	grid.size = (Coords) { .c = 32, .r = 32, .t = 12 };

	grid.cell_count = grid.size.c * grid.size.r * grid.size.t;
	grid.cells = calloc(grid.cell_count, sizeof(EntGridCell));

	/*
	printf("cols: %d\n", grid.size.c);
	printf("rows: %d\n", grid.size.r);
	printf("tabs: %d\n", grid.size.t);
	printf("cell count: %d\n", (grid.size.c * grid.size.r * grid.size.t));
	*/

	Vector3 origin = (Vector3) {
		(-grid.size.c * ENT_GRID_CELL_EXTENTS.x) * 0.5f,
		(-grid.size.r * ENT_GRID_CELL_EXTENTS.y) * 0.5f, 
		(-grid.size.t * ENT_GRID_CELL_EXTENTS.z) * 0.5f 
	};  
	grid.origin = origin;

	for(u16 i = 0; i < grid.cell_count; i++) {
		Coords coords = CellIdToCoords(i, &grid); 

		BoundingBox box = (BoundingBox) { .min = Vector3Scale(ENT_GRID_CELL_EXTENTS, -0.5f), .max = Vector3Scale(ENT_GRID_CELL_EXTENTS, 0.5f) };
		grid.cells[i].aabb = BoxTranslate(box, CoordsToVec3(coords, &grid));
		
		grid.cells[i].ent_count = 0;
		for(u8 j = 0; j < MAX_ENTS_PER_CELL; j++) grid.cells[i].ents[j] = 0;
	}

	handler->grid = grid;
}

void UpdateGrid(EntityHandler *handler) {
	EntGrid *grid = &handler->grid;

	for(u16 i = 0; i < handler->count; i++) {
		Entity *ent = &handler->ents[i];
		comp_Transform *ct = &ent->comp_transform;

		i16 src_id = ent->cell_id; 	

		Coords dest_coords = Vec3ToCoords(ct->position, grid);
		if(!CoordsInBounds(dest_coords, grid)) {
			//puts("dest coords out of bounds");
			continue;
		}

		i16 dest_id = CellCoordsToId(dest_coords, grid);

		EntGridCell *dest_cell = &grid->cells[dest_id];

		if(src_id == -1) {
			ent->cell_id = dest_id;
			dest_cell->ents[dest_cell->ent_count++] = ent->id;
			continue;
		}
	
		EntGridCell *src_cell = &grid->cells[src_id];
			
		if(src_id == dest_id)
			continue;

		/*
		if(!CheckCollisionBoxes(ct->bounds, src_cell->aabb)) {
			for(u8 j = 0; j < src_cell->ent_count; j++) {
				if(src_cell->ents[j] == ent->id) { 
					for(u8 n = j; n < src_cell->ent_count-1; n++)
						src_cell->ents[n] = src_cell->ents[n+1];

					src_cell->ent_count--;
					break;
				}
			}
		}
		*/

		for(u8 j = 0; j < src_cell->ent_count; j++) {
			if(src_cell->ents[j] == ent->id && !CheckCollisionBoxes(src_cell->aabb, ct->bounds)) { 
				for(u8 n = j; n < src_cell->ent_count-1; n++) {
					src_cell->ents[n] = src_cell->ents[n+1];
				}

				src_cell->ent_count--;
				break;
			}
		}

		if(dest_cell->ent_count + 1 > MAX_ENTS_PER_CELL)
			continue;

		ent->cell_id = dest_id;
		dest_cell->ents[dest_cell->ent_count++] = ent->id;
	}
}

int16_t CellCoordsToId(Coords coords, EntGrid *grid) {
	//return (coords.c + coords.r * + coords.t * grid->size.c * grid->size.r);
	return (
		coords.c + 
		coords.r * grid->size.c + 
		coords.t * grid->size.c * grid->size.r
	);
}

Coords CellIdToCoords(int16_t id, EntGrid *grid) {
	return (Coords) {
		.c = id % grid->size.c,						// x,
		.r = (id / grid->size.c) % grid->size.r,	// y,
		.t = id / (grid->size.c * grid->size.r)		// z
	};
}

Coords Vec3ToCoords(Vector3 v, EntGrid *grid) {
	Vector3 local = Vector3Subtract(v, grid->origin);

	return (Coords) {
		.c = ( (local.x) / ENT_GRID_CELL_EXTENTS.x ),
		.r = ( (local.y) / ENT_GRID_CELL_EXTENTS.y ),
		.t = ( (local.z) / ENT_GRID_CELL_EXTENTS.z )
	};
}

Vector3 CoordsToVec3(Coords coords, EntGrid *grid) {
	//return Vector3Scale((Vector3) { coords.c, coords.r, coords.t }, ENT_GRID_CELL_EXTENTS.x);
	Vector3 local = (Vector3) {
		coords.c * ENT_GRID_CELL_EXTENTS.x,
		coords.r * ENT_GRID_CELL_EXTENTS.y,
		coords.t * ENT_GRID_CELL_EXTENTS.z
	};
	return Vector3Add(local, grid->origin);
}

bool CoordsInBounds(Coords coords, EntGrid *grid) {
	return ( coords.c > -1 && coords.c < grid->size.c &&
			 coords.r > -1 && coords.r < grid->size.r &&
			 coords.t > -1 && coords.t < grid->size.t );
}

