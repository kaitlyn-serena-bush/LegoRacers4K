// [library:input] 4-player couch co-op controls; see coopcontrols.h.

#include "app/coopcontrols.h"

#include "input/inputdevice.h"
#include "input/inputmanager.h"
#include "input/joystickdevice.h"
#include "input/keyboarddevice.h"

#include <SDL3/SDL.h>
#include <miniwin/dinput.h>
#include <miniwin/hdtexture.h>
#include <miniwin/miniwinapp.h>
#include <string.h>

namespace
{

// Order of the nine race actions in InputBindingState::Entry::m_events.
const char* const c_actionNames[9] =
	{"left", "right", "accelerate", "brake", "powerup", "camera", "hud", "slide", "lookback"};

const char* const c_defaultIni =
	"; LEGO Racers - controls for Versus races (2 to 4 players, split screen)\n"
	";\n"
	"; Player N uses gamepad N when it is plugged in, otherwise keyboard set N.\n"
	"; Gamepads can be plugged in or out at any time; the first pad becomes gamepad 1,\n"
	"; and a pad that is unplugged frees its number for the next one.\n"
	";\n"
	"; This file is read again before every race, so edits apply on the next race.\n"
	"; Delete it to get the defaults back.\n"
	"\n"
	"[players]\n"
	"; auto | gamepad1 | gamepad2 | gamepad3 | gamepad4 | keyboard1 | keyboard2 | keyboard3 | keyboard4\n"
	"player1 = auto\n"
	"player2 = auto\n"
	"player3 = auto\n"
	"player4 = auto\n"
	"\n"
	"; Gamepad buttons (same for every pad), SDL/Xbox names:\n"
	";   A B X Y LB RB LS RS Back Start\n"
	";   RT LT (triggers), StickLeft StickRight (left stick as buttons), none\n"
	"; Steering always works with the left stick (analog) and the D-pad.\n"
	"; RT also presses A and LT also presses B. Start pauses the race.\n"
	"[gamepad]\n"
	"left = none\n"
	"right = none\n"
	"accelerate = A\n"
	"brake = B\n"
	"powerup = X\n"
	"camera = Y\n"
	"hud = Back\n"
	"slide = RB\n"
	"lookback = LB\n"
	"\n"
	"; Keyboard sets use SDL key names, for example:\n"
	";   A-Z, 0-9, Up Down Left Right, Space, Return, Tab, Backspace,\n"
	";   Left Shift, Right Shift, Left Ctrl, Right Ctrl, Left Alt, Right Alt,\n"
	";   Keypad 0-9, Keypad Enter, Keypad +, Keypad -, Keypad *, Keypad /\n"
	"; Esc pauses the race.\n"
	"[keyboard1]\n"
	"left = Left\n"
	"right = Right\n"
	"accelerate = Up\n"
	"brake = Down\n"
	"powerup = Return\n"
	"camera = Right Ctrl\n"
	"hud = M\n"
	"slide = Space\n"
	"lookback = Right Alt\n"
	"\n"
	"[keyboard2]\n"
	"left = A\n"
	"right = D\n"
	"accelerate = W\n"
	"brake = S\n"
	"powerup = Left Alt\n"
	"camera = Q\n"
	"hud = E\n"
	"slide = Left Shift\n"
	"lookback = Z\n"
	"\n"
	"[keyboard3]\n"
	"left = J\n"
	"right = L\n"
	"accelerate = I\n"
	"brake = K\n"
	"powerup = O\n"
	"camera = 9\n"
	"hud = 0\n"
	"slide = U\n"
	"lookback = P\n"
	"\n"
	"[keyboard4]\n"
	"left = Keypad 4\n"
	"right = Keypad 6\n"
	"accelerate = Keypad 8\n"
	"brake = Keypad 5\n"
	"powerup = Keypad 0\n"
	"camera = Keypad /\n"
	"hud = Keypad *\n"
	"slide = Keypad Enter\n"
	"lookback = Keypad 7\n";

char g_path[1024];

struct Ini {
	// section index: 0 players, 1 gamepad, 2..5 keyboard1..4
	char m_values[6][9][48];
	char m_players[4][24];
};

LegoS32 FindAction(const char* p_name)
{
	for (LegoS32 i = 0; i < 9; i++) {
		if (SDL_strcasecmp(p_name, c_actionNames[i]) == 0) {
			return i;
		}
	}
	return -1;
}

void Trim(char* p_text)
{
	char* start = p_text;
	while (*start == ' ' || *start == '\t') {
		start++;
	}
	size_t length = SDL_strlen(start);
	while (length && (start[length - 1] == ' ' || start[length - 1] == '\t' || start[length - 1] == '\r' ||
					  start[length - 1] == '\n')) {
		length--;
	}
	SDL_memmove(p_text, start, length);
	p_text[length] = '\0';
}

void Parse(const char* p_text, Ini* p_ini)
{
	LegoS32 section = -1;
	const char* cursor = p_text;

	while (*cursor) {
		char line[256];
		size_t length = 0;
		while (cursor[length] && cursor[length] != '\n') {
			length++;
		}
		size_t copy = length < sizeof(line) - 1 ? length : sizeof(line) - 1;
		SDL_memcpy(line, cursor, copy);
		line[copy] = '\0';
		cursor += length;
		if (*cursor == '\n') {
			cursor++;
		}

		char* comment = SDL_strchr(line, ';');
		if (comment) {
			*comment = '\0';
		}
		comment = SDL_strchr(line, '#');
		if (comment) {
			*comment = '\0';
		}
		Trim(line);
		if (!line[0]) {
			continue;
		}

		if (line[0] == '[') {
			char* end = SDL_strchr(line, ']');
			if (end) {
				*end = '\0';
			}
			const char* name = line + 1;
			section = -1;
			if (SDL_strcasecmp(name, "players") == 0) {
				section = 0;
			}
			else if (SDL_strcasecmp(name, "gamepad") == 0) {
				section = 1;
			}
			else if (SDL_strncasecmp(name, "keyboard", 8) == 0 && name[8] >= '1' && name[8] <= '4' && !name[9]) {
				section = 2 + (name[8] - '1');
			}
			continue;
		}

		char* equals = SDL_strchr(line, '=');
		if (!equals || section < 0) {
			continue;
		}
		*equals = '\0';
		char* key = line;
		char* value = equals + 1;
		Trim(key);
		Trim(value);

		if (section == 0) {
			if (SDL_strncasecmp(key, "player", 6) == 0 && key[6] >= '1' && key[6] <= '4' && !key[7]) {
				SDL_strlcpy(p_ini->m_players[key[6] - '1'], value, sizeof(p_ini->m_players[0]));
			}
			continue;
		}

		LegoS32 action = FindAction(key);
		if (action >= 0) {
			SDL_strlcpy(p_ini->m_values[section][action], value, sizeof(p_ini->m_values[0][0]));
		}
	}
}

void LoadIni(Ini* p_ini)
{
	SDL_memset(p_ini, 0, sizeof(*p_ini));
	Parse(c_defaultIni, p_ini);

	if (!g_path[0]) {
		const char* custom = SDL_getenv("RACERS_CONTROLS");
		if (custom && custom[0]) {
			SDL_strlcpy(g_path, custom, sizeof(g_path));
		}
		else {
			const char* base = SDL_GetBasePath();
			SDL_snprintf(g_path, sizeof(g_path), "%scontrols.ini", base ? base : "");
		}
	}

	size_t size = 0;
	char* text = static_cast<char*>(SDL_LoadFile(g_path, &size));
	if (text) {
		Parse(text, p_ini);
		SDL_free(text);
		return;
	}

	SDL_IOStream* stream = SDL_IOFromFile(g_path, "wb");
	if (stream) {
		SDL_WriteIO(stream, c_defaultIni, SDL_strlen(c_defaultIni));
		SDL_CloseIO(stream);
		SDL_Log("Controls: wrote default %s", g_path);
	}
	else {
		SDL_Log("Controls: could not write %s, using defaults", g_path);
	}
}

LegoU32 PadEvent(const char* p_name)
{
	static const struct {
		const char* m_name;
		LegoU32 m_event;
	} c_pad[] = {
		{"A", InputDevice::c_sourceJoystickButton | 0},
		{"B", InputDevice::c_sourceJoystickButton | 1},
		{"X", InputDevice::c_sourceJoystickButton | 2},
		{"Y", InputDevice::c_sourceJoystickButton | 3},
		{"Back", InputDevice::c_sourceJoystickButton | 4},
		{"Select", InputDevice::c_sourceJoystickButton | 4},
		{"Guide", InputDevice::c_sourceJoystickButton | 5},
		{"Start", InputDevice::c_sourceJoystickButton | 6},
		{"LS", InputDevice::c_sourceJoystickButton | 7},
		{"RS", InputDevice::c_sourceJoystickButton | 8},
		{"LB", InputDevice::c_sourceJoystickButton | 9},
		{"RB", InputDevice::c_sourceJoystickButton | 10},
		{"StickRight", InputDevice::c_sourceJoystickAxisButton | 0},
		{"StickLeft", InputDevice::c_sourceJoystickAxisButton | 1},
		{"RT", InputDevice::c_sourceJoystickAxisButton | 4},
		{"LT", InputDevice::c_sourceJoystickAxisButton | 5},
	};

	for (size_t i = 0; i < sizeof(c_pad) / sizeof(c_pad[0]); i++) {
		if (SDL_strcasecmp(p_name, c_pad[i].m_name) == 0) {
			return c_pad[i].m_event;
		}
	}
	if (p_name[0] && SDL_strcasecmp(p_name, "none") != 0) {
		SDL_Log("Controls: unknown gamepad button '%s'", p_name);
	}
	return 0;
}

LegoU32 KeyEvent(const char* p_name)
{
	if (!p_name[0] || SDL_strcasecmp(p_name, "none") == 0) {
		return 0;
	}
	int dik = MiniwinInput_DikFromKeyName(p_name);
	if (!dik) {
		SDL_Log("Controls: unknown key '%s'", p_name);
		return 0;
	}
	return InputDevice::c_sourceKeyboard | (LegoU32) dik;
}

} // namespace

const char* CoopControls_GetPath()
{
	return g_path;
}

namespace
{
LegoS32 g_sessionPresets[c_coopMaxPlayers] = {c_coopPresetNone, c_coopPresetNone, c_coopPresetNone, c_coopPresetNone};
CoopVersusSettings g_versusSettings = {2, FALSE, 3};

void UpperCopy(char* p_out, size_t p_size, const char* p_in)
{
	size_t i = 0;
	for (; p_in[i] && i + 1 < p_size; i++) {
		p_out[i] = static_cast<char>(SDL_toupper(static_cast<unsigned char>(p_in[i])));
	}
	p_out[i] = '\0';
}
} // namespace

CoopVersusSettings* CoopVersus_GetSettings()
{
	return &g_versusSettings;
}

void CoopControls_SetSessionPreset(LegoU32 p_playerIndex, LegoS32 p_preset)
{
	if (p_playerIndex < c_coopMaxPlayers) {
		g_sessionPresets[p_playerIndex] = p_preset;
	}
}

void CoopControls_GetPresetEvents(LegoS32 p_preset, LegoU32 p_events[c_coopActionCount])
{
	Ini ini;
	LoadIni(&ini);
	for (LegoS32 i = 0; i < c_coopActionCount; i++) {
		if (p_preset >= c_coopPresetKeyboard1 && p_preset < c_coopPresetCount) {
			p_events[i] = KeyEvent(ini.m_values[2 + p_preset - c_coopPresetKeyboard1][i]);
		}
		else if (p_preset >= c_coopPresetGamepad1 && p_preset < c_coopPresetKeyboard1) {
			p_events[i] = PadEvent(ini.m_values[1][i]);
		}
		else {
			p_events[i] = 0;
		}
	}
	if (p_preset >= c_coopPresetGamepad1 && p_preset < c_coopPresetKeyboard1) {
		// Steering is always on the left stick.
		if (!p_events[c_coopActionLeft]) {
			p_events[c_coopActionLeft] = InputDevice::c_sourceJoystickAxisButton | 1;
		}
		if (!p_events[c_coopActionRight]) {
			p_events[c_coopActionRight] = InputDevice::c_sourceJoystickAxisButton | 0;
		}
	}
}

void CoopControls_GetActionName(LegoS32 p_preset, LegoS32 p_action, char* p_buffer, LegoU32 p_size)
{
	Ini ini;
	LoadIni(&ini);
	LegoBool32 pad = p_preset >= c_coopPresetGamepad1 && p_preset < c_coopPresetKeyboard1;
	LegoS32 section = pad ? 1 : 2 + (p_preset - c_coopPresetKeyboard1);
	if (section < 1 || section > 5 || p_action < 0 || p_action >= c_coopActionCount) {
		p_buffer[0] = '\0';
		return;
	}
	UpperCopy(p_buffer, p_size, ini.m_values[section][p_action][0] ? ini.m_values[section][p_action] : "NONE");
}

void CoopControls_DescribePreset(LegoS32 p_preset, char* p_buffer, LegoU32 p_size)
{
	Ini ini;
	LoadIni(&ini);
	char names[c_coopActionCount][48];
	LegoBool32 pad = p_preset >= c_coopPresetGamepad1 && p_preset < c_coopPresetKeyboard1;
	LegoS32 section = pad ? 1 : 2 + (p_preset - c_coopPresetKeyboard1);
	if (section < 1 || section > 5) {
		p_buffer[0] = '\0';
		return;
	}
	for (LegoS32 i = 0; i < c_coopActionCount; i++) {
		UpperCopy(names[i], sizeof(names[i]), ini.m_values[section][i][0] ? ini.m_values[section][i] : "NONE");
	}
	if (pad) {
		SDL_snprintf(
			p_buffer,
			p_size,
			"STEER STICK  GO %s  BRAKE %s\nITEM %s  SLIDE %s",
			names[c_coopActionAccelerate],
			names[c_coopActionBrake],
			names[c_coopActionPowerup],
			names[c_coopActionSlide]
		);
	}
	else {
		SDL_snprintf(
			p_buffer,
			p_size,
			"STEER %s %s  GO %s  BRAKE %s\nITEM %s  SLIDE %s",
			names[c_coopActionLeft],
			names[c_coopActionRight],
			names[c_coopActionAccelerate],
			names[c_coopActionBrake],
			names[c_coopActionPowerup],
			names[c_coopActionSlide]
		);
	}
}

LegoBool32 CoopControls_GetBinding(
	InputManager* p_inputManager,
	LegoU32 p_playerIndex,
	InputBindingState::Entry* p_entry
)
{
	Ini ini;
	LoadIni(&ini);

	if (p_playerIndex >= c_coopMaxPlayers) {
		return FALSE;
	}

	SDL_memset(p_entry, 0, sizeof(*p_entry));

	const char* device = ini.m_players[p_playerIndex];
	LegoS32 padSlot = -1;
	LegoS32 keyboardSet = -1;
	LegoS32 preset = g_sessionPresets[p_playerIndex];

	if (preset >= c_coopPresetKeyboard1 && preset < c_coopPresetCount) {
		keyboardSet = preset - c_coopPresetKeyboard1;
	}
	else if (preset >= c_coopPresetGamepad1 && preset < c_coopPresetKeyboard1) {
		padSlot = preset;
	}
	else if (SDL_strncasecmp(device, "gamepad", 7) == 0 && device[7] >= '1' && device[7] <= '4') {
		padSlot = device[7] - '1';
	}
	else if (SDL_strncasecmp(device, "keyboard", 8) == 0 && device[8] >= '1' && device[8] <= '4') {
		keyboardSet = device[8] - '1';
	}
	else {
		if (SDL_strcasecmp(device, "auto") != 0 && device[0]) {
			SDL_Log("Controls: unknown device '%s' for player %u, using auto", device, p_playerIndex + 1);
		}
		if (MiniwinGamepad_ConnectedMask() & (1 << p_playerIndex)) {
			padSlot = p_playerIndex;
		}
		else {
			keyboardSet = p_playerIndex;
		}
	}

	if (padSlot >= 0) {
		JoystickDevice* joystick = p_inputManager ? p_inputManager->FindJoystickByDeviceId(padSlot) : NULL;
		if (joystick) {
			p_entry->m_deviceType = DIDEVTYPE_JOYSTICK;
			p_entry->m_deviceSubType = static_cast<LegoU8>(joystick->GetDeviceSubType());
			p_entry->m_deviceId = static_cast<LegoU8>(padSlot);
			for (LegoS32 i = 0; i < 9; i++) {
				p_entry->m_events[i] = PadEvent(ini.m_values[1][i]);
			}
			SDL_Log("Controls: player %u uses gamepad %d", p_playerIndex + 1, padSlot + 1);
			return TRUE;
		}
		keyboardSet = p_playerIndex;
	}

	KeyboardDevice* keyboard = p_inputManager ? p_inputManager->GetKeyboard() : NULL;
	p_entry->m_deviceType = DIDEVTYPE_KEYBOARD;
	p_entry->m_deviceSubType = keyboard ? static_cast<LegoU8>(keyboard->GetDeviceSubType()) : 0;
	p_entry->m_deviceId = 0;
	for (LegoS32 i = 0; i < 9; i++) {
		p_entry->m_events[i] = KeyEvent(ini.m_values[2 + keyboardSet][i]);
	}
	SDL_Log("Controls: player %u uses keyboard set %d", p_playerIndex + 1, keyboardSet + 1);
	return keyboard != NULL;
}

// ---------------------------------------------------------------------------
// LegoRacers4K Options > Font (settings.ini next to the exe).

namespace
{
const char* const c_fontSetFolders[c_fontSetCount] = {"", "legomovie", "clicky2", "clicky3"};
const char* const c_fontSetLabels[c_fontSetCount] = {"ORIGINAL", "LEGO MOVIE", "CLICKY BRICKS 2", "CLICKY BRICKS 3"};
LegoS32 g_fontIndex = 0;
LegoBool32 g_fontChanged = FALSE;

void SettingsPath(char* p_path, size_t p_size)
{
	const char* base = SDL_GetBasePath();
	SDL_snprintf(p_path, p_size, "%ssettings.ini", base ? base : "");
}
} // namespace

void GameSettings_Load()
{
	char path[1024];
	SettingsPath(path, sizeof(path));
	size_t size = 0;
	char* text = static_cast<char*>(SDL_LoadFile(path, &size));
	if (text) {
		const char* line = SDL_strstr(text, "font=");
		if (line) {
			line += 5;
			for (LegoS32 i = 1; i < c_fontSetCount; i++) {
				size_t length = SDL_strlen(c_fontSetFolders[i]);
				if (SDL_strncmp(line, c_fontSetFolders[i], length) == 0 &&
					(line[length] == '\0' || line[length] == '\r' || line[length] == '\n' || line[length] == ' ')) {
					g_fontIndex = i;
				}
			}
		}
		SDL_free(text);
	}
	MiniwinHdSetFontSet(c_fontSetFolders[g_fontIndex]);
}

LegoS32 GameSettings_GetFontIndex()
{
	return g_fontIndex;
}

void GameSettings_SetFontIndex(LegoS32 p_index)
{
	if (p_index < 0 || p_index >= c_fontSetCount || p_index == g_fontIndex) {
		return;
	}
	g_fontIndex = p_index;
	g_fontChanged = TRUE;
	MiniwinHdSetFontSet(c_fontSetFolders[g_fontIndex]);

	char path[1024];
	SettingsPath(path, sizeof(path));
	char text[128];
	SDL_snprintf(text, sizeof(text), "[display]\nfont=%s\n", g_fontIndex ? c_fontSetFolders[g_fontIndex] : "original");
	SDL_SaveFile(path, text, SDL_strlen(text));
}

const char* GameSettings_GetFontLabel(LegoS32 p_index)
{
	return p_index >= 0 && p_index < c_fontSetCount ? c_fontSetLabels[p_index] : "";
}

LegoBool32 GameSettings_TakeFontChanged()
{
	LegoBool32 changed = g_fontChanged;
	g_fontChanged = FALSE;
	return changed;
}
