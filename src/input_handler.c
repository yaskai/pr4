#include "raylib.h"
#include "input_handler.h"

void InputInit(InputHandler *handler) {
	handler->input_method = INPUT_DEVICE_KEYBOARD;	

	//handler->mouse_sensitivity = 0.475f;
	handler->mouse_sensitivity = 0.0045f;
	//handler->mouse_sensitivity = 0.0195f;

	handler->actions[ACTION_MOVE_L].key 	= KEY_A;
	handler->actions[ACTION_MOVE_R].key 	= KEY_D;
	handler->actions[ACTION_MOVE_U].key 	= KEY_W;
	handler->actions[ACTION_MOVE_D].key 	= KEY_S;

	handler->actions[ACTION_JUMP].key = KEY_SPACE;
}

void PollInput(InputHandler *handler) {
	handler->mouse_position = GetMousePosition();

	float dt = GetFrameTime();

	// Get mouse delta, scale by sensitivity setting
	handler->mouse_delta = GetMouseDelta();
	handler->mouse_delta.x *= handler->mouse_sensitivity;
	handler->mouse_delta.y *= handler->mouse_sensitivity;

	for(u8 i = 0; i < INPUT_ACTION_COUNT; i++) {
		InputAction *action = &handler->actions[i];

		i32 key = action->key;
		if(!key) continue;

		if(IsKeyUp(key))
			action->state = INPUT_UP;

		if(IsKeyDown(key)) 
			action->state = INPUT_DOWN;

		if(IsKeyPressed(key))
			action->state = INPUT_PRESSED;

		if(IsKeyReleased(key))
			action->state = INPUT_RELEASED;
	}
}
