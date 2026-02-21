#include "raylib.h"
#include "raymath.h"
#include "anim.h"

void FabrikSolve(FabrikChain *chain) {
	if(chain->count < 2)
		return;

	float total_length = 0.0f;
	for(int i = 0; i < chain->count; i++) {
		total_length += chain->lengths[i];
	}

	Vector3 root_to_targ = Vector3Subtract(chain->targ_pos, chain->root_pos); 
	float rt_dist = Vector3Length(root_to_targ);

	// Target is unreachable, stretch
	if(rt_dist > total_length) {
		Vector3 dir = Vector3Normalize(root_to_targ);

		chain->points[0] = chain->root_pos;

		for(int i = 1; i < chain->count; i++) {
			chain->points[i] = Vector3Add(chain->points[i-1], Vector3Scale(dir, chain->lengths[i-1]));
		}
	}
	
	// Target can be reached, iterate solve
	for(int i = 0; i < FABRIK_MAX_ITERS; i++) {
		// Backwards
		chain->points[chain->count-1] = chain->targ_pos;

		for(int j = chain->count-2; j >= 0; j--) {
			Vector3 dir = Vector3Subtract(chain->points[j], chain->points[j+1]);
			dir = Vector3Normalize(dir);

			chain->points[j] = Vector3Add(chain->points[j+1], Vector3Scale(dir, chain->lengths[j]));
		}

		// Forwards
		// move root to end 
		chain->points[0] = chain->root_pos;

		for(int j = 1; j < chain->count; j++) {
			Vector3 dir = Vector3Subtract(chain->points[j], chain->points[j-1]);
			dir = Vector3Normalize(dir);

			chain->points[j] = Vector3Add(chain->points[j-1], Vector3Scale(dir, chain->lengths[j-1]));
		}

		if(Vector3Distance(chain->points[chain->count-1], chain->targ_pos) <= 0.001f)
			break;
	}
}



