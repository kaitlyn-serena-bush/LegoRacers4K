// [library:input] LegoRacers4K: simultaneous 2-4 player Versus racer pick; see cooppickscreen.h.

#include "menu/screens/cooppickscreen.h"

#include "golhashtable.h"
#include "input/inputmanager.h"
#include "input/joystickdevice.h"
#include "input/keyboarddevice.h"
#include "menu/menugamecontext.h"
#include "menu/menuinputdispatcher.h"
#include "menu/menuscreencreateparams.h"
#include "menu/menuscreenid.h"
#include "world/golworlddatabase.h"

#include <SDL3/SDL.h>
#include <string.h>

namespace
{
const LegoU32 c_keyEscape = InputDevice::c_sourceKeyboard | 0x01;
const LegoU32 c_padA = InputDevice::c_sourceJoystickButton | 0;
const LegoU32 c_padB = InputDevice::c_sourceJoystickButton | 1;
const LegoU32 c_padStart = InputDevice::c_sourceJoystickButton | 6;

// Menus normally map every pad button to "accept" (MenuInputHandler::MapAllJoystickButtons).
undefined2 g_acceptAllMapping[256];

void MoveTo(MenuWidget* p_widget, LegoS32 p_left, LegoS32 p_top)
{
	Rect rect = *p_widget->GetRect();
	rect.m_right += p_left - rect.m_left;
	rect.m_bottom += p_top - rect.m_top;
	rect.m_left = p_left;
	rect.m_top = p_top;
	p_widget->SetRect(&rect);
}
} // namespace

CoopPickScreen::CoopPickScreen()
{
	Reset();
}

CoopPickScreen::~CoopPickScreen()
{
	Destroy();
}

LegoBool32 CoopPickScreen::Destroy()
{
	if (m_initialized) {
		g_menuRawInput = FALSE;
		for (LegoU32 i = 0; i < sizeOfArray(g_acceptAllMapping); i++) {
			g_acceptAllMapping[i] = 4;
		}
		InputManager* inputManager = m_context->m_context->m_golApp->GetInputManager();
		for (LegoS32 i = 0; i < inputManager->GetJoystickCount(); i++) {
			JoystickDevice* joystick = inputManager->GetJoystick(i);
			if (joystick) {
				joystick->SetEventMappings(g_acceptAllMapping, NULL);
			}
		}
	}
	return RacerModelScreenBase::Destroy();
}

void CoopPickScreen::Reset()
{
	::memset(m_lineBuffers, 0, sizeof(m_lineBuffers));
	::memset(m_presetEvents, 0, sizeof(m_presetEvents));
	for (LegoS32 i = 0; i < c_panelCount; i++) {
		m_panelPresets[i] = c_coopPresetNone;
		m_panelReady[i] = FALSE;
		m_panelDimmed[i] = FALSE;
	}
	m_playerCount = 2;
	RacerModelScreenBase::Reset();
}

static void PanelOrigin(LegoS32 p_panel, LegoS32 p_count, LegoS32* p_x, LegoS32* p_y)
{
	*p_x = (p_panel % 2) * 320;
	*p_y = p_count <= 2 ? 120 : (p_panel / 2) * 240;
}

void CoopPickScreen::CreateWidgets()
{
	LegoS32 i;

	CreateImage(&m_photoImage, 0x49, 0x49);

	if (g_hashTable != NULL) {
		g_hashTable->SetCurrentEntryFromString("MENUDATA\\RS_SET");
	}

	MenuFramedSceneView::CreateParams* source = static_cast<MenuFramedSceneView::CreateParams*>(GetBindingEntry(0x6e));
	for (i = 0; i < m_modelSlotCount && source; i++) {
		LegoS32 x, y;
		PanelOrigin(i, m_modelSlotCount, &x, &y);

		MenuFramedSceneView::CreateParams createParams = *source;
		ApplyWidgetDefaults(&createParams);
		if (createParams.m_unk0x84) {
			ApplyWidgetDefaults(createParams.m_unk0x84);
		}
		createParams.m_rect.m_left = x + 6;
		createParams.m_rect.m_top = y + 4;
		createParams.m_rect.m_right = x + 314;
		createParams.m_rect.m_bottom = y + 236;
		createParams.m_eventCode = 0;
		createParams.m_worldScale = 1.0f;
		createParams.m_aspectScale = GetAspectScale();
		m_sceneViews[i].Create(&createParams, m_useBinaryFiles);
	}

	if (g_hashTable != NULL) {
		g_hashTable->SetCurrentEntryFromString("MENUDATA");
	}

	// Text sits on top of each panel's 3D view: title at the top, the rest along the bottom.
	static const LegoS32 c_lineOffsets[c_lineCount] = {10, 150, 172, 190, 212};
	for (i = 0; i < m_modelSlotCount; i++) {
		LegoS32 x, y;
		PanelOrigin(i, m_modelSlotCount, &x, &y);
		for (LegoS32 line = 0; line < c_lineCount; line++) {
			CreateTextLabel(&m_lines[i][line], 0x3a, 0x37, 0x37);
			Rect rect;
			rect.m_left = x + 16;
			rect.m_top = y + c_lineOffsets[line];
			rect.m_right = x + 306;
			rect.m_bottom = rect.m_top + 18;
			m_lines[i][line].SetRect(&rect);
		}
	}

	CreateModelSlots();
}

LegoBool32 CoopPickScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	CoopVersusSettings* settings = CoopVersus_GetSettings();
	m_playerCount = settings->m_playerCount;
	if (m_playerCount < 2) {
		m_playerCount = 2;
	}
	if (m_playerCount > c_panelCount) {
		m_playerCount = c_panelCount;
	}

	undefined4 params[3];
	params[0] = m_playerCount;
	params[1] = 1;
	params[2] = 0xffff3;
	p_context->m_saveSystem.GetActiveRecord().ClearSelectedRecords();

	// Widgets and bindings come from the single-player pick screen (pickrcr.mib).
	p_createParams->m_menuId = c_menuPickRacer;
	if (!RacerModelScreenBase::Initialize(p_context, p_createParams, params)) {
		return FALSE;
	}

	for (LegoS32 preset = 0; preset < c_coopPresetCount; preset++) {
		CoopControls_GetPresetEvents(preset, m_presetEvents[preset]);
	}

	// Read raw pad buttons and every key (menu focus navigation is off on this screen).
	InputManager* inputManager = m_context->m_context->m_golApp->GetInputManager();
	for (LegoS32 i = 0; i < inputManager->GetJoystickCount(); i++) {
		JoystickDevice* joystick = inputManager->GetJoystick(i);
		if (joystick) {
			joystick->SetAxisButtonEventsEnabled(TRUE);
			joystick->SetEventMappings(NULL, NULL);
		}
	}
	g_menuRawInput = TRUE;

	for (LegoS32 i = 0; i < m_modelSlotCount; i++) {
		m_lineStrings[i][1].CopyFromBufSelection(m_lineBuffers[i][1], 0x10);
		SwapSlotModel(i);
		RefreshPanel(i);
	}

	SetupLighting();
	return TRUE;
}

void CoopPickScreen::SetLine(LegoS32 p_panel, LegoS32 p_line, const char* p_text)
{
	undefined2* buffer = m_lineBuffers[p_panel][p_line];
	LegoU32 i = 0;
	for (; p_text[i] && i < c_textLength - 1; i++) {
		buffer[i] = static_cast<LegoU8>(p_text[i]);
	}
	buffer[i] = 0;
	m_lineStrings[p_panel][p_line].CopyFromBufSelection(buffer, static_cast<undefined2>(i + 1));
	m_lines[p_panel][p_line].SetString(&m_lineStrings[p_panel][p_line], 0);
}

void CoopPickScreen::RefreshPanel(LegoS32 p_panel)
{
	char text[128];
	LegoS32 preset = m_panelPresets[p_panel];

	if (preset == c_coopPresetNone) {
		SDL_snprintf(text, sizeof(text), "PLAYER %d", p_panel + 1);
	}
	else if (preset < c_coopPresetKeyboard1) {
		SDL_snprintf(text, sizeof(text), "PLAYER %d - GAMEPAD %d", p_panel + 1, preset + 1);
	}
	else {
		SDL_snprintf(text, sizeof(text), "PLAYER %d - KEYS %d", p_panel + 1, preset - c_coopPresetKeyboard1 + 1);
	}
	SetLine(p_panel, 0, text);

	SaveRecordList::Record* record = m_recordCursors[p_panel].GetSelectedRecord();
	if (record) {
		record->GetName(&m_lineStrings[p_panel][1]);
		m_lines[p_panel][1].SetString(&m_lineStrings[p_panel][1], 0);
	}

	if (preset == c_coopPresetNone) {
		char keys[4][48];
		for (LegoS32 i = 0; i < 4; i++) {
			CoopControls_GetActionName(c_coopPresetKeyboard1 + i, c_coopActionAccelerate, keys[i], sizeof(keys[i]));
		}
		SetLine(p_panel, 2, "TO JOIN PRESS A ON A GAMEPAD");
		SDL_snprintf(text, sizeof(text), "OR %s, %s, %s OR %s", keys[0], keys[1], keys[2], keys[3]);
		SetLine(p_panel, 3, text);
		SetLine(p_panel, 4, "");
		return;
	}

	CoopControls_DescribePreset(preset, text, sizeof(text));
	char* secondLine = SDL_strchr(text, '\n');
	if (secondLine) {
		*secondLine++ = '\0';
	}
	SetLine(p_panel, 2, text);
	SetLine(p_panel, 3, secondLine ? secondLine : "");

	if (m_panelReady[p_panel]) {
		SetLine(p_panel, 4, "READY!");
	}
	else {
		char go[48];
		char brake[48];
		CoopControls_GetActionName(preset, c_coopActionAccelerate, go, sizeof(go));
		CoopControls_GetActionName(preset, c_coopActionBrake, brake, sizeof(brake));
		SDL_snprintf(text, sizeof(text), "%s - READY   %s - LEAVE", go, brake);
		SetLine(p_panel, 4, text);
	}
}

void CoopPickScreen::SetSlotDimmed(LegoS32 p_panel, LegoBool32 p_dimmed)
{
	if (m_panelDimmed[p_panel] == p_dimmed) {
		return;
	}
	m_panelDimmed[p_panel] = p_dimmed;

	GolWorldDatabase** database = m_sceneViews[p_panel].GetWorldAddress();
	GolRenderDevice::MaterialColor* materialColor = (*database)->GetAmbientMaterial();
	GolRenderDevice::Light* light = (*database)->GetLight();

	ColorRGBA color = materialColor->GetColor();
	ColorRGBA lightColor = light->GetColor();
	if (p_dimmed) {
		color.m_red >>= 1;
		color.m_grn >>= 1;
		color.m_blu >>= 1;
		lightColor.m_red >>= 1;
		lightColor.m_grn >>= 1;
		lightColor.m_blu >>= 1;
	}
	else {
		color.m_red <<= 1;
		color.m_grn <<= 1;
		color.m_blu <<= 1;
		lightColor.m_red <<= 1;
		lightColor.m_grn <<= 1;
		lightColor.m_blu <<= 1;
	}
	materialColor->SetColor(color);
	light->SetColor(lightColor);
}

void CoopPickScreen::ChangeRacer(LegoS32 p_panel, LegoBool32 p_next)
{
	m_context->m_modelBuilder.RefreshMenuResources();
	if (p_next) {
		SelectNextRecord(p_panel);
	}
	else {
		SelectPreviousRecord(p_panel);
	}

	// Rebuilding menu resources invalidates the other panels' models.
	for (LegoS32 j = 0; j < m_modelSlotCount; j++) {
		if (j != p_panel) {
			SwapSlotModel(j);
		}
	}
	RefreshPanel(p_panel);
}

LegoS32 CoopPickScreen::PresetForEvent(InputEventQueue::Event* p_event, LegoS32* p_action)
{
	InputManager* inputManager = m_context->m_context->m_golApp->GetInputManager();
	LegoU32 code = p_event->m_keyCode;
	*p_action = -1;

	if (p_event->m_device == inputManager->GetKeyboard()) {
		// Presets already in use win when two sets share a key.
		for (LegoS32 pass = 0; pass < 2; pass++) {
			for (LegoS32 preset = c_coopPresetKeyboard1; preset < c_coopPresetCount; preset++) {
				LegoBool32 joined = FALSE;
				for (LegoS32 i = 0; i < m_modelSlotCount; i++) {
					joined |= m_panelPresets[i] == preset;
				}
				if (joined != (pass == 0)) {
					continue;
				}
				for (LegoS32 action = 0; action < c_coopActionCount; action++) {
					if (m_presetEvents[preset][action] == code) {
						*p_action = action;
						return preset;
					}
				}
			}
		}
		return c_coopPresetNone;
	}

	for (LegoS32 slot = 0; slot < c_coopMaxPlayers; slot++) {
		if (p_event->m_device != inputManager->FindJoystickByDeviceId(slot)) {
			continue;
		}
		if (code == c_padA || code == c_padStart) {
			*p_action = c_coopActionAccelerate;
		}
		else if (code == c_padB) {
			*p_action = c_coopActionBrake;
		}
		else {
			for (LegoS32 action = 0; action < c_coopActionCount; action++) {
				if (m_presetEvents[slot][action] == code) {
					*p_action = action;
					break;
				}
			}
		}
		return slot;
	}

	return c_coopPresetNone;
}

LegoBool32 CoopPickScreen::HandleKeyDown(MenuWidget* p_source, InputEventQueue::Event* p_event, undefined4, undefined4)
{
	if (m_navPending) {
		return TRUE;
	}

	if (p_source != GetRootIcon()) {
		return FALSE;
	}

	if (p_event->m_keyCode == c_keyEscape || p_event->m_keyCode == (InputDevice::c_sourceJoystickButton | 4)) {
		m_nextMenuId = 0x3f;
		m_navPending = TRUE;
		return TRUE;
	}

	LegoS32 action;
	LegoS32 preset = PresetForEvent(p_event, &action);
	if (preset == c_coopPresetNone || action < 0) {
		return FALSE;
	}

	LegoS32 panel = -1;
	for (LegoS32 i = 0; i < m_modelSlotCount; i++) {
		if (m_panelPresets[i] == preset) {
			panel = i;
		}
	}

	ActiveRecordBuffer& selected = m_context->m_saveSystem.GetActiveRecord();

	if (panel < 0) {
		if (action != c_coopActionAccelerate) {
			return FALSE;
		}
		for (LegoS32 i = 0; i < m_modelSlotCount; i++) {
			if (m_panelPresets[i] == c_coopPresetNone) {
				m_panelPresets[i] = preset;
				RefreshPanel(i);
				break;
			}
		}
		return TRUE;
	}

	switch (action) {
	case c_coopActionAccelerate:
		if (!m_panelReady[panel]) {
			m_panelReady[panel] = TRUE;
			selected.SetSelectedRecord(panel, m_recordCursors[panel].GetSelectedRecord());
			SetSlotDimmed(panel, TRUE);
			PlayRandomNamedAnimation(panel);
			RefreshPanel(panel);
		}
		break;
	case c_coopActionBrake:
		if (m_panelReady[panel]) {
			m_panelReady[panel] = FALSE;
			selected.SetSelectedRecord(panel, NULL);
			SetSlotDimmed(panel, FALSE);
		}
		else {
			m_panelPresets[panel] = c_coopPresetNone;
		}
		RefreshPanel(panel);
		break;
	case c_coopActionLeft:
	case c_coopActionRight:
		if (!m_panelReady[panel]) {
			ChangeRacer(panel, action == c_coopActionRight);
		}
		break;
	default:
		return FALSE;
	}

	LegoBool32 allReady = TRUE;
	for (LegoS32 i = 0; i < m_modelSlotCount; i++) {
		allReady &= m_panelReady[i];
	}
	if (allReady) {
		m_nextMenuId = 0x41;
		m_navPending = TRUE;
	}

	return TRUE;
}

void CoopPickScreen::Navigate()
{
	LegoRacers::Context* context = m_context->m_context;

	switch (m_nextMenuId) {
	case 0x3f:
		m_context->m_menuStack.Pop();
		break;
	case 0x41: {
		CoopVersusSettings* settings = CoopVersus_GetSettings();
		m_context->m_saveSystem.GetActiveRecord().SetSelectedRecordCount(m_modelSlotCount);
		for (LegoS32 i = 0; i < c_coopMaxPlayers; i++) {
			CoopControls_SetSessionPreset(i, i < m_modelSlotCount ? m_panelPresets[i] : c_coopPresetNone);
		}
		context->m_lapCount = settings->m_lapCount;

		if (settings->m_grandPrix) {
			CircuitDefinitionList::CircuitDefinition* circuit =
				m_context->m_circuitList.FindCircuit(context->m_circuitName);
			LegoU32 circuitIndex = m_context->m_circuitList.GetEntryIndex(circuit);
			context->m_raceMode = LegoRacers::Context::c_raceModeCircuit;
			context->m_racerCount = 6;
			m_context->m_menuStack.Push(static_cast<LegoU16>(circuitIndex + c_menuCircuit1));
		}
		else {
			context->m_raceMode = LegoRacers::Context::c_raceModeSingle;
			context->m_racerCount = m_modelSlotCount;
			m_context->m_menuStack.ResetSize();
		}

		m_context->m_modelBuilder.SetMenuFlowFlags(
			m_context->m_modelBuilder.GetMenuFlowFlags() & ~DriverModelBuilder::c_menuFlowVersus
		);
		m_navPending = TRUE;
		break;
	}
	}

	CommitRecordSelections();
}

LegoBool32 CoopPickScreen::Update(undefined4 p_elapsed)
{
	return RacerModelScreenBase::Update(p_elapsed);
}
