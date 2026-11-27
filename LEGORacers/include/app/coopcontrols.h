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

// Control presets: 0-3 = gamepad 1-4, 4-7 = keyboard set 1-4.
enum {
	c_coopPresetNone = -1,
	c_coopPresetGamepad1 = 0,
	c_coopPresetKeyboard1 = 4,
	c_coopPresetCount = 8,
};

// The nine race actions, in InputBindingState::Entry::m_events order.
enum {
	c_coopActionLeft = 0,
	c_coopActionRight,
	c_coopActionAccelerate,
	c_coopActionBrake,
	c_coopActionPowerup,
	c_coopActionCamera,
	c_coopActionHud,
	c_coopActionSlide,
	c_coopActionLookback,
	c_coopActionCount
};

// Players who joined on the Versus pick screen use the preset they joined with
// (overrides [players] in controls.ini). c_coopPresetNone clears it.
void CoopControls_SetSessionPreset(LegoU32 p_playerIndex, LegoS32 p_preset);

// Event codes of a preset's nine actions (0 = unbound).
void CoopControls_GetPresetEvents(LegoS32 p_preset, LegoU32 p_events[c_coopActionCount]);

// Uppercase key legend for a preset, two lines separated by '\n'.
void CoopControls_DescribePreset(LegoS32 p_preset, char* p_buffer, LegoU32 p_size);

// Uppercase name of the key or button bound to one action of a preset.
void CoopControls_GetActionName(LegoS32 p_preset, LegoS32 p_action, char* p_buffer, LegoU32 p_size);

// Versus setup chosen on the Versus race screen.
struct CoopVersusSettings {
	LegoU32 m_playerCount; // 2-4
	LegoBool32 m_grandPrix;
	LegoU8 m_lapCount; // 1-5
};
CoopVersusSettings* CoopVersus_GetSettings();

#endif // COOPCONTROLS_H
