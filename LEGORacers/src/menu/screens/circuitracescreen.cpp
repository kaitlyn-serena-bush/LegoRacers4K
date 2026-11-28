#include "menu/screens/circuitracescreen.h"

#include "camera/golcamera.h"
#include "golname.h"
#include "golstring.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"
#include "race/data/racenameentry.h"

#include <string.h>

DECOMP_SIZE_ASSERT(CircuitRaceScreen, 0x2228)

// FUNCTION: LEGORACERS 0x00479d70
CircuitRaceScreen::CircuitRaceScreen()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x00479e70
CircuitRaceScreen::~CircuitRaceScreen()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x00479f50
void CircuitRaceScreen::Reset()
{
	m_transition = NULL;
	m_previewTimerMs = 0;
	m_previewRaceIndex = 0;
	m_previewChanged = FALSE;
	MenuGameScreen::Reset();
}

// FUNCTION: LEGORACERS 0x00479f70
void CircuitRaceScreen::CreateWidgets()
{
	CreateImage(&m_photoImage, 0x49, 0x49);
	SingleRaceSelectBase::CreateWidgets();

	CreateTextLabel(&m_infoLabel, 0x3a, 0x3a, 0x0d);
	m_infoLabel.WrapText(0x14);
	CreateTextLabel(&m_circuitCaptionLabel, 0x5b, 0x37, 0x4a);
	CreateTextLabel(&m_circuitNameLabel, 0x5c, 0x37, 0x4c);
	CreateTextLabel(&m_raceNameLabel, 0x5d, 0x37, 0x4a);
	CreateTextLabel(&m_trackCaptionLabel, 0x5f, 0x37, 0x4b);
	CreateTextLabel(&m_circuitDescLabel, 0x60, 0x37, 0x60);
	CreateTextButton(&m_startButton, 0x40, 0x46, 0x72);
	CreateTextButton(&m_backButton, 0x3f, 0x43, 2);
}

// [library:config] LegoRacers4K layout, like Race Select: title centred, the preview with the circuit
// arrows on either side, the circuit name below it, OK bottom-right and MAIN MENU bottom-left. The
// circuit number and the hosting champion are gone.
static void PlaceWidget(MenuWidget* p_widget, LegoS32 p_left, LegoS32 p_top)
{
	Rect rect = *p_widget->GetRect();
	rect.m_right += p_left - rect.m_left;
	rect.m_bottom += p_top - rect.m_top;
	rect.m_left = p_left;
	rect.m_top = p_top;
	p_widget->SetRect(&rect);
}

static LegoS32 CenterLeft(MenuWidget* p_widget)
{
	const Rect* r = p_widget->GetRect();
	return 320 - ((r->m_right - r->m_left) >> 1);
}

void CircuitRaceScreen::LayoutCentered()
{
	PlaceWidget(&m_infoLabel, CenterLeft(&m_infoLabel), m_infoLabel.GetRect()->m_top);

	const LegoS32 frameTop = 84;
	LegoS32 frameLeft = CenterLeft(&m_borderFrame);
	LegoS32 dx = frameLeft - m_borderFrame.GetRect()->m_left;
	LegoS32 dy = frameTop - m_borderFrame.GetRect()->m_top;
	LegoS32 frameWidth = m_borderFrame.GetRect()->m_right - m_borderFrame.GetRect()->m_left;
	LegoS32 frameHeight = m_borderFrame.GetRect()->m_bottom - m_borderFrame.GetRect()->m_top;
	PlaceWidget(&m_borderFrame, frameLeft, frameTop);
	PlaceWidget(&m_sceneWidget, m_sceneWidget.GetRect()->m_left + dx, m_sceneWidget.GetRect()->m_top + dy);
	m_sceneWidget.ComputeViewportRect();
	if (m_sceneWidget.m_frame) {
		m_sceneWidget.m_frame->SetViewportRect(&m_sceneWidget.m_viewportRect);
	}

	// Circuit selector: spans the preview so its arrows sit on either side, level with the
	// preview's middle; its own picture carousel is not shown.
	const LegoS32 arrowWidth = 32;
	const LegoS32 arrowGap = 8;
	LegoS32 selectorWidth = frameWidth + 2 * (arrowWidth + arrowGap);
	LegoS32 selectorTop = frameTop + (frameHeight >> 1) - (arrowWidth >> 1);
	Rect selector = *m_trackSelector.GetRect();
	selector.m_left = 320 - (selectorWidth >> 1);
	selector.m_right = selector.m_left + selectorWidth;
	selector.m_top = selectorTop;
	selector.m_bottom = selectorTop + arrowWidth;
	m_trackSelector.SetRect(&selector);
	PlaceWidget(&m_trackSelector.GetPrevButton(), 0, 0);
	PlaceWidget(&m_trackSelector.GetNextButton(), selectorWidth - arrowWidth, 0);
	PlaceWidget(&m_trackCarousel, (selectorWidth - arrowWidth) >> 1, 0);
	m_trackCarousel.ClearFlags(2);

	// Circuit name (the race name label) below the preview.
	Rect name = *m_raceNameLabel.GetRect();
	LegoS32 nameHeight = name.m_bottom - name.m_top;
	name.m_left = 320 - 190;
	name.m_right = 320 + 190;
	name.m_top = frameTop + frameHeight + 6;
	name.m_bottom = name.m_top + nameHeight;
	m_raceNameLabel.SetRect(&name);

	m_circuitCaptionLabel.ClearFlags(2);
	m_circuitNameLabel.ClearFlags(2);
	m_trackCaptionLabel.ClearFlags(2);
	m_circuitDescLabel.ClearFlags(2);

	PlaceWidget(&m_backButton, 3, 418);
	PlaceWidget(&m_startButton, 640 - 3 - (m_startButton.GetRect()->m_right - m_startButton.GetRect()->m_left), 418);
}

// FUNCTION: LEGORACERS 0x0047a040
LegoBool32 CircuitRaceScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	if (!SingleRaceSelectBase::Initialize(p_context, p_createParams)) {
		return FALSE;
	}

	OnWidgetValueChanged(&m_trackSelector);
	m_startButton.Select(0);
	LayoutCentered();
	return TRUE;
}

// FUNCTION: LEGORACERS 0x0047a090
void CircuitRaceScreen::OnIconUnfocused(MenuWidget* p_source)
{
	if (p_source == &m_startButton) {
		m_nextMenuId = 0x1e;
	}
	else if (p_source == &m_backButton) {
		m_nextMenuId = 2;
	}

	if (m_nextMenuId != 0xffff) {
		m_navPending = TRUE;
	}

	m_clickedWidget = p_source;
}

// FUNCTION: LEGORACERS 0x0047a0e0
void CircuitRaceScreen::OnWidgetValueChanged(MenuWidget*)
{
	m_context->m_context->m_raceMode = LegoRacers::Context::c_raceModeCircuit;

	m_circuitIndex = m_trackCarousel.GetSelectedIndex();
	m_circuitNameLabel.SetStringByIndex(static_cast<undefined2>(m_circuitIndex + 0x4c), 0);
	m_circuitDescLabel.SetStringByIndex(static_cast<undefined2>(m_circuitIndex + 0x61), 0);

	if (m_circuitIndex < m_context->m_circuitList.GetEntryCount()) {
		m_circuitEntry = &m_context->m_circuitList.GetEntries()[m_circuitIndex];
		LegoRacers::Context* context = m_context->m_context;
		::memcpy(context->m_circuitName, m_circuitEntry->GetName(), sizeof(GolName));

		LegoU8 mask = static_cast<LegoU8>(1 << m_context->m_circuitList.GetEntryIndex(m_circuitEntry));
		for (LegoU32 i = 0; i < sizeOfArray(context->m_raceSlots); i++) {
			LegoRacers::RaceSlot* slot = &context->m_raceSlots[i];
			RaceNameEntry* raceNameEntry = m_circuitEntry->GetRaceNameEntry(i);
			if (raceNameEntry) {
				::memcpy(slot->m_raceName, raceNameEntry->GetName(), sizeof(GolName));
				::memcpy(slot->m_folderName, raceNameEntry->GetFolderName(), sizeof(GolName));
				slot->m_enabled = TRUE;
				slot->m_mirror = raceNameEntry->GetMirror();
			}
			else {
				slot->m_raceName[0] = '\0';
				slot->m_folderName[0] = '\0';
				slot->m_enabled = FALSE;
				slot->m_mirror = 0;
			}
		}

		if (m_context->m_saveSystem.GetGameState().GetUnlockedCircuits() & mask) {
			m_startButton.Enable(5);
			m_sceneOverlay.ClearFlags(2);
		}
		else {
			m_startButton.Disable(5);
			m_backButton.Select(0);
			m_sceneOverlay.SetFlags(2);
		}

		m_previewTimerMs = 0;
		m_previewRaceIndex = 0;
		UpdateRacePreview();
	}
}

// FUNCTION: LEGORACERS 0x0047a250
void CircuitRaceScreen::Navigate()
{
	if (m_transition && m_transition->IsActive()) {
		m_context->m_menuAnimations.Deactivate(m_transition);
		m_transition = NULL;
	}

	if (m_nextMenuId == 2) {
		m_context->m_menuStack.Pop();
		return;
	}

	m_context->m_menuStack.Push(0x13);
}

// FUNCTION: LEGORACERS 0x0047a2b0
void CircuitRaceScreen::UpdateRacePreview()
{
	GolString string;
	GolName frameName;
	GolName driverName;
	driverName[0] = '\0';

	RaceNameEntry* raceNameEntry = m_circuitEntry->GetRaceNameEntry(m_previewRaceIndex);
	if (raceNameEntry) {
		::memcpy(frameName, raceNameEntry->GetThemeName(), sizeof(GolName));
		::memcpy(driverName, raceNameEntry->GetMascotName(), sizeof(GolName));

		GolNameTable* frameNames = &m_sceneWidget.m_definition;
		CutsceneDefinition::Frame* frame;
		if (!frameNames->GetNameEntries()) {
			frame = NULL;
		}
		else {
			frame = static_cast<CutsceneDefinition::Frame*>(frameNames->GetName(frameName));
		}

		if (frame && frame != m_sceneWidget.m_frame) {
			m_sceneWidget.SetFrame(frame);
			m_sceneWidget.m_frame->SetFlags(CutsceneDefinition::Frame::c_flagLoop);
			m_previewTimerMs = 2000;
			ApplyThemeColor(m_previewRaceIndex + m_circuitIndex * 4);
			raceNameEntry->CopyDisplayString(&string);
			m_raceNameLabel.SetString(&string, 0);
		}
	}
	else {
		m_previewTimerMs = 2000;
	}

	if (driverName[0]) {
		SetPreviewDriver(driverName);
	}

	m_previewChanged = TRUE;
}

// FUNCTION: LEGORACERS 0x0047a3f0
LegoBool32 CircuitRaceScreen::Update(undefined4 p_elapsed)
{
	if (m_transition && !m_transition->IsActive()) {
		m_transition = NULL;
	}

	if (p_elapsed > m_previewTimerMs && m_circuitIndex != 6) {
		m_previewRaceIndex = (m_previewRaceIndex + 1) & 3;
		UpdateRacePreview();
	}
	else {
		m_previewTimerMs -= p_elapsed;
	}

	if (m_previewTimerMs < 250 && !m_context->m_menuAnimations.HasActive() && m_circuitIndex != 6) {
		if (m_transition && m_transition->IsActive()) {
			m_context->m_menuAnimations.Deactivate(m_transition);
			m_transition = NULL;
		}

		GolCamera* rectSource = m_sceneWidget.m_frame->GetActiveCamera();
		m_transition = m_context->m_menuAnimations.Activate(250, FALSE, NULL, rectSource);
	}

	LegoBool32 result = MenuSceneScreen::Update(p_elapsed);
	if (m_previewChanged) {
		if (m_transition && m_transition->IsActive()) {
			m_context->m_menuAnimations.Deactivate(m_transition);
			m_transition = NULL;
		}

		m_sceneWidget.m_frame->Update(0);
		GolCamera* rectSource = m_sceneWidget.m_frame->GetActiveCamera();
		m_transition = m_context->m_menuAnimations.Activate(250, TRUE, NULL, rectSource);
		m_previewChanged = FALSE;
	}

	return result;
}
