#ifndef COOPCONTROLS_H
#define COOPCONTROLS_H

// [library:input] 4-player couch co-op controls (portable build only).
//
// Versus races take every player's controls from controls.ini next to the exe
// (written with defaults on first use, re-read before each race). By default
// player N uses gamepad N when one is plugged in, otherwise keyboard set N.

#include "save/inputbindingstate.h"
#include "types.h"

class InputManager;

enum {
	c_coopMaxPlayers = 4,
};

// Fills the race binding for p_playerIndex (0-3). Returns FALSE when no device fits.
LegoBool32 CoopControls_GetBinding(
	InputManager* p_inputManager,
	LegoU32 p_playerIndex,
	InputBindingState::Entry* p_entry
);

// Path of controls.ini (for logs/messages).
const char* CoopControls_GetPath();

#endif // COOPCONTROLS_H
