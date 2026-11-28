// [library:input] DirectInput 5 emulation over SDL3. Buffered keyboard/mouse device
// data is fed from the game thread's event drain (MiniwinInput_HandleEvent, called by
// Win32GolApp::Tick for every forwarded SDL event); gamepads are polled through the
// SDL3 gamepad API; the GUID_Sine force-feedback effect maps to gamepad rumble.

#include "miniwin.h"

#include <miniwin/dinput.h>
#include <miniwin/miniwinapp.h>
#include <stdio.h>
#include <string.h>

// --- SDL scancode -> DirectInput DIK_* (PC scan code set 1) ---

static Uint8 ScancodeToDik(SDL_Scancode p_scancode)
{
	switch (p_scancode) {
	case SDL_SCANCODE_ESCAPE:
		return 0x01;
	case SDL_SCANCODE_1:
		return 0x02;
	case SDL_SCANCODE_2:
		return 0x03;
	case SDL_SCANCODE_3:
		return 0x04;
	case SDL_SCANCODE_4:
		return 0x05;
	case SDL_SCANCODE_5:
		return 0x06;
	case SDL_SCANCODE_6:
		return 0x07;
	case SDL_SCANCODE_7:
		return 0x08;
	case SDL_SCANCODE_8:
		return 0x09;
	case SDL_SCANCODE_9:
		return 0x0A;
	case SDL_SCANCODE_0:
		return 0x0B;
	case SDL_SCANCODE_MINUS:
		return 0x0C;
	case SDL_SCANCODE_EQUALS:
		return 0x0D;
	case SDL_SCANCODE_BACKSPACE:
		return 0x0E;
	case SDL_SCANCODE_TAB:
		return 0x0F;
	case SDL_SCANCODE_Q:
		return 0x10;
	case SDL_SCANCODE_W:
		return 0x11;
	case SDL_SCANCODE_E:
		return 0x12;
	case SDL_SCANCODE_R:
		return 0x13;
	case SDL_SCANCODE_T:
		return 0x14;
	case SDL_SCANCODE_Y:
		return 0x15;
	case SDL_SCANCODE_U:
		return 0x16;
	case SDL_SCANCODE_I:
		return 0x17;
	case SDL_SCANCODE_O:
		return 0x18;
	case SDL_SCANCODE_P:
		return 0x19;
	case SDL_SCANCODE_LEFTBRACKET:
		return 0x1A;
	case SDL_SCANCODE_RIGHTBRACKET:
		return 0x1B;
	case SDL_SCANCODE_RETURN:
		return 0x1C;
	case SDL_SCANCODE_LCTRL:
		return 0x1D;
	case SDL_SCANCODE_A:
		return 0x1E;
	case SDL_SCANCODE_S:
		return 0x1F;
	case SDL_SCANCODE_D:
		return 0x20;
	case SDL_SCANCODE_F:
		return 0x21;
	case SDL_SCANCODE_G:
		return 0x22;
	case SDL_SCANCODE_H:
		return 0x23;
	case SDL_SCANCODE_J:
		return 0x24;
	case SDL_SCANCODE_K:
		return 0x25;
	case SDL_SCANCODE_L:
		return 0x26;
	case SDL_SCANCODE_SEMICOLON:
		return 0x27;
	case SDL_SCANCODE_APOSTROPHE:
		return 0x28;
	case SDL_SCANCODE_GRAVE:
		return 0x29;
	case SDL_SCANCODE_LSHIFT:
		return 0x2A;
	case SDL_SCANCODE_BACKSLASH:
		return 0x2B;
	case SDL_SCANCODE_Z:
		return 0x2C;
	case SDL_SCANCODE_X:
		return 0x2D;
	case SDL_SCANCODE_C:
		return 0x2E;
	case SDL_SCANCODE_V:
		return 0x2F;
	case SDL_SCANCODE_B:
		return 0x30;
	case SDL_SCANCODE_N:
		return 0x31;
	case SDL_SCANCODE_M:
		return 0x32;
	case SDL_SCANCODE_COMMA:
		return 0x33;
	case SDL_SCANCODE_PERIOD:
		return 0x34;
	case SDL_SCANCODE_SLASH:
		return 0x35;
	case SDL_SCANCODE_RSHIFT:
		return 0x36;
	case SDL_SCANCODE_KP_MULTIPLY:
		return 0x37;
	case SDL_SCANCODE_LALT:
		return 0x38;
	case SDL_SCANCODE_SPACE:
		return 0x39;
	case SDL_SCANCODE_CAPSLOCK:
		return 0x3A;
	case SDL_SCANCODE_F1:
		return 0x3B;
	case SDL_SCANCODE_F2:
		return 0x3C;
	case SDL_SCANCODE_F3:
		return 0x3D;
	case SDL_SCANCODE_F4:
		return 0x3E;
	case SDL_SCANCODE_F5:
		return 0x3F;
	case SDL_SCANCODE_F6:
		return 0x40;
	case SDL_SCANCODE_F7:
		return 0x41;
	case SDL_SCANCODE_F8:
		return 0x42;
	case SDL_SCANCODE_F9:
		return 0x43;
	case SDL_SCANCODE_F10:
		return 0x44;
	case SDL_SCANCODE_NUMLOCKCLEAR:
		return 0x45;
	case SDL_SCANCODE_SCROLLLOCK:
		return 0x46;
	case SDL_SCANCODE_KP_7:
		return 0x47;
	case SDL_SCANCODE_KP_8:
		return 0x48;
	case SDL_SCANCODE_KP_9:
		return 0x49;
	case SDL_SCANCODE_KP_MINUS:
		return 0x4A;
	case SDL_SCANCODE_KP_4:
		return 0x4B;
	case SDL_SCANCODE_KP_5:
		return 0x4C;
	case SDL_SCANCODE_KP_6:
		return 0x4D;
	case SDL_SCANCODE_KP_PLUS:
		return 0x4E;
	case SDL_SCANCODE_KP_1:
		return 0x4F;
	case SDL_SCANCODE_KP_2:
		return 0x50;
	case SDL_SCANCODE_KP_3:
		return 0x51;
	case SDL_SCANCODE_KP_0:
		return 0x52;
	case SDL_SCANCODE_KP_PERIOD:
		return 0x53;
	case SDL_SCANCODE_F11:
		return 0x57;
	case SDL_SCANCODE_F12:
		return 0x58;
	case SDL_SCANCODE_KP_ENTER:
		return 0x9C;
	case SDL_SCANCODE_RCTRL:
		return 0x9D;
	case SDL_SCANCODE_KP_DIVIDE:
		return 0xB5;
	case SDL_SCANCODE_PRINTSCREEN:
		return 0xB7;
	case SDL_SCANCODE_RALT:
		return 0xB8;
	case SDL_SCANCODE_PAUSE:
		return 0xC5;
	case SDL_SCANCODE_HOME:
		return 0xC7;
	case SDL_SCANCODE_UP:
		return 0xC8;
	case SDL_SCANCODE_PAGEUP:
		return 0xC9;
	case SDL_SCANCODE_LEFT:
		return 0xCB;
	case SDL_SCANCODE_RIGHT:
		return 0xCD;
	case SDL_SCANCODE_END:
		return 0xCF;
	case SDL_SCANCODE_DOWN:
		return 0xD0;
	case SDL_SCANCODE_PAGEDOWN:
		return 0xD1;
	case SDL_SCANCODE_INSERT:
		return 0xD2;
	case SDL_SCANCODE_DELETE:
		return 0xD3;
	case SDL_SCANCODE_LGUI:
		return 0xDB;
	case SDL_SCANCODE_RGUI:
		return 0xDC;
	case SDL_SCANCODE_APPLICATION:
		return 0xDD;
	default:
		return 0;
	}
}

static SDL_Scancode DikToScancode(Uint8 p_dik)
{
	for (int i = 0; i < SDL_SCANCODE_COUNT; i++) {
		if (ScancodeToDik((SDL_Scancode) i) == p_dik) {
			return (SDL_Scancode) i;
		}
	}
	return SDL_SCANCODE_UNKNOWN;
}

// --- Buffered device data rings (all access is on the game thread: the event drain
// pushes, PollDevices' GetDeviceData pops) ---

struct InputRing {
	DIDEVICEOBJECTDATA m_items[512];
	Uint32 m_head = 0;
	Uint32 m_count = 0;

	void Push(DWORD p_ofs, DWORD p_data)
	{
		if (m_count == SDL_arraysize(m_items)) {
			m_head = (m_head + 1) % SDL_arraysize(m_items);
			m_count--;
		}
		DIDEVICEOBJECTDATA& item = m_items[(m_head + m_count) % SDL_arraysize(m_items)];
		item.dwOfs = p_ofs;
		item.dwData = p_data;
		item.dwTimeStamp = SDL_GetTicks();
		item.dwSequence = 0;
		m_count++;
	}

	bool Pop(DIDEVICEOBJECTDATA& p_out)
	{
		if (!m_count) {
			return false;
		}
		p_out = m_items[m_head];
		m_head = (m_head + 1) % SDL_arraysize(m_items);
		m_count--;
		return true;
	}
};

static InputRing g_keyboardRing;
static InputRing g_mouseRing;

static float g_mouseScaleX = 1.0f;
static float g_mouseScaleY = 1.0f;

void MiniwinInput_SetMouseScale(float p_scaleX, float p_scaleY)
{
	g_mouseScaleX = p_scaleX;
	g_mouseScaleY = p_scaleY;
}

void MiniwinInput_InjectMouseButton(bool p_down)
{
	g_mouseRing.Push(DIMOFS_BUTTON0, p_down ? 0x80 : 0x00);
}

void MiniwinInput_InjectMouseMotion(long p_dx, long p_dy)
{
	if (p_dx != 0) {
		g_mouseRing.Push(DIMOFS_X, (DWORD) (LONG) p_dx);
	}
	if (p_dy != 0) {
		g_mouseRing.Push(DIMOFS_Y, (DWORD) (LONG) p_dy);
	}
}

void MiniwinInput_HandleEvent(const SDL_Event& p_event)
{
	switch (p_event.type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		if (p_event.key.repeat) {
			break; // DirectInput buffered keyboards do not auto-repeat.
		}
		Uint8 dik = ScancodeToDik(p_event.key.scancode);
		if (dik) {
			g_keyboardRing.Push(dik, p_event.type == SDL_EVENT_KEY_DOWN ? 0x80 : 0x00);
		}
		break;
	}
	case SDL_EVENT_MOUSE_MOTION: {
		// Scale the raw window-space delta into the game's cursor space (consumed by the menu
		// cursor's relative accumulation and by widget drag handlers). The fractional remainder
		// is carried across events so slow motion is not lost to integer rounding.
		static float accumX = 0.0f;
		static float accumY = 0.0f;
		if (SDL_getenv("RACERS_INPUT_LOG")) {
			SDL_Log(
				"motion xrel=%f yrel=%f scale=%f,%f",
				p_event.motion.xrel,
				p_event.motion.yrel,
				g_mouseScaleX,
				g_mouseScaleY
			);
		}
		accumX += p_event.motion.xrel * g_mouseScaleX;
		accumY += p_event.motion.yrel * g_mouseScaleY;
		LONG dx = (LONG) accumX;
		LONG dy = (LONG) accumY;
		accumX -= (float) dx;
		accumY -= (float) dy;
		if (dx != 0) {
			g_mouseRing.Push(DIMOFS_X, (DWORD) dx);
		}
		if (dy != 0) {
			g_mouseRing.Push(DIMOFS_Y, (DWORD) dy);
		}
		break;
	}
	case SDL_EVENT_MOUSE_WHEEL:
		if (p_event.wheel.y != 0.0f) {
			g_mouseRing.Push(DIMOFS_Z, (DWORD) (LONG) (p_event.wheel.y * 120.0f));
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		DWORD ofs;
		switch (p_event.button.button) {
		case SDL_BUTTON_LEFT:
			ofs = DIMOFS_BUTTON0;
			break;
		case SDL_BUTTON_RIGHT:
			ofs = DIMOFS_BUTTON1;
			break;
		case SDL_BUTTON_MIDDLE:
			ofs = DIMOFS_BUTTON2;
			break;
		case SDL_BUTTON_X1:
			ofs = DIMOFS_BUTTON3;
			break;
		default:
			return;
		}
		g_mouseRing.Push(ofs, p_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 0x80 : 0x00);
		break;
	}
	default:
		break;
	}
}

int MiniwinInput_DikFromKeyName(const char* p_name)
{
	SDL_Scancode scancode = SDL_GetScancodeFromName(p_name);
	if (scancode == SDL_SCANCODE_UNKNOWN) {
		return 0;
	}
	return ScancodeToDik(scancode);
}

const char* MiniwinInput_KeyNameFromDik(int p_dik)
{
	if (p_dik <= 0 || p_dik > 0xff) {
		return "";
	}
	SDL_Scancode scancode = DikToScancode((Uint8) p_dik);
	return scancode == SDL_SCANCODE_UNKNOWN ? "" : SDL_GetScancodeName(scancode);
}

// --- Gamepad slots (4-player couch co-op, hot-plug) ---
// The game enumerates joysticks once at startup and keeps the device objects for
// the whole session, so DirectInput sees four fixed virtual pads ("Gamepad 1".."4").
// Each slot is bound to whichever SDL gamepad is plugged in: a new pad takes the
// lowest free slot, and a pad that is unplugged frees its slot for the next one.
// Slots are opened/closed on the main thread (SDL_AppEvent) and read under a mutex
// from the game thread.

static const int c_gamepadSlotCount = MINIWIN_GAMEPAD_SLOTS;

struct GamepadSlot {
	SDL_JoystickID m_id = 0;
	SDL_Gamepad* m_pad = nullptr;
};

static GamepadSlot g_gamepadSlots[MINIWIN_GAMEPAD_SLOTS];
static SDL_Mutex* g_gamepadMutex = nullptr;

static SDL_Mutex* GamepadMutex()
{
	if (!g_gamepadMutex) {
		g_gamepadMutex = SDL_CreateMutex();
	}
	return g_gamepadMutex;
}

// Main thread only.
static void GamepadSlots_Add(SDL_JoystickID p_id)
{
	for (int i = 0; i < c_gamepadSlotCount; i++) {
		if (g_gamepadSlots[i].m_id == p_id) {
			return;
		}
	}
	for (int i = 0; i < c_gamepadSlotCount; i++) {
		if (!g_gamepadSlots[i].m_pad) {
			SDL_Gamepad* pad = SDL_OpenGamepad(p_id);
			if (!pad) {
				return;
			}
			SDL_LockMutex(GamepadMutex());
			g_gamepadSlots[i].m_id = p_id;
			g_gamepadSlots[i].m_pad = pad;
			SDL_UnlockMutex(GamepadMutex());
			const char* name = SDL_GetGamepadName(pad);
			SDL_Log("Gamepad %d connected: %s", i + 1, name ? name : "?");
			SDL_SetGamepadPlayerIndex(pad, i);
			return;
		}
	}
	SDL_Log("Gamepad ignored (all %d slots in use)", c_gamepadSlotCount);
}

// Main thread only.
static void GamepadSlots_Remove(SDL_JoystickID p_id)
{
	for (int i = 0; i < c_gamepadSlotCount; i++) {
		if (g_gamepadSlots[i].m_id == p_id && g_gamepadSlots[i].m_pad) {
			SDL_LockMutex(GamepadMutex());
			SDL_Gamepad* pad = g_gamepadSlots[i].m_pad;
			g_gamepadSlots[i].m_pad = nullptr;
			g_gamepadSlots[i].m_id = 0;
			SDL_UnlockMutex(GamepadMutex());
			SDL_CloseGamepad(pad);
			SDL_Log("Gamepad %d disconnected", i + 1);
			return;
		}
	}
}

static void GamepadSlots_ScanAll()
{
	int count = 0;
	SDL_JoystickID* ids = SDL_GetGamepads(&count);
	if (ids) {
		for (int i = 0; i < count; i++) {
			GamepadSlots_Add(ids[i]);
		}
		SDL_free(ids);
	}
}

void MiniwinGamepad_HandleMainThreadEvent(const SDL_Event& p_event)
{
	switch (p_event.type) {
	case SDL_EVENT_GAMEPAD_ADDED:
		GamepadSlots_Add(p_event.gdevice.which);
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		GamepadSlots_Remove(p_event.gdevice.which);
		break;
	default:
		break;
	}
}

struct PadScriptStep {
	SDL_Joystick* m_joystick;
	int m_button;
	Uint64 m_start;
	Uint64 m_end;
	int m_state;
};
static SDL_Joystick* g_virtualPads[MINIWIN_GAMEPAD_SLOTS];
static PadScriptStep g_padScript[32];
static size_t g_padScriptCount = 0;

void MiniwinGamepad_Init()
{
	// RACERS_VIRTUAL_PADS=N attaches N SDL virtual gamepads (automated tests).
	const char* virtualPads = SDL_getenv("RACERS_VIRTUAL_PADS");
	if (virtualPads) {
		int count = SDL_atoi(virtualPads);
		for (int i = 0; i < count && i < c_gamepadSlotCount; i++) {
			SDL_VirtualJoystickDesc desc;
			SDL_INIT_INTERFACE(&desc);
			desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
			desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
			desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
			desc.name = "Virtual Test Pad";
			SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
			g_virtualPads[i] = id ? SDL_OpenJoystick(id) : nullptr;
		}

		// RACERS_AUTOPAD="ms:pad:button:holdMs,..." presses SDL gamepad button numbers on
		// the virtual pads (pad 1-4) for scripted tests.
		const char* script = SDL_getenv("RACERS_AUTOPAD");
		while (script && *script && g_padScriptCount < SDL_arraysize(g_padScript)) {
			unsigned ms = 0, pad = 0, button = 0, hold = 100;
			if (SDL_sscanf(script, "%u:%u:%u:%u", &ms, &pad, &button, &hold) >= 3 && pad >= 1 && pad <= 4 &&
				g_virtualPads[pad - 1]) {
				g_padScript[g_padScriptCount++] = {g_virtualPads[pad - 1], (int) button, ms, ms + hold, 0};
			}
			const char* comma = SDL_strchr(script, ',');
			script = comma ? comma + 1 : nullptr;
		}
		if (g_padScriptCount) {
			SDL_AddTimer(
				10,
				[](void*, SDL_TimerID, Uint32 p_interval) -> Uint32 {
					Uint64 now = SDL_GetTicks();
					for (size_t i = 0; i < g_padScriptCount; i++) {
						PadScriptStep& step = g_padScript[i];
						int state = now >= step.m_start && now < step.m_end ? 1 : 0;
						if (state != step.m_state) {
							step.m_state = state;
							SDL_SetJoystickVirtualButton(step.m_joystick, step.m_button, state != 0);
						}
					}
					return p_interval;
				},
				nullptr
			);
		}
	}
	GamepadSlots_ScanAll();
}

int MiniwinGamepad_ConnectedMask()
{
	int mask = 0;
	SDL_LockMutex(GamepadMutex());
	for (int i = 0; i < c_gamepadSlotCount; i++) {
		if (g_gamepadSlots[i].m_pad) {
			mask |= 1 << i;
		}
	}
	SDL_UnlockMutex(GamepadMutex());
	return mask;
}

bool MiniwinGamepad_GetName(int p_slot, char* p_buffer, size_t p_size)
{
	bool connected = false;
	SDL_LockMutex(GamepadMutex());
	if (p_slot >= 0 && p_slot < c_gamepadSlotCount && g_gamepadSlots[p_slot].m_pad) {
		const char* name = SDL_GetGamepadName(g_gamepadSlots[p_slot].m_pad);
		SDL_strlcpy(p_buffer, name ? name : "Gamepad", p_size);
		connected = true;
	}
	SDL_UnlockMutex(GamepadMutex());
	return connected;
}

// --- Joystick instance GUIDs (one per gamepad slot) ---

static GUID JoystickGuid(int p_slot)
{
	GUID guid = {};
	guid.Data1 = 0x52435253; // 'RCRS'
	guid.Data2 = (Uint16) (p_slot + 1);
	guid.Data3 = 0;
	memcpy(guid.Data4, "SDLPAD\0", 8);
	return guid;
}

static int JoystickSlotFromGuid(const GUID& p_guid)
{
	if (p_guid.Data1 != 0x52435253 || p_guid.Data2 < 1 || p_guid.Data2 > c_gamepadSlotCount) {
		return -1;
	}
	return (int) p_guid.Data2 - 1;
}

// The gamepad axes exposed to the game: left stick = X/Y, right stick = Rx/Ry, and
// the trigger pair combined into Z (right pulls positive, like a combined pedal axis).
static const struct {
	const GUID* m_guid;
	DWORD m_offset;
	const char* m_name;
} g_joystickAxes[] = {
	{&GUID_XAxis, DIJOFS_X, "Left Stick X"},
	{&GUID_YAxis, DIJOFS_Y, "Left Stick Y"},
	{&GUID_ZAxis, DIJOFS_Z, "Triggers"},
	{&GUID_RxAxis, DIJOFS_RX, "Right Stick X"},
	{&GUID_RyAxis, DIJOFS_RY, "Right Stick Y"},
};

struct MiniwinInputDevice;

struct MiniwinInputEffect : public IDirectInputEffect {
	MiniwinInputDevice* m_device;
	DWORD m_magnitude = 0; // DirectInput units, 0..10000
	bool m_playing = false;

	explicit MiniwinInputEffect(MiniwinInputDevice* p_device, DWORD p_magnitude)
		: m_device(p_device), m_magnitude(p_magnitude)
	{
	}

	HRESULT Start(DWORD dwIterations, DWORD dwFlags) override;
	HRESULT Stop() override;
	HRESULT SetParameters(LPCDIEFFECT peff, DWORD dwFlags) override;
};

struct MiniwinInputDevice : public IDirectInputDevice2A {
	enum class Kind {
		Keyboard,
		Mouse,
		Joystick,
	};

	Kind m_kind;
	bool m_acquired = false;
	int m_padSlot = -1;
	DWORD m_deadZone = 0; // DirectInput units, 0..10000
	LONG m_rangeMin = -1000;
	LONG m_rangeMax = 1000;
	int m_nameBudget = 0; // remaining wchars in the game's control-name buffer

	explicit MiniwinInputDevice(Kind p_kind) : m_kind(p_kind) {}

	HRESULT QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (ppvObject && riid == IID_IDirectInputDevice2A) {
			AddRef();
			*ppvObject = this;
			return DI_OK;
		}
		if (ppvObject) {
			*ppvObject = nullptr;
		}
		return E_NOINTERFACE;
	}

	HRESULT GetDeviceInfo(LPDIDEVICEINSTANCEA pdidi) override
	{
		if (!pdidi) {
			return DIERR_GENERIC;
		}

		DWORD size = pdidi->dwSize;
		memset(pdidi, 0, sizeof(DIDEVICEINSTANCEA));
		pdidi->dwSize = size;

		switch (m_kind) {
		case Kind::Keyboard:
			pdidi->dwDevType = DIDEVTYPE_KEYBOARD;
			strcpy(pdidi->tszProductName, "Keyboard");
			strcpy(pdidi->tszInstanceName, "Keyboard");
			pdidi->guidInstance = GUID_SysKeyboard;
			break;
		case Kind::Mouse:
			pdidi->dwDevType = DIDEVTYPE_MOUSE;
			strcpy(pdidi->tszProductName, "Mouse");
			strcpy(pdidi->tszInstanceName, "Mouse");
			pdidi->guidInstance = GUID_SysMouse;
			break;
		case Kind::Joystick: {
			pdidi->dwDevType = DIDEVTYPE_JOYSTICK;
			SDL_snprintf(pdidi->tszProductName, sizeof(pdidi->tszProductName), "Gamepad %d", m_padSlot + 1);
			SDL_strlcpy(pdidi->tszInstanceName, pdidi->tszProductName, sizeof(pdidi->tszInstanceName));
			pdidi->guidInstance = JoystickGuid(m_padSlot);
			break;
		}
		}

		return DI_OK;
	}

	HRESULT GetCapabilities(LPDIDEVCAPS lpDIDevCaps) override
	{
		if (!lpDIDevCaps) {
			return DIERR_GENERIC;
		}

		DWORD size = lpDIDevCaps->dwSize;
		memset(lpDIDevCaps, 0, sizeof(DIDEVCAPS));
		lpDIDevCaps->dwSize = size;

		switch (m_kind) {
		case Kind::Keyboard:
			lpDIDevCaps->dwButtons = 256;
			break;
		case Kind::Mouse:
			lpDIDevCaps->dwAxes = 3;
			lpDIDevCaps->dwButtons = 4;
			break;
		case Kind::Joystick:
			lpDIDevCaps->dwAxes = SDL_arraysize(g_joystickAxes);
			lpDIDevCaps->dwButtons = 16;
			break;
		}

		return DI_OK;
	}

	// The game stores control names in a wchar buffer sized 4*(buttons + 4*axes)
	// characters and appends without bounds checks (InputDevice::StoreString), so the
	// combined length of every name we report must stay inside that budget.
	int NameBudget() const
	{
		switch (m_kind) {
		case Kind::Keyboard:
			return 4 * 256;
		case Kind::Mouse:
			return 4 * (4 + 4 * 3);
		case Kind::Joystick:
			return 4 * (16 + 4 * (int) SDL_arraysize(g_joystickAxes));
		}
		return 0;
	}

	bool EmitObject(
		LPDIENUMDEVICEOBJECTSCALLBACKA p_callback,
		LPVOID p_ref,
		const GUID& p_type,
		DWORD p_offset,
		const char* p_name
	)
	{
		DIDEVICEOBJECTINSTANCEA object;
		memset(&object, 0, sizeof(object));
		object.dwSize = sizeof(object);
		object.guidType = p_type;
		object.dwOfs = p_offset;

		int length = (int) SDL_strlen(p_name);
		if (length + 1 > m_nameBudget) {
			length = m_nameBudget - 1; // truncated (or empty once exhausted)
		}
		if (length > 0) {
			memcpy(object.tszName, p_name, (size_t) length);
			m_nameBudget -= length + 1;
		}

		return p_callback(&object, p_ref) != 0;
	}

	HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) override
	{
		if (!lpCallback) {
			return DIERR_GENERIC;
		}

		// Button and axis enumeration share one name buffer in the game; reset the
		// budget on the first (button) pass of a device setup.
		if (dwFlags & DIDFT_BUTTON) {
			// Two trailing zeros terminate the game's name scan.
			m_nameBudget = NameBudget() - 2;
		}

		char name[64];

		if (dwFlags & DIDFT_BUTTON) {
			switch (m_kind) {
			case Kind::Keyboard:
				for (int dik = 1; dik < 256; dik++) {
					SDL_Scancode scancode = DikToScancode((Uint8) dik);
					if (scancode == SDL_SCANCODE_UNKNOWN) {
						continue;
					}

					const char* scancodeName = SDL_GetScancodeName(scancode);
					if (scancodeName && scancodeName[0]) {
						SDL_strlcpy(name, scancodeName, sizeof(name));
					}
					else {
						SDL_snprintf(name, sizeof(name), "Key %02X", dik);
					}
					if (!EmitObject(lpCallback, pvRef, GUID_Button, (DWORD) dik, name)) {
						return DI_OK;
					}
				}
				break;
			case Kind::Mouse:
				for (int i = 0; i < 4; i++) {
					SDL_snprintf(name, sizeof(name), "Button %d", i);
					if (!EmitObject(lpCallback, pvRef, GUID_Button, DIMOFS_BUTTON0 + i, name)) {
						return DI_OK;
					}
				}
				break;
			case Kind::Joystick:
				for (int i = 0; i < 16; i++) {
					SDL_snprintf(name, sizeof(name), "Btn %d", i);
					if (!EmitObject(lpCallback, pvRef, GUID_Button, DIJOFS_BUTTON(i), name)) {
						return DI_OK;
					}
				}
				break;
			}
		}

		if (dwFlags & DIDFT_AXIS) {
			switch (m_kind) {
			case Kind::Keyboard:
				break;
			case Kind::Mouse: {
				static const struct {
					const GUID* m_guid;
					DWORD m_offset;
					const char* m_name;
				} axes[] = {
					{&GUID_XAxis, DIMOFS_X, "X-axis"},
					{&GUID_YAxis, DIMOFS_Y, "Y-axis"},
					{&GUID_ZAxis, DIMOFS_Z, "Wheel"},
				};
				for (size_t i = 0; i < SDL_arraysize(axes); i++) {
					if (!EmitObject(lpCallback, pvRef, *axes[i].m_guid, axes[i].m_offset, axes[i].m_name)) {
						return DI_OK;
					}
				}
				break;
			}
			case Kind::Joystick:
				for (size_t i = 0; i < SDL_arraysize(g_joystickAxes); i++) {
					if (!EmitObject(
							lpCallback,
							pvRef,
							*g_joystickAxes[i].m_guid,
							g_joystickAxes[i].m_offset,
							g_joystickAxes[i].m_name
						)) {
						return DI_OK;
					}
				}
				break;
			}
		}

		return DI_OK;
	}

	HRESULT GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) override
	{
		if (!pdwInOut) {
			return DIERR_GENERIC;
		}

		InputRing* ring = nullptr;
		switch (m_kind) {
		case Kind::Keyboard:
			ring = &g_keyboardRing;
			break;
		case Kind::Mouse:
			ring = &g_mouseRing;
			break;
		case Kind::Joystick:
			break; // Polled via GetDeviceState.
		}

		DWORD requested = *pdwInOut;
		DWORD produced = 0;
		if (ring && rgdod) {
			while (produced < requested && ring->Pop(rgdod[produced])) {
				produced++;
			}
		}

		if (produced && m_kind == Kind::Mouse && SDL_getenv("RACERS_INPUT_LOG")) {
			SDL_Log("mouse GetDeviceData produced %u", (unsigned) produced);
		}
		*pdwInOut = produced;
		return DI_OK;
	}

	LONG ScaleAxis(Sint16 p_value) const
	{
		float normalized = (float) p_value / 32767.0f;
		if (normalized < -1.0f) {
			normalized = -1.0f;
		}

		float deadZone = (float) m_deadZone / 10000.0f;
		float magnitude = normalized < 0.0f ? -normalized : normalized;
		if (magnitude <= deadZone) {
			return (LONG) ((m_rangeMin + m_rangeMax) / 2);
		}
		if (deadZone < 1.0f) {
			magnitude = (magnitude - deadZone) / (1.0f - deadZone);
		}

		float sign = normalized < 0.0f ? -1.0f : 1.0f;
		float center = (float) (m_rangeMin + m_rangeMax) / 2.0f;
		float halfSpan = (float) (m_rangeMax - m_rangeMin) / 2.0f;
		return (LONG) (center + sign * magnitude * halfSpan);
	}

	HRESULT GetDeviceState(DWORD cbData, LPVOID lpvData) override
	{
		if (!lpvData) {
			return DIERR_GENERIC;
		}
		memset(lpvData, 0, cbData);

		if (m_kind != Kind::Joystick || cbData < sizeof(DIJOYSTATE2)) {
			return DI_OK;
		}

		DIJOYSTATE2* state = (DIJOYSTATE2*) lpvData;
		state->lX = state->lY = state->lZ = state->lRx = state->lRy = (m_rangeMin + m_rangeMax) / 2;
		state->rgdwPOV[0] = state->rgdwPOV[1] = state->rgdwPOV[2] = state->rgdwPOV[3] = 0xFFFFFFFF;

		SDL_LockMutex(GamepadMutex());
		SDL_Gamepad* pad = g_gamepadSlots[m_padSlot].m_pad;
		if (pad) {
			ReadGamepad(pad, state);
		}
		SDL_UnlockMutex(GamepadMutex());
		return DI_OK;
	}

	void ReadGamepad(SDL_Gamepad* pad, DIJOYSTATE2* state)
	{
		state->lX = ScaleAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
		state->lY = ScaleAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));

		// D-pad input overrides the left stick independently on each axis.
		// Neither or both opposing directions leave that axis's analog value intact.
		bool left = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
		bool right = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
		bool up = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
		bool down = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
		if (left != right) {
			state->lX = left ? m_rangeMin : m_rangeMax;
		}
		if (up != down) {
			state->lY = up ? m_rangeMin : m_rangeMax;
		}

		state->lRx = ScaleAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX));
		state->lRy = ScaleAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));

		// Combined trigger axis: right trigger pulls positive, left negative.
		Sint16 lt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
		Sint16 rt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
		state->lZ = ScaleAxis((Sint16) ((rt - lt) / 2 * 2));

		state->rgdwPOV[0] = 0xFFFFFFFF;
		state->rgdwPOV[1] = 0xFFFFFFFF;
		state->rgdwPOV[2] = 0xFFFFFFFF;
		state->rgdwPOV[3] = 0xFFFFFFFF;

		for (int i = 0; i < 16; i++) {
			if (i == SDL_GAMEPAD_BUTTON_DPAD_UP || i == SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
				i == SDL_GAMEPAD_BUTTON_DPAD_LEFT || i == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
				// D-pad directions already drive lX/lY. Suppress all four button slots
				// so menu navigation cannot also select an item.
				continue;
			}
			state->rgbButtons[i] = SDL_GetGamepadButton(pad, (SDL_GamepadButton) i) ? 0x80 : 0x00;
		}

		// Triggers double as A (accelerate) and B (brake/reverse) when pulled past halfway.
		if (rt > 16384) {
			state->rgbButtons[SDL_GAMEPAD_BUTTON_SOUTH] = 0x80;
		}
		if (lt > 16384) {
			state->rgbButtons[SDL_GAMEPAD_BUTTON_EAST] = 0x80;
		}
	}

	HRESULT Acquire() override
	{
		m_acquired = true;
		return DI_OK;
	}

	HRESULT Unacquire() override
	{
		m_acquired = false;
		return DI_OK;
	}

	HRESULT Poll() override { return DI_OK; }

	HRESULT SetProperty(const GUID* rguidProp, LPCDIPROPHEADER pdiph) override
	{
		if (!pdiph) {
			return DIERR_GENERIC;
		}

		if (rguidProp == DIPROP_DEADZONE) {
			m_deadZone = ((const DIPROPDWORD*) pdiph)->dwData;
		}
		else if (rguidProp == DIPROP_RANGE) {
			const DIPROPRANGE* range = (const DIPROPRANGE*) pdiph;
			m_rangeMin = range->lMin;
			m_rangeMax = range->lMax;
		}

		return DI_OK;
	}

	HRESULT CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT* ppdeff, IUnknown* punkOuter) override
	{
		if (!ppdeff) {
			return DIERR_GENERIC;
		}
		*ppdeff = nullptr;

		if (m_kind != Kind::Joystick || !(rguid == GUID_Sine)) {
			return DIERR_GENERIC;
		}

		// Rumble support is checked when the effect plays: the pad in this slot can
		// change at any time (hot-plug), so the effect is always created.

		DWORD magnitude = 5000;
		if (lpeff && lpeff->lpvTypeSpecificParams && lpeff->cbTypeSpecificParams >= sizeof(DIPERIODIC)) {
			magnitude = ((const DIPERIODIC*) lpeff->lpvTypeSpecificParams)->dwMagnitude;
		}

		*ppdeff = new MiniwinInputEffect(this, magnitude);
		return DI_OK;
	}
};

HRESULT MiniwinInputEffect::Start(DWORD, DWORD)
{
	m_playing = true;
	Uint16 strength = (Uint16) ((Uint32) m_magnitude * 65535 / 10000);
	SDL_LockMutex(GamepadMutex());
	if (SDL_Gamepad* pad = g_gamepadSlots[m_device->m_padSlot].m_pad) {
		SDL_RumbleGamepad(pad, strength, strength, 600000);
	}
	SDL_UnlockMutex(GamepadMutex());
	return DI_OK;
}

HRESULT MiniwinInputEffect::Stop()
{
	m_playing = false;
	SDL_LockMutex(GamepadMutex());
	if (SDL_Gamepad* pad = g_gamepadSlots[m_device->m_padSlot].m_pad) {
		SDL_RumbleGamepad(pad, 0, 0, 0);
	}
	SDL_UnlockMutex(GamepadMutex());
	return DI_OK;
}

HRESULT MiniwinInputEffect::SetParameters(LPCDIEFFECT peff, DWORD dwFlags)
{
	if ((dwFlags & DIEP_TYPESPECIFICPARAMS) && peff && peff->lpvTypeSpecificParams &&
		peff->cbTypeSpecificParams >= sizeof(DIPERIODIC)) {
		m_magnitude = ((const DIPERIODIC*) peff->lpvTypeSpecificParams)->dwMagnitude;
		if (m_playing) {
			return Start(1, 0);
		}
	}
	return DI_OK;
}

struct MiniwinDirectInput : public IDirectInputA {
	HRESULT CreateDevice(REFGUID rguid, LPDIRECTINPUTDEVICEA* lplpDirectInputDevice, IUnknown* pUnkOuter) override
	{
		if (!lplpDirectInputDevice) {
			return DIERR_GENERIC;
		}
		*lplpDirectInputDevice = nullptr;

		if (rguid == GUID_SysKeyboard) {
			*lplpDirectInputDevice = new MiniwinInputDevice(MiniwinInputDevice::Kind::Keyboard);
			return DI_OK;
		}
		if (rguid == GUID_SysMouse) {
			*lplpDirectInputDevice = new MiniwinInputDevice(MiniwinInputDevice::Kind::Mouse);
			return DI_OK;
		}

		int slot = JoystickSlotFromGuid(rguid);
		if (slot < 0) {
			return DIERR_GENERIC;
		}

		MiniwinInputDevice* device = new MiniwinInputDevice(MiniwinInputDevice::Kind::Joystick);
		device->m_padSlot = slot;

		*lplpDirectInputDevice = device;
		return DI_OK;
	}

	HRESULT EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) override
	{
		if (dwDevType != DIDEVTYPE_JOYSTICK || !lpCallback) {
			return DI_OK;
		}

		// Always four slots, connected or not, so pads can be plugged in mid-game.
		for (int i = 0; i < c_gamepadSlotCount; i++) {
			DIDEVICEINSTANCEA instance;
			memset(&instance, 0, sizeof(instance));
			instance.dwSize = sizeof(instance);
			instance.dwDevType = DIDEVTYPE_JOYSTICK;
			instance.guidInstance = JoystickGuid(i);
			SDL_snprintf(instance.tszProductName, sizeof(instance.tszProductName), "Gamepad %d", i + 1);
			SDL_strlcpy(instance.tszInstanceName, instance.tszProductName, sizeof(instance.tszInstanceName));

			if (!lpCallback(&instance, pvRef)) {
				break;
			}
		}

		return DI_OK;
	}
};

HRESULT DirectInputCreate(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUT* ppDI, IUnknown* punkOuter)
{
	if (!ppDI) {
		return DIERR_GENERIC;
	}

	*ppDI = new MiniwinDirectInput();
	return DI_OK;
}

// --- Base-interface default bodies ---

HRESULT IDirectInputA::CreateDevice(REFGUID, LPDIRECTINPUTDEVICEA*, IUnknown*)
{
	return DIERR_GENERIC;
}
HRESULT IDirectInputA::EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD)
{
	return DI_OK;
}

HRESULT IDirectInputDevice2A::Acquire()
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::Unacquire()
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT* ppdeff, IUnknown*)
{
	if (ppdeff) {
		*ppdeff = nullptr;
	}
	return DIERR_GENERIC;
}
HRESULT IDirectInputDevice2A::EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD)
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::GetCapabilities(LPDIDEVCAPS)
{
	return DIERR_GENERIC;
}
HRESULT IDirectInputDevice2A::GetDeviceData(DWORD, LPDIDEVICEOBJECTDATA, LPDWORD pdwInOut, DWORD)
{
	if (pdwInOut) {
		*pdwInOut = 0;
	}
	return DI_OK;
}
HRESULT IDirectInputDevice2A::GetDeviceInfo(LPDIDEVICEINSTANCEA)
{
	return DIERR_GENERIC;
}
HRESULT IDirectInputDevice2A::GetDeviceState(DWORD cbData, LPVOID lpvData)
{
	if (lpvData) {
		memset(lpvData, 0, cbData);
	}
	return DI_OK;
}
HRESULT IDirectInputDevice2A::Poll()
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::SetCooperativeLevel(HWND, DWORD)
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::SetDataFormat(LPCDIDATAFORMAT)
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::SetProperty(const GUID*, LPCDIPROPHEADER)
{
	return DI_OK;
}

HRESULT IDirectInputEffect::Start(DWORD, DWORD)
{
	return DI_OK;
}
HRESULT IDirectInputEffect::Stop()
{
	return DI_OK;
}
HRESULT IDirectInputEffect::SetParameters(LPCDIEFFECT, DWORD)
{
	return DI_OK;
}
