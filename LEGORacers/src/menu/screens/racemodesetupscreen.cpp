#include "menu/screens/racemodesetupscreen.h"

#include "app/coopcontrols.h"
#include "golname.h"
#include "golstringtable.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"
#include "race/data/racenameentry.h"
#include "save/savesystem.h"

#include <string.h>

DECOMP_SIZE_ASSERT(RaceModeSetupScreen, 0x2e60)

// GLOBAL: LEGORACERS 0x004c2204
LegoChar g_veronicaVoltageName[] = "VV";

// FUNCTION: LEGORACERS 0x004246d0 FOLDED
void RaceModeSetupScreen::FormatTime(LegoChar* p_buffer, LegoU32 p_time)
{
	LegoU32 millisecondsPerHour = 3600000;
	LegoS32 divisor = 10;
	LegoS32 digitOffset;

	p_buffer[8] = '\0';
	LegoU32 time = p_time % millisecondsPerHour;
	if (time >= 600000) {
		digitOffset = 7;
		p_buffer[5] = ':';
		p_buffer[2] = ':';
	}
	else {
		digitOffset = 6;
		p_buffer[4] = ':';
		p_buffer[1] = ':';
		p_buffer[7] = '\0';
	}

	LegoU32 centiseconds = time / divisor;
	p_buffer[digitOffset] = static_cast<LegoChar>(centiseconds % divisor + '0');
	centiseconds /= divisor;
	p_buffer[digitOffset - 1] = static_cast<LegoChar>(centiseconds % divisor + '0');

	LegoU32 seconds = centiseconds / divisor;
	LegoS32 secondsWithinMinute = static_cast<LegoS32>(seconds % 60);
	LegoU32 minutes = seconds / 60;
	p_buffer[digitOffset - 3] = static_cast<LegoChar>(secondsWithinMinute % divisor + '0');
	p_buffer[digitOffset - 4] = static_cast<LegoChar>((secondsWithinMinute / divisor) % divisor + '0');

	p_buffer[digitOffset - 6] = static_cast<LegoChar>(minutes % divisor + '0');
	LegoU32 extraMinutes = minutes / divisor;
	if (extraMinutes) {
		p_buffer[digitOffset - 7] = static_cast<LegoChar>(extraMinutes % divisor + '0');
	}
}

// FUNCTION: LEGORACERS 0x00487850
RaceModeSetupScreen::RaceModeSetupScreen()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x004879b0
RaceModeSetupScreen::~RaceModeSetupScreen()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x00487ae0
void RaceModeSetupScreen::Reset()
{
	m_recordToggleMs = 0;
	m_selectedRaceIndex = 0;
	m_showLapTime = 0;
	::memset(m_holderNameBuffer, 0, sizeof(m_holderNameBuffer));
	::memset(m_timeBuffer, 0, sizeof(m_timeBuffer));
	m_holderNameString.CopyFromBufSelection(m_holderNameBuffer, sizeOfArray(m_holderNameBuffer) - 1);
	m_timeString.CopyFromBufSelection(m_timeBuffer, sizeOfArray(m_timeBuffer) - 1);
	MenuGameScreen::Reset();
}

// FUNCTION: LEGORACERS 0x00487b50
void RaceModeSetupScreen::CreateWidgets()
{
	CreateImage(&m_photoImage, 0x49, 0x49);
	SingleRaceSelectBase::CreateWidgets();

	undefined4 textId;
	if (m_menuId != 6) {
		textId = 0x0f;
	}
	else {
		textId =
			0x0e + ((m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) ? 0x0b : 0);
	}

	CreateTextLabel(&m_infoLabel, 0x3a, 0x3a, textId);
	m_infoLabel.WrapText(0x14);
	CreateCarousel(&m_raceCarousel, 0x3d, 0x3b);
	CreateSelector(&m_raceSelector, &m_raceCarousel, 0x69, 0x4c);
	CreateTextButton(&m_startButton, 0x40, 0x46, 0x72);
	CreateTextButton(&m_backButton, 0x3f, 0x43, 2);

	for (LegoS32 i = 0; i < sizeOfArray(m_raceLabels); i++) {
		CreateTextLabel(&m_raceLabels[i], 0x96, 0x37, 0x70);
	}

	if (m_menuId == 0x1d) {
		CreateTextLabel(&m_timeLabel, 0x6a, 0x37, 0x70);
		CreateTextLabel(&m_holderNameLabel, 0x6c, 0x37, 0x70);
		CreateTextLabel(&m_recordTypeLabel, 0x6b, 0x37, 0x70);
		m_showLapTime = 1;
		UpdateBestTimePanel();
		CreateTextLabel(&m_unlockedLabel, 0x6d, 0x37, 0x49);
		m_unlockedLabel.ClearFlags(2);
	}

	// [library:config] The race list (see BuildRaceList), made like the race picker.
	CreateCarousel(&m_listCarousel, 0x3d, 0x3b);
	CreateSelector(&m_listSelector, &m_listCarousel, 0x95, 0x4d);
	for (LegoS32 i = 0; i < sizeOfArray(m_listLabels); i++) {
		CreateTextLabel(&m_listLabels[i], 0x96, 0x37, 0x70);
	}
	CreateModeSelector();
}

// [library:config] LegoRacers4K layout, like Racer Select: title centred, circuit picker below it, the
// preview with the race arrows on either side, the race name below the preview, OK bottom-right and
// MAIN MENU bottom-left.
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

void RaceModeSetupScreen::LayoutCentered()
{
	PlaceWidget(&m_infoLabel, CenterLeft(&m_infoLabel), m_infoLabel.GetRect()->m_top);
	PlaceWidget(&m_trackSelector, CenterLeft(&m_trackSelector), 66);

	// Preview: the frame and the scene inside it move together.
	const LegoS32 frameTop = 112;
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

	// Race picker: big arrows touching the preview on either side, the race name above the
	// preview (under the title). With the race list on, the list takes the picker's place.
	MenuSelector* selector = m_listActive ? &m_listSelector : &m_raceSelector;
	MenuCarouselNavigator* carousel = m_listActive ? &m_listCarousel : &m_raceCarousel;
	const LegoS32 arrowSize = 64;
	const LegoS32 arrowInset = 6; // the arrow art has a margin: overlap it so the arrows touch
	const LegoS32 nameTop = 70;
	LegoS32 nameHeight = carousel->GetRect()->m_bottom - carousel->GetRect()->m_top;
	LegoS32 selectorWidth = frameWidth + 2 * (arrowSize - arrowInset);
	LegoS32 arrowTop = frameTop + (frameHeight >> 1) - (arrowSize >> 1);
	Rect rect = *selector->GetRect();
	rect.m_left = 320 - (selectorWidth >> 1);
	rect.m_right = rect.m_left + selectorWidth;
	rect.m_top = nameTop;
	rect.m_bottom = arrowTop + arrowSize;
	selector->SetRect(&rect);
	Rect arrow;
	arrow.m_left = 0;
	arrow.m_top = arrowTop - nameTop;
	arrow.m_right = arrowSize;
	arrow.m_bottom = arrow.m_top + arrowSize;
	selector->GetPrevButton().SetRect(&arrow);
	arrow.m_left = selectorWidth - arrowSize;
	arrow.m_right = selectorWidth;
	selector->GetNextButton().SetRect(&arrow);
	LegoS32 carouselWidth = carousel->GetRect()->m_right - carousel->GetRect()->m_left;
	PlaceWidget(carousel, (selectorWidth - carouselWidth) >> 1, 0);
	(void) nameHeight;

	// No OK button: the start button is an invisible area over the preview, so clicking the
	// picture or pressing Enter starts. MAIN MENU sits bottom-left, as on every screen.
	static GolString s_empty;
	static undefined2 s_emptyBuffer[1] = {0};
	s_empty.CopyFromBufSelection(s_emptyBuffer, 1);
	m_startButton.SetText(&s_empty);
	m_startButton.HideImage();
	Rect start = *m_borderFrame.GetRect();
	m_startButton.SetRect(&start);
	PlaceWidget(&m_backButton, 3, 418);

	// Time Trial: the best time sits under the preview, left-aligned with it (the arrows now
	// fill its sides): record type on one line, holder and time on the next.
	if (m_menuId == 0x1d) {
		LegoS32 row = frameTop + frameHeight + 4;
		PlaceWidget(&m_recordTypeLabel, frameLeft, row);
		row += 26;
		PlaceWidget(&m_holderNameLabel, frameLeft, row);
		PlaceWidget(&m_timeLabel, frameLeft + frameWidth - 110, row);
	}
}

// [library:config] Every track once, in circuit order (mirrored repeats left out).
void RaceModeSetupScreen::BuildRaceList()
{
	GolString string;
	m_listCount = 0;
	m_listCarousel.RemoveAllItems();
	for (LegoU32 c = 0; c < m_context->m_circuitList.GetEntryCount(); c++) {
		CircuitDefinitionList::CircuitDefinition* circuit = &m_context->m_circuitList.m_entries[c];
		for (LegoU32 r = 0; r < 4 && m_listCount < sizeOfArray(m_listLabels); r++) {
			RaceNameEntry* entry = circuit->GetRaceNameEntry(r);
			if (!entry || entry->GetMirror()) {
				continue;
			}
			LegoBool32 seen = FALSE;
			for (LegoS32 i = 0; i < m_listCount && !seen; i++) {
				RaceNameEntry* other =
					m_context->m_circuitList.m_entries[m_listCircuit[i]].GetRaceNameEntry(m_listRace[i]);
				seen = ::strcmp(other->GetFolderName(), entry->GetFolderName()) == 0;
			}
			if (seen) {
				continue;
			}
			entry->CopyDisplayString(&string);
			m_listLabels[m_listCount].SetString(&string, TRUE);
			m_listCarousel.AddItem(&m_listLabels[m_listCount]);
			m_listCircuit[m_listCount] = static_cast<LegoU8>(c);
			m_listRace[m_listCount] = static_cast<LegoU8>(r);
			m_listCount++;
		}
	}
	m_listCarousel.SetSelection(0);
}

// [library:config] Points the hidden circuit and race pickers at the listed race.
void RaceModeSetupScreen::ApplyListSelection()
{
	LegoU32 index = m_listCarousel.GetSelectedIndex();
	if (static_cast<LegoS32>(index) >= m_listCount) {
		return;
	}
	m_trackCarousel.SetSelection(m_listCircuit[index]);
	OnWidgetValueChanged(&m_trackSelector);
	m_raceCarousel.SetSelection(m_listRace[index]);
	UpdateRacePreview();
}

// FUNCTION: LEGORACERS 0x00487ca0
LegoBool32 RaceModeSetupScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	m_menuId = p_createParams->m_menuId;
	p_createParams->m_menuId = 6;
	m_savedRacerCount = p_context->m_context->m_racerCount;

	LegoBool32 result = SingleRaceSelectBase::Initialize(p_context, p_createParams);
	if (!result) {
		return FALSE;
	}

	OnWidgetValueChanged(&m_trackSelector);
	m_startButton.Select(0);
	m_recordToggleMs = 2500;

	// [library:input] LegoRacers4K: a Versus Grand Prix is picked on the circuit screen
	// (MULTIPLAYER CIRCUIT RACE), so this screen always picks one race.
	if (p_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) {
		CoopVersus_GetSettings()->m_grandPrix = FALSE;
	}
	LegoBool32 grandPrix = FALSE;
	if (grandPrix) {
		m_raceSelector.ClearFlags(2);
	}

	// [library:config] Otherwise one list of every race replaces the circuit and race pickers.
	m_listActive = !grandPrix;
	if (m_listActive) {
		m_trackSelector.ClearFlags(2);
		m_raceSelector.ClearFlags(2);
		// Hidden pickers would still take left/right; the list takes them instead.
		m_trackSelector.SetAcceptUnfocusedInput(0);
		m_raceSelector.SetAcceptUnfocusedInput(0);
		m_listSelector.SetAcceptUnfocusedInput(1);
		BuildRaceList();
		ApplyListSelection();
	}
	else {
		m_listSelector.ClearFlags(2);
	}
	LayoutCentered();
	if (!m_listActive) {
		// Grand Prix: the circuit picker takes the race name's place above the preview.
		PlaceWidget(&m_trackSelector, CenterLeft(&m_trackSelector), 70);
	}

	// [library:config] English: the Time Trial title says TIME TRIAL.
	if (m_menuId == 0x1d) {
		const undefined2* language =
			m_menuTextStrings->GetStringBuffer(static_cast<LegoU16>(0x9d + p_context->m_saveSystem.GetLanguageIndex()));
		static const char c_english[] = "ENGLISH";
		LegoBool32 english = language != NULL;
		for (LegoS32 i = 0; english && c_english[i]; i++) {
			LegoU16 c = language[i];
			if (c >= 'a' && c <= 'z') {
				c -= 'a' - 'A';
			}
			english = c == static_cast<LegoU8>(c_english[i]);
		}
		if (english) {
			static GolString s_title;
			static undefined2 s_titleBuffer[16];
			static const char c_title[] = "TIME TRIAL";
			LegoS32 length = 0;
			for (; c_title[length]; length++) {
				s_titleBuffer[length] = static_cast<LegoU8>(c_title[length]);
			}
			s_titleBuffer[length] = 0;
			s_title.CopyFromBufSelection(s_titleBuffer, static_cast<undefined2>(length + 1));
			m_infoLabel.SetString(&s_title, 0);
			PlaceWidget(&m_infoLabel, CenterLeft(&m_infoLabel), m_infoLabel.GetRect()->m_top);
		}
	}
	LayoutModeSelector(&m_infoLabel, m_menuId == 0x1d ? 2 : 1);
	return TRUE;
}

// FUNCTION: LEGORACERS 0x00487d10
void RaceModeSetupScreen::OnIconSelected(MenuIcon* p_icon)
{
	m_selectedIcon = p_icon;
	UpdateRacePreview();
}

// FUNCTION: LEGORACERS 0x00487d30
void MenuGameScreen::OnIconDeselected(MenuIcon*)
{
	m_selectedIcon = NULL;
}

// FUNCTION: LEGORACERS 0x00487d40
void RaceModeSetupScreen::OnIconUnfocused(MenuWidget* p_source)
{
	MenuWidget* source = p_source;
	if (source == &m_startButton) {
		CircuitDefinitionList::CircuitDefinition* circuitDefinition = m_circuitEntry;
		if (circuitDefinition) {
			LegoRacers::Context* context = m_context->m_context;
			::memcpy(context->m_circuitName, circuitDefinition->GetName(), sizeof(GolName));

			RaceNameEntry* raceNameEntry = m_circuitEntry->GetRaceNameEntry(m_raceCarousel.GetSelectedIndex());
			if (raceNameEntry) {
				::memcpy(context->m_raceSlots[0].m_raceName, raceNameEntry->GetName(), sizeof(GolName));
				::memcpy(context->m_raceSlots[0].m_folderName, raceNameEntry->GetFolderName(), sizeof(GolName));
				context->m_raceSlots[0].m_enabled = TRUE;
				context->m_raceSlots[0].m_mirror = raceNameEntry->GetMirror();
			}

			// Both branches assign the same id in the original; the flag test is real but inert
			if (m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) {
				// [library:input] LegoRacers4K: all players pick at once; a Grand Prix runs every
				// race of the chosen circuit.
				if (CoopVersus_GetSettings()->m_grandPrix) {
					for (LegoU32 i = 0; i < sizeOfArray(context->m_raceSlots); i++) {
						LegoRacers::RaceSlot* slot = &context->m_raceSlots[i];
						RaceNameEntry* entry = m_circuitEntry->GetRaceNameEntry(i);
						slot->m_enabled = entry != NULL;
						slot->m_mirror = entry ? entry->GetMirror() : 0;
						::memcpy(slot->m_raceName, entry ? entry->GetName() : "", entry ? sizeof(GolName) : 1);
						::memcpy(slot->m_folderName, entry ? entry->GetFolderName() : "", entry ? sizeof(GolName) : 1);
					}
				}
				m_nextMenuId = 0x1e;
			}
			else {
				m_nextMenuId = 0x13;
			}
		}
	}
	else if (source == &m_backButton) {
		m_context->m_modelBuilder.SetMenuFlowFlags(
			m_context->m_modelBuilder.GetMenuFlowFlags() & ~DriverModelBuilder::c_menuFlowVersus
		);
		m_nextMenuId = 2;
	}

	if (m_nextMenuId != 0xffff) {
		m_navPending = TRUE;
	}

	m_clickedWidget = source;
}

// FUNCTION: LEGORACERS 0x00487e10
void RaceModeSetupScreen::OnWidgetValueChanged(MenuWidget* p_source)
{
	GolString string;
	LegoBool32 isComplete = FALSE;

	if (HandleModeChanged(p_source)) {
		return;
	}
	// [library:config] The race list drives the hidden pickers.
	if (p_source == &m_listSelector) {
		ApplyListSelection();
		return;
	}

	if (p_source == &m_trackSelector) {
		LegoU32 circuitIndex = m_trackCarousel.GetSelectedIndex();
		if (circuitIndex >= m_context->m_circuitList.GetEntryCount()) {
			m_circuitEntry = NULL;
		}
		else {
			m_circuitEntry = &m_context->m_circuitList.m_entries[circuitIndex];
		}

		if (m_circuitEntry) {
			LegoU8 mask = static_cast<LegoU8>(1 << m_context->m_circuitList.GetEntryIndex(m_circuitEntry));
			LegoU8 flags = m_context->m_saveSystem.GetGameState().GetUnlockedCircuits();
			if (flags & mask) {
				isComplete = TRUE;
			}
		}

		m_raceCarousel.RemoveAllItems();
		for (LegoS32 i = 0; i < sizeOfArray(m_raceLabels); i++) {
			RaceNameEntry* raceNameEntry = m_circuitEntry->GetRaceNameEntry(i);
			if (raceNameEntry) {
				raceNameEntry->CopyDisplayString(&string);
				m_raceLabels[i].SetString(&string, TRUE);
				m_raceCarousel.AddItem(&m_raceLabels[i]);
			}
		}
		m_raceCarousel.SetSelection(0);

		if (isComplete) {
			m_startButton.Enable(5);
			m_sceneOverlay.ClearFlags(2);
		}
		else {
			m_startButton.Disable(5);
			m_sceneOverlay.SetFlags(2);
		}
	}

	UpdateRacePreview();
}

// FUNCTION: LEGORACERS 0x00487f90
void RaceModeSetupScreen::Navigate()
{
	if (NavigateModeSwitch()) { // [library:config]
		m_context->m_context->m_racerCount = m_savedRacerCount;
		return;
	}

	if (m_nextMenuId == 2) {
		m_context->m_context->m_racerCount = m_savedRacerCount;
		m_context->m_menuStack.Pop();
		return;
	}

	if (m_menuId == 6) {
		m_context->m_context->m_raceMode = LegoRacers::Context::c_raceModeSingle;
	}
	else {
		m_context->m_context->m_raceMode = LegoRacers::Context::c_raceModeTimeRace;
		m_context->m_context->m_racerCount = 0;
	}

	m_context->m_menuStack.Push(m_nextMenuId);
}

// FUNCTION: LEGORACERS 0x00488010
void RaceModeSetupScreen::UpdateRacePreview()
{
	GolName frameName;
	GolName driverName;
	GolName raceName;
	driverName[0] = '\0';

	LegoU32 selectedEntryIndex = m_raceCarousel.GetSelectedIndex();
	RaceNameEntry* raceNameEntry = m_circuitEntry->GetRaceNameEntry(selectedEntryIndex);
	if (!raceNameEntry) {
		return;
	}

	::memcpy(frameName, raceNameEntry->GetThemeName(), sizeof(GolName));
	if (m_menuId != 0x1d) {
		::memcpy(driverName, raceNameEntry->GetMascotName(), sizeof(GolName));
	}
	else {
		::strcpy(driverName, g_veronicaVoltageName);
	}

	GolNameTable* frameNames = &m_sceneWidget.m_definition;
	CutsceneDefinition::Frame* frame;
	if (!frameNames->GetNameEntries()) {
		frame = NULL;
	}
	else {
		frame = static_cast<CutsceneDefinition::Frame*>(frameNames->GetName(frameName));
	}

	LegoChar* raceNameSource = raceNameEntry->GetName();
	::memcpy(raceName, raceNameSource, sizeof(GolName));
	m_selectedRaceIndex = m_context->m_raceNames.GetEntryIndexByName(raceName);
	if (frame && frame != m_sceneWidget.m_frame) {
		m_sceneWidget.SetFrame(frame);
		m_sceneWidget.m_frame->SetFlags(CutsceneDefinition::Frame::c_flagLoop);
		m_showLapTime = TRUE;
		UpdateBestTimePanel();
	}

	LegoU32 circuitIndex = m_context->m_circuitList.GetEntryIndex(m_circuitEntry);
	selectedEntryIndex += circuitIndex * 4;
	ApplyThemeColor(selectedEntryIndex);
	if (driverName[0]) {
		SetPreviewDriver(driverName);
	}
}

// FUNCTION: LEGORACERS 0x00488150
LegoBool32 RaceModeSetupScreen::Update(undefined4 p_elapsed)
{
	// [library:config] LegoRacers4K: a mode arrow switches screens at once (hover focus clears
	// m_navPending before the base Update sees it).
	if (m_modeSwitchMenuId) {
		Navigate();
		return FALSE;
	}

	if (p_elapsed > m_recordToggleMs) {
		m_showLapTime = (m_showLapTime == 0);
		UpdateBestTimePanel();
	}
	else {
		m_recordToggleMs -= p_elapsed;
	}

	return MenuSceneScreen::Update(p_elapsed);
}

// FUNCTION: LEGORACERS 0x004881a0
void RaceModeSetupScreen::UpdateBestTimePanel()
{
	GolString string;
	LegoU32 bestTime = 0xffffffff;

	if (m_menuId != 0x1d) {
		return;
	}

	::memset(m_holderNameBuffer, 0, sizeof(m_holderNameBuffer));
	::memset(m_timeBuffer, 0, sizeof(m_timeBuffer));

	undefined2 buffer[15];
	::memset(buffer, 0, sizeof(buffer));
	string.CopyFromBufSelection(buffer, 14);

	LegoBool32 raceTime = (m_showLapTime == FALSE);
	LegoU16 unlockedMask = m_context->m_saveSystem.GetGameState().GetUnlockedRaces();
	LegoU32 time = m_context->m_saveSystem.GetGameState().GetBestTime(m_selectedRaceIndex, raceTime, &string);
	if (time && time < 0xffffffff) {
		bestTime = time;
		m_holderNameString.GolStrcpy(&string);
	}

	if (m_showLapTime) {
		m_recordTypeLabel.SetStringByIndex(0x46, 0);
	}
	else {
		m_recordTypeLabel.SetStringByIndex(0x47, 0);
	}

	if (bestTime != 0xffffffff) {
		LegoChar time[9];
		FormatTime(time, bestTime);
		GolString::CopyStringToBuf16(time, m_timeBuffer);
		m_holderNameLabel.SetString(&m_holderNameString, 0);
	}
	else if (m_showLapTime) {
		m_holderNameLabel.SetStringByIndex(0xbe, 0);
	}
	else {
		m_holderNameLabel.SetStringByIndex(0xbf, 0);
	}

	m_timeLabel.SetString(&m_timeString, 0);
	m_recordToggleMs = 2500;

	// [library:config] LegoRacers4K: every race is unlocked, so the "won" note would always show.
	if (FALSE && static_cast<LegoU16>(unlockedMask & (1 << m_selectedRaceIndex))) {
		m_unlockedLabel.SetFlags(2);
	}
	else {
		m_unlockedLabel.ClearFlags(2);
	}
}
