#include "raylib.h"
#include "config.h"
#include "game.h"

int main() {
	Config conf = (Config) {0};
	ConfigInit(&conf);
	ConfigRead(&conf, "options.conf");

	Game game = (Game) {0};
	GameInit(&game, &conf);

	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT | FLAG_FULLSCREEN_MODE);
	InitWindow(conf.window_width, conf.window_height, "PR4");

	GameRenderSetup(&game);
	//GameLoadTestScene(&game, "resources/maps/test1");
	GameLoadTestScene(&game, "resources/maps/encounter_00");

	// Disable exit key
	SetExitKey(KEY_NULL);
	DisableCursor();

	bool exit = false;

	while(!exit) {
		exit = ((game.flags & FLAG_EXIT_REQUEST) || WindowShouldClose());

		float delta_time = GetFrameTime();
		GameUpdate(&game, delta_time);
		GameDraw(&game);
	}

	CloseWindow();

	ConfigClose(&conf);
	GameClose(&game);

	return 0;
}

