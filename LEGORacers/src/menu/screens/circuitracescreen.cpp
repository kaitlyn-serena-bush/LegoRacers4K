#include "menu/screens/circuitracescreen.h"

#include "camera/golcamera.h"
#include "core/gol.h"
#include "golanimatedentity.h"
#include "golhashtable.h"
#include "golname.h"
#include "golstring.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"
#include "race/data/racenameentry.h"
#include "world/golworlddatabase.h"

#include <string.h>

DECOMP_SIZE_ASSERT(CircuitRaceScreen, 0x2228)

// [library:3d] Where a preview starts playing: just after its champion steps into the scene, so all
// four champions are there from the first frame (p_staggerMs offsets the cells from each other while
// keeping the champion on screen).
static LegoS32 ChampionStartMs(CutsceneDefinition::Frame* p_frame, GolWorldEntity* p_driver, LegoS32 p_staggerMs)
{
	LegoU32 rate = p_frame->GetPlaybackRate();
	if (!rate) {
		return 0;
	}

	LegoU32 start = 0;
	LegoU32 end = 0;
	LegoBool32 found = FALSE;
	for (LegoU32 i = 0; i < p_frame->GetModelEventCount(); i++) {
		CutsceneDefinition::Frame::ModelEvent* model = p_frame->GetModel(i);
		if (model->GetEntity() == p_driver) {
			if (!found || model->m_startFrame < start) {
				start = model->m_startFrame;
			}
			if (!found || model->m_endFrame > end) {
				end = model->m_endFrame;
			}
			found = TRUE;
		}
	}
	if (!found) {
		return 0;
	}

	LegoS32 startMs = static_cast<LegoS32>(start * 1000 / rate) + 40;
	LegoS32 endMs = static_cast<LegoS32>(end * 1000 / rate);
	LegoS32 stagger = p_staggerMs;
	if (startMs + stagger > endMs - 1500) {
		stagger = endMs - 1500 - startMs;
	}
	if (stagger < 0) {
		stagger = 0;
	}
	return startMs + stagger;
}

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
	m_quadActive = FALSE; // [library:3d]
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
	CreateQuadWidgets();  // [library:3d]
	CreateModeSelector(); // [library:config]
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

	// Circuit selector: big arrows touching the preview on either side, level with its middle;
	// its own picture carousel is not shown.
	const LegoS32 arrowSize = 64;
	const LegoS32 arrowInset = 6; // the arrow art has a margin: overlap it so the arrows touch
	LegoS32 selectorWidth = frameWidth + 2 * (arrowSize - arrowInset);
	LegoS32 selectorTop = frameTop + (frameHeight >> 1) - (arrowSize >> 1);
	Rect selector = *m_trackSelector.GetRect();
	selector.m_left = 320 - (selectorWidth >> 1);
	selector.m_right = selector.m_left + selectorWidth;
	selector.m_top = selectorTop;
	selector.m_bottom = selectorTop + arrowSize;
	m_trackSelector.SetRect(&selector);
	Rect arrow;
	arrow.m_left = 0;
	arrow.m_top = 0;
	arrow.m_right = arrowSize;
	arrow.m_bottom = arrowSize;
	m_trackSelector.GetPrevButton().SetRect(&arrow);
	arrow.m_left = selectorWidth - arrowSize;
	arrow.m_right = selectorWidth;
	m_trackSelector.GetNextButton().SetRect(&arrow);
	PlaceWidget(&m_trackCarousel, (selectorWidth - arrowSize) >> 1, 0);
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
	LayoutModeSelector(&m_infoLabel, 0);
	ShowAllRaces();
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
void CircuitRaceScreen::OnWidgetValueChanged(MenuWidget* p_source)
{
	if (HandleModeChanged(p_source)) { // [library:config]
		return;
	}
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

	if (NavigateModeSwitch()) { // [library:config]
		return;
	}

	if (m_nextMenuId == 2) {
		// [library:config] LegoRacers4K: MULTIPLAYER CIRCUIT RACE returns to the main menu too.
		m_context->m_modelBuilder.SetMenuFlowFlags(
			m_context->m_modelBuilder.GetMenuFlowFlags() & ~DriverModelBuilder::c_menuFlowVersus
		);
		m_context->m_menuStack.Pop();
		return;
	}

	// [library:input] LegoRacers4K: in Multiplayer, every player picks at once and the circuit's
	// races run back to back (the race slots were filled when the circuit was picked).
	if (m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) {
		m_context->m_context->m_raceMode = LegoRacers::Context::c_raceModeSingle;
		m_context->m_menuStack.Push(0x1e);
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

	ShowAllRaces();
	m_previewChanged = TRUE;
}

// [library:3d] LegoRacers4K: all of a circuit's races are previewed at once in a 2x2 grid, under
// the champion's circuit name.
void CircuitRaceScreen::ShowAllRaces()
{
	static const char* const c_names[7] = {
		"CAPTAIN REDBEARD'S RAMPAGE",
		"KING KAHUKA'S CLASH",
		"BASIL'S BATTY BRICK BASH",
		"JOHNNY THUNDER'S JUNGLE JAUNT",
		"BARON'S BATTLEFIELD BLITZ",
		"GYPSY MOTH'S MOONLIT MARATHON",
		"ROCKET RACER'S RALLY"
	};
	static GolString s_strings[7];
	static undefined2 s_buffers[7][48];

	if (m_circuitIndex < 7) {
		LegoU32 length = 0;
		for (; c_names[m_circuitIndex][length]; length++) {
			s_buffers[m_circuitIndex][length] = static_cast<LegoU8>(c_names[m_circuitIndex][length]);
		}
		s_buffers[m_circuitIndex][length] = 0;
		s_strings[m_circuitIndex].CopyFromBufSelection(s_buffers[m_circuitIndex], static_cast<undefined2>(length + 1));
		m_raceNameLabel.SetString(&s_strings[m_circuitIndex], 0);
	}

	// Four independent scenes: the main widget plus three more, each with its own cutscene data,
	// champion and animation clock.
	LegoS32 raceCount = 0;
	for (LegoU32 i = 0; i < 4; i++) {
		if (m_circuitEntry->GetRaceNameEntry(i)) {
			raceCount++;
		}
	}

	m_quadActive = m_circuitIndex != 6 && raceCount >= 2 && m_sceneWidget.m_frame;
	for (LegoS32 i = 0; i < 3; i++) {
		if (m_quadActive) {
			m_quadWidgets[i].SetFlags(2);
		}
		else {
			m_quadWidgets[i].ClearFlags(2);
		}
	}

	if (!m_quadActive) {
		return;
	}

	const Rect full = m_sceneWidget.m_viewportRect;
	LegoS32 width = full.m_right - full.m_left;
	LegoS32 height = full.m_bottom - full.m_top;
	const LegoS32 gap = 2;
	for (LegoS32 i = 0; i < 4; i++) {
		RaceNameEntry* entry = m_circuitEntry->GetRaceNameEntry(i);
		MenuSceneScreen::SceneWidget* widget = i == 0 ? &m_sceneWidget : &m_quadWidgets[i - 1];
		if (!entry || !widget->m_frame) {
			continue;
		}

		Rect cell;
		cell.m_left = full.m_left + (i & 1) * ((width + gap) >> 1);
		cell.m_right = cell.m_left + ((width - gap) >> 1);
		cell.m_top = full.m_top + (i >> 1) * ((height + gap) >> 1);
		cell.m_bottom = cell.m_top + ((height - gap) >> 1);

		GolName frameName;
		::memcpy(frameName, entry->GetThemeName(), sizeof(GolName));
		GolNameTable* frameNames = &widget->m_definition;
		CutsceneDefinition::Frame* frame = NULL;
		if (frameNames->GetNameEntries()) {
			frame = static_cast<CutsceneDefinition::Frame*>(frameNames->GetName(frameName));
		}

		if (frame && (i > 0 || frame != widget->m_frame)) {
			widget->SetFrame(frame);
		}
		// Each preview plays its animation through once and then starts again from the top.
		widget->m_frame->ClearFlags(CutsceneDefinition::Frame::c_flagLoop);
		widget->m_frame->SetViewportRect(&cell);
		// Start each preview at a different point of its loop (revisited circuits begin from a clean state).
		widget->m_finished = FALSE;
		widget->m_frame->Rewind();
		widget->m_frame->Play();
		widget->m_player.StopAll();
		if (i > 0) {
			SetQuadDriver(i - 1, entry->GetMascotName());
		}
		widget->m_frame->Update(ChampionStartMs(widget->m_frame, QuadDriver(i), i * 700));
	}
}

// [library:3d] The champion of one of the extra preview scenes (same recipe as SetPreviewDriver).
void CircuitRaceScreen::SetQuadDriver(LegoS32 p_index, const LegoChar* p_name)
{
	MenuSceneScreen::SceneWidget* widget = &m_quadWidgets[p_index];
	GolAnimatedEntity* modelEntity = NULL;
	for (LegoU32 i = 0; !modelEntity && i < widget->m_definition.GetWorldDatabaseCount(); i++) {
		GolWorldDatabase* worldDatabase = widget->m_definition.GetWorldDatabase(i);
		if (worldDatabase->GetAnimatedEntityEntries()) {
			modelEntity = worldDatabase->GetAnimatedEntityByName("guy1");
		}
	}
	if (!modelEntity || !p_name[0]) {
		return;
	}

	GolAnimatedEntity* driver = &m_quadDrivers[p_index];
	LegoChar* name = m_quadNames[p_index];
	if (::strncmp(name, p_name, sizeof(m_quadNames[p_index])) != 0 || !driver->HasModel()) {
		::strncpy(name, p_name, sizeof(m_quadNames[p_index]));

		if (driver->HasModel()) {
			driver->ResetModelState();
			m_context->m_modelBuilder.RefreshMenuResources();
		}
		if (m_quadModels[p_index]) {
			m_golExport->DestroyModel(m_quadModels[p_index]);
			m_quadModels[p_index] = NULL;
		}

		DriverCosmetics cosmetics;
		m_context->m_cosmeticTable.CopyCosmetics(name, &cosmetics);
		m_quadModels[p_index] = m_context->m_modelBuilder.BuildDriverModel(&cosmetics, NULL, 0);
		driver->SetModel(
			m_quadModels[p_index],
			modelEntity->GetSceneNode(0),
			modelEntity->GetModelPart(),
			modelEntity->GetModelDistance(0)
		);
	}

	CutsceneDefinition::Frame* frame = widget->m_frame;
	for (LegoU32 i = 0; i < frame->GetModelEventCount(); i++) {
		CutsceneDefinition::Frame::ModelEvent* model = frame->GetModel(i);
		if (model->GetEntity() == modelEntity) {
			model->SetEntity(driver);
		}
	}
}

// [library:3d] The extra preview scenes and their champions.
void CircuitRaceScreen::CreateQuadWidgets()
{
	if (g_hashTable) {
		g_hashTable->SetCurrentEntryFromString("MENUDATA\\SINGRACE");
	}
	for (LegoS32 i = 0; i < 3; i++) {
		CreateRegion(&m_quadWidgets[i], 6);
		m_quadWidgets[i].m_skippable = FALSE;
		m_quadNames[i][0] = '\0';
		m_quadModels[i] = NULL;
	}
	if (g_hashTable) {
		g_hashTable->SetCurrentEntryFromString("MENUDATA");
	}
}

LegoBool32 CircuitRaceScreen::Destroy()
{
	if (m_initialized) {
		for (LegoS32 i = 0; i < 3; i++) {
			m_quadDrivers[i].ResetModelState();
			if (m_quadModels[i]) {
				m_golExport->DestroyModel(m_quadModels[i]);
				m_quadModels[i] = NULL;
			}
			m_quadNames[i][0] = '\0';
			m_quadWidgets[i].Destroy();
		}
	}

	return SingleRaceSelectBase::Destroy();
}

// FUNCTION: LEGORACERS 0x0047a3f0
LegoBool32 CircuitRaceScreen::Update(undefined4 p_elapsed)
{
	// [library:config] LegoRacers4K: a mode arrow switches screens at once (hover focus clears
	// m_navPending before the base Update sees it).
	if (m_modeSwitchMenuId) {
		Navigate();
		return FALSE;
	}

	if (m_transition && !m_transition->IsActive()) {
		m_transition = NULL;
	}

	if (p_elapsed > m_previewTimerMs && m_circuitIndex != 6 && !m_quadActive) {
		m_previewRaceIndex = (m_previewRaceIndex + 1) & 3;
		UpdateRacePreview();
	}
	else {
		m_previewTimerMs -= p_elapsed;
	}

	if (m_previewTimerMs < 250 && !m_context->m_menuAnimations.HasActive() && m_circuitIndex != 6 && !m_quadActive) {
		if (m_transition && m_transition->IsActive()) {
			m_context->m_menuAnimations.Deactivate(m_transition);
			m_transition = NULL;
		}

		GolCamera* rectSource = m_sceneWidget.m_frame->GetActiveCamera();
		m_transition = m_context->m_menuAnimations.Activate(250, FALSE, NULL, rectSource);
	}

	if (m_quadActive) {
		for (LegoS32 i = 0; i < 4; i++) {
			MenuSceneScreen::SceneWidget* widget = i == 0 ? &m_sceneWidget : &m_quadWidgets[i - 1];
			if (widget->m_frame &&
				(widget->m_finished || (widget->m_frame->GetFlags() & CutsceneDefinition::Frame::c_flagComplete))) {
				widget->m_finished = FALSE;
				widget->m_frame->Rewind();
				widget->m_frame->Play();
				widget->m_frame->Update(ChampionStartMs(widget->m_frame, QuadDriver(i), 0));
				widget->m_player.StopAll();
			}
		}
	}

	LegoBool32 result = MenuSceneScreen::Update(p_elapsed);
	if (m_previewChanged && m_quadActive) {
		m_previewChanged = FALSE;
	}
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
