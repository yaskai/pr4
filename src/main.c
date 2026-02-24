#include "raylib.h"
#include "raymath.h"
#include "config.h"
#include "game.h"

int main() {
	Config conf = (Config) {0};
	ConfigInit(&conf);
	ConfigRead(&conf, "options.conf");

	Game game = (Game) {0};
	GameInit(&game, &conf);

	SetTraceLogLevel(LOG_ERROR);
	//SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT | FLAG_BORDERLESS_WINDOWED_MODE | FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_VSYNC_HINT);
	InitWindow(conf.window_width, conf.window_height, "DISRUPTOR");

	GameRenderSetup(&game);
	//GameLoadTestScene(&game, "resources/maps/test1");
	//GameLoadTestScene(&game, "resources/maps/encounter_00");
	//GameLoadTestScene1(&game, "resources/maps/test0");
	//GameLoadTestScene1(&game, "resources/maps/test1");
	//GameLoadTestScene1(&game, "resources/maps/test03");
	GameLoadTestScene1(&game, "resources/maps/05");

	// Disable exit key
	SetExitKey(KEY_NULL);
	DisableCursor();

	bool exit = false;
	while(!exit) {
		exit = ((game.flags & FLAG_EXIT_REQUEST) || WindowShouldClose());
		float dt = GetFrameTime(); 	

		GameUpdate(&game, dt);
		GameDraw(&game, dt);
	}

	CloseWindow();

	ConfigClose(&conf);
	GameClose(&game);

	return 0;
}

