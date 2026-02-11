#include "../include/num_redefs.h"
#include "raylib.h"

#ifndef INPUT_HANDLER_H_
#define INPUT_HANDLER_H_

enum INPUT_METHODS : u8 {
	INPUT_DEVICE_KEYBOARD  		= 0,
	INPUT_DEVICE_CONTROLLER		= 1		// * not implemented!
};

// Possible input states
#define INPUT_UP			0
#define INPUT_DOWN			1
#define INPUT_PRESSED		2
#define INPUT_RELEASED		3

typedef struct {
	int key;		// Keyboard key 		
	int button;		// Mouse button

	short state;	// up, down, press, release
	
} InputAction;

#define INPUT_ACTION_COUNT	16

// Action indices, keyboard 
#define ACTION_MOVE_U		0
#define ACTION_MOVE_L		1
#define ACTION_MOVE_D		2
#define ACTION_MOVE_R		3
#define ACTION_JUMP			4

typedef struct {
	InputAction actions[INPUT_ACTION_COUNT];

	Vector2 mouse_position;
	Vector2 mouse_delta;

	float mouse_sensitivity;

	u8 input_method;

} InputHandler;

void InputInit(InputHandler *handler);
void PollInput(InputHandler *handler);

#endif

