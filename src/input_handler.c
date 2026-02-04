#include "raylib.h"
#include "input_handler.h"

void InputInit(InputHandler *handler) {
	handler->input_method = INPUT_DEVICE_KEYBOARD;	

	//handler->mouse_sensitivity = 0.475f;
	handler->mouse_sensitivity = 0.0045f;

	handler->actions[ACTION_MOVE_LEFT].key 	= KEY_A;
	handler->actions[ACTION_MOVE_RIGHT].key = KEY_D;
	handler->actions[ACTION_MOVE_UP].key 	= KEY_W;
	handler->actions[ACTION_MOVE_DOWN].key 	= KEY_S;

	handler->actions[ACTION_JUMP].key = KEY_SPACE;
}

void PollInput(InputHandler *handler) {
	handler->mouse_position = GetMousePosition();
	handler->mouse_delta = GetMouseDelta();

	for(u8 i = 0; i < INPUT_ACTION_COUNT; i++) {
		InputAction *action = &handler->actions[i];

		i32 key = action->key;
		if(!key) continue;

		if(IsKeyUp(key))
			action->state = INPUT_ACTION_UP;

		if(IsKeyDown(key)) 
			action->state = INPUT_ACTION_DOWN;

		if(IsKeyPressed(key))
			action->state = INPUT_ACTION_PRESSED;

		if(IsKeyReleased(key))
			action->state = INPUT_ACTION_RELEASED;
	}
}
