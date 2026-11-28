#include "menu/screens/controlconfigscreen.h"

#include "app/coopcontrols.h"
#include "audio/soundgroupbinding.h"
#include "input/inputdevice.h"
#include "input/inputmanager.h"
#include "input/joystickdevice.h"
#include "input/keyboarddevice.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"

#include <string.h>

DECOMP_SIZE_ASSERT(ControlConfigScreen, 0x3360)

// GLOBAL: LEGORACERS 0x004c1fc0
static LegoU32 g_controlConfigBlockedEvents[] = {
	InputDevice::c_sourceKeyboard | 0x01,
	InputDevice::c_sourceKeyboard | 0xb7,
	InputDevice::c_sourceKeyboard | 0x46,
	InputDevice::c_sourceKeyboard | 0xc5,
	InputDevice::c_sourceKeyboard | 0x45,
	InputDevice::c_sourceKeyboard | 0xdb,
	InputDevice::c_sourceKeyboard | 0xdc,
	InputDevice::c_sourceKeyboard | 0xdd,
	InputDevice::c_sourceKeyboard | 0xde,
	InputDevice::c_sourceKeyboard | 0xdf,
	InputDevice::c_sourceKeyboard | 0xe3,
};

// FUNCTION: LEGORACERS 0x0047a550
ControlConfigScreen::ControlConfigScreen()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x0047a6b0
ControlConfigScreen::~ControlConfigScreen()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x0047a7d0
void ControlConfigScreen::Reset()
{
	InputDevice** devices = m_devices;
	LegoS32* bindings = m_bindingIndices;

	m_deviceCount = 0;
	m_selectedDevice = 0;
	m_playerIndex = 0;
	m_awaitingButton = 0;
	m_savedButtonMapping = NULL;
	m_savedAxisMapping = NULL;

	::memset(devices, 0, sizeof(m_devices));
	::memset(bindings, 0, sizeof(m_bindingIndices));

	::memset(m_ellipsisText, 0, sizeof(m_ellipsisText));

	for (LegoU32 i = 0; i < sizeOfArray(m_ellipsisText) - 1; i++) {
		m_ellipsisText[i] = '.';
	}

	MenuGameScreen::Reset();
}

// FUNCTION: LEGORACERS 0x0047a860
void ControlConfigScreen::CreateWidgets()
{
	CreateImage(&m_photoImage, 0x49, 0x49);
	// [library:config] LegoRacers4K: no device picker. The title is a two-entry selector that
	// cycles through the players' controls (keyboard).
	CreateCarousel(&m_deviceCarousel, 0x3e, 0x3b);
	CreateSelector(&m_deviceSelector, &m_deviceCarousel, 0x111, 0x4d);
	CreateTextLabel(&m_infoLabel, 0x3a, 0x3a, 0x17);
	CreateTextLabel(&m_deviceLabel, 0x3a, 0x3a, 0x18);
	m_deviceCarousel.AddItem(&m_infoLabel);
	m_deviceCarousel.AddItem(&m_deviceLabel);
	// Players 3 and 4: the Player 2 title with its digit changed.
	for (LegoS32 i = 0; i < 2; i++) {
		CreateTextLabel(&m_extraPlayerLabels[i], 0x3a, 0x3a, 0x18);
		const undefined2* source = m_menuTextStrings->GetStringBuffer(0x18);
		LegoS32 length = 0;
		LegoBool32 replaced = FALSE;
		for (; source && source[length] && length < 31; length++) {
			undefined2 c = source[length];
			if (!replaced && c == '2') {
				c = static_cast<undefined2>('3' + i);
				replaced = TRUE;
			}
			m_extraPlayerBuffers[i][length] = c;
		}
		m_extraPlayerBuffers[i][length] = 0;
		m_extraPlayerStrings[i].CopyFromBufSelection(m_extraPlayerBuffers[i], static_cast<undefined2>(length + 1));
		m_extraPlayerLabels[i].SetString(&m_extraPlayerStrings[i], 0);
		m_deviceCarousel.AddItem(&m_extraPlayerLabels[i]);
	}

	for (LegoU32 i = 0; i < 9; i++) {
		CreateTextButton(&m_eventButtons[i], i + 0x108, 0x42, 0x69);
		CreateTextLabel(&m_eventLabels[i], i + 0x116, 0x37, i + 0x69);
	}

	CreateTextButton(&m_backButton, 0x3f, 0x46, 0x1e);
}

// FUNCTION: LEGORACERS 0x0047a930
void ControlConfigScreen::PopulateDeviceCarousel()
{
	ReinitializeInputBindings();
	m_deviceCount = m_inputManager->GetJoystickCount();
	if (m_deviceCount > 2) {
		m_deviceCount = 2;
	}

	LegoS32 outputIndex = 0;
	LegoS32 i;
	for (i = 0; i < 2; i++) {
		JoystickDevice* device = m_inputManager->GetJoystick(i);
		if (device) {
			m_devices[outputIndex] = device;
			m_bindingIndices[outputIndex] = i;
			outputIndex++;
		}
	}

	if (m_inputManager->IsKeyboardAvailable()) {
		InputDevice* keyboard = m_inputManager->GetKeyboard();
		for (LegoU32 i = 2; i < 5; i++) {
			m_devices[m_deviceCount] = keyboard;
			m_bindingIndices[m_deviceCount] = i;
			m_deviceCount++;
		}
	}
}

// [library:config] LegoRacers4K: each player uses their own keyboard set (2 + player).
LegoS32 ControlConfigScreen::FindPlayerDevice(LegoU32 p_player)
{
	for (LegoU32 i = 0; i < m_deviceCount; i++) {
		if (m_devices[i] == m_inputManager->GetKeyboard() &&
			m_bindingIndices[i] == static_cast<LegoS32>(2 + p_player)) {
			return i;
		}
	}

	return m_selectedDevice;
}

// [library:config] LegoRacers4K: the player title with an arrow on either side, top right.
static void PlaceControlWidget(MenuWidget* p_widget, LegoS32 p_left, LegoS32 p_top, LegoS32 p_width, LegoS32 p_height)
{
	Rect rect;
	rect.m_left = p_left;
	rect.m_top = p_top;
	rect.m_right = p_left + p_width;
	rect.m_bottom = p_top + p_height;
	p_widget->SetRect(&rect);
}

void ControlConfigScreen::LayoutPlayerSelector()
{
	const LegoS32 arrowWidth = 32;
	const LegoS32 arrowGap = 8;
	const LegoS32 titleWidth = 300;
	const LegoS32 titleHeight = 48;
	const LegoS32 selectorWidth = titleWidth + 2 * (arrowWidth + arrowGap);
	const LegoS32 selectorLeft = 320 - (selectorWidth >> 1); // centred

	PlaceControlWidget(&m_deviceSelector, selectorLeft, 14, selectorWidth, titleHeight);
	PlaceControlWidget(&m_deviceSelector.GetPrevButton(), 0, (titleHeight - arrowWidth) >> 1, arrowWidth, arrowWidth);
	PlaceControlWidget(
		&m_deviceSelector.GetNextButton(),
		selectorWidth - arrowWidth,
		(titleHeight - arrowWidth) >> 1,
		arrowWidth,
		arrowWidth
	);
	PlaceControlWidget(&m_deviceCarousel, arrowWidth + arrowGap, 0, titleWidth, titleHeight);
	PlaceControlWidget(&m_infoLabel, 0, 0, titleWidth, titleHeight);
	PlaceControlWidget(&m_deviceLabel, titleWidth, 0, titleWidth, titleHeight);
	PlaceControlWidget(&m_extraPlayerLabels[0], 2 * titleWidth, 0, titleWidth, titleHeight);
	PlaceControlWidget(&m_extraPlayerLabels[1], 3 * titleWidth, 0, titleWidth, titleHeight);
}

// FUNCTION: LEGORACERS 0x0047aaa0
LegoBool32 ControlConfigScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	if (p_createParams->m_menuId == 0x0b) {
		m_playerIndex = 1;
	}

	p_createParams->m_menuId = 9;
	if (!MenuGameScreen::Initialize(p_context, p_createParams)) {
		return FALSE;
	}

	PopulateDeviceCarousel();
	for (LegoS32 i = 0; i < 4; i++) {
		CoopControls_GetPresetEvents(c_coopPresetKeyboard1 + i, m_keyEvents[i]); // [library:config]
	}
	m_selectedDevice = FindPlayerDevice(m_playerIndex);
	LayoutPlayerSelector();
	m_deviceCarousel.SetSelection(m_playerIndex);
	m_deviceSelector.Select(4);
	Update(0);

	return TRUE;
}

// FUNCTION: LEGORACERS 0x0047ab60
void ControlConfigScreen::Navigate()
{
	if (m_playerIndex < 2) { // [library:config] the save state only has players 1 and 2
		m_context->m_saveSystem.GetGameState().SelectInputBinding(m_playerIndex, m_bindingIndices[m_selectedDevice]);
	}
	m_context->m_menuStack.Pop();
	m_context->m_menuStack.Push(0x30);
}

// FUNCTION: LEGORACERS 0x0047abb0
void ControlConfigScreen::OnWidgetValueChanged(MenuWidget* p_source)
{
	if (p_source == &m_deviceSelector) {
		m_playerIndex = m_deviceCarousel.GetSelectedIndex();
		m_selectedDevice = FindPlayerDevice(m_playerIndex);
	}
}

// FUNCTION: LEGORACERS 0x0047abd0
void ControlConfigScreen::OnIconFocused(MenuIcon* p_source)
{
	LegoS32 eventIndex = p_source->GetId() - m_eventButtons[0].GetId();

	if (eventIndex >= 0 && static_cast<LegoU32>(eventIndex) <= 8) {
		InputDevice* device = m_devices[m_selectedDevice];
		m_savedButtonMapping = device->GetCustomButtonMapping();
		m_savedAxisMapping = device->GetCustomAxisMapping();
		device->SetEventMappings(NULL, NULL);
		if (m_playerIndex < 2) { // [library:config]
			m_context->m_saveSystem.GetGameState().SetInputEvent(m_bindingIndices[m_selectedDevice], eventIndex, 0);
		}
		m_awaitingButton = p_source;
	}
}

// FUNCTION: LEGORACERS 0x0047ac60
void ControlConfigScreen::OnIconUnfocused(MenuWidget* p_source)
{
	if (p_source == &m_backButton) {
		m_nextMenuId = 8;
		m_navPending = TRUE;
	}

	m_clickedWidget = p_source;
}

// FUNCTION: LEGORACERS 0x0047ac90
LegoBool32 ControlConfigScreen::HandleKeyDown(MenuWidget*, InputEventQueue::Event* p_item, undefined4, undefined4)
{
	GameState& state = m_context->m_saveSystem.GetGameState();
	LegoS32 selectedDeviceIndex = m_selectedDevice;
	InputDevice* device = m_devices[selectedDeviceIndex];
	LegoS32 entryIndex = m_bindingIndices[selectedDeviceIndex];
	LegoU32 keyCode = p_item->m_keyCode;
	LegoU32 source = keyCode & InputDevice::c_sourceMask;

	if (!m_navPending) {
		if (!m_awaitingButton) {
			return FALSE;
		}

		if (source == InputDevice::c_sourceJoystickButton || source == InputDevice::c_sourceKeyboard) {
			for (LegoU32 i = 0; i < sizeOfArray(g_controlConfigBlockedEvents); i++) {
				if (keyCode == g_controlConfigBlockedEvents[i]) {
					m_soundGroupBinding->PlaySoundByIndex(8);
					return TRUE;
				}
			}

			if (source == InputDevice::c_sourceJoystickButton && p_item->m_device != device) {
				m_soundGroupBinding->PlaySoundByIndex(8);
				return TRUE;
			}

			LegoS32 eventIndex = m_awaitingButton->GetId() - m_eventButtons[0].GetId();
			if (m_playerIndex < 2) { // [library:config]
				state.SetInputEvent(entryIndex, eventIndex, keyCode);
			}
			// [library:config] LegoRacers4K: every player's keys are saved to controls.ini.
			if (source == InputDevice::c_sourceKeyboard && m_playerIndex < 4) {
				m_keyEvents[m_playerIndex][eventIndex] = keyCode;
				CoopControls_SetKeyboardEvent(m_playerIndex, eventIndex, keyCode);
			}
			device->SetEventMappings(m_savedButtonMapping, m_savedAxisMapping);
			m_awaitingButton->Unfocus(0);
			m_awaitingButton = NULL;
		}
	}

	return TRUE;
}

// FUNCTION: LEGORACERS 0x0047ad90
LegoBool32 ControlConfigScreen::HandleKeyUp(MenuWidget*, InputEventQueue::Event*, undefined4, undefined4)
{
	if (m_navPending) {
		return TRUE;
	}

	return MenuEventResult(m_awaitingButton);
}

// FUNCTION: LEGORACERS 0x0047adb0
void ControlConfigScreen::RefreshEventTexts()
{
	GameState& state = m_context->m_saveSystem.GetGameState();

	for (LegoS32 i = 0; i < sizeOfArray(m_eventButtons); i++) {
		m_eventTexts[i].CopyFromBufSelection(m_ellipsisText, 0);

		if (!(m_eventButtons[i].GetStateFlags() & MenuIcon::c_flagFocused)) {
			// [library:config] LegoRacers4K: the player's keyboard set from controls.ini.
			LegoU32 event = m_playerIndex < 4 ? m_keyEvents[m_playerIndex][i] : 0;

			if (event) {
				InputDevice* device;
				if ((event & InputDevice::c_sourceMask) == InputDevice::c_sourceKeyboard) {
					device = m_inputManager->GetKeyboard();
				}
				else {
					device = m_devices[m_selectedDevice];
				}

				m_eventTexts[i].CopyFromBufSelection((undefined2*) device->GetControlName(event), 0);
				m_eventTexts[i].ToUpperCase();
			}
			else if (i < 2) {
				InputDevice* device = m_devices[m_selectedDevice];
				if (device->GetDeviceType() == 4 && (device->GetAxisMask() & 1)) {
					m_eventTexts[i].CopyFromBufSelection(
						(undefined2*) device->GetControlName(InputDevice::c_sourceJoystickAxisButton),
						0
					);
					m_eventTexts[i].ToUpperCase();
				}
			}
		}

		m_eventButtons[i].SetText(&m_eventTexts[i]);
	}
}

// FUNCTION: LEGORACERS 0x0047aeb0
LegoBool32 ControlConfigScreen::Update(undefined4 p_source)
{
	RefreshEventTexts();

	if (m_devices[m_selectedDevice]->GetDeviceType() == 3) {
		m_eventButtons[0].Enable(0);
		m_eventButtons[1].Enable(0);
	}
	else {
		m_eventButtons[0].Disable(0);
		m_eventButtons[1].Disable(0);
	}

	return MenuGameScreen::Update(p_source);
}
