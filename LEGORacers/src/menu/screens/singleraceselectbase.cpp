#include "menu/screens/singleraceselectbase.h"

#include "app/coopcontrols.h"
#include "core/gol.h"
#include "golanimatedentity.h"
#include "golhashtable.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"
#include "menu/widgets/visualstatecolor.h"
#include "save/savesystem.h"
#include "world/golworlddatabase.h"

DECOMP_SIZE_ASSERT(SingleRaceSelectBase, 0x1908)

// GLOBAL: LEGORACERS 0x004c2278
static VisualStateColor g_singleRaceVisualStates[] = {
	{{0xffadadad}},
	{{0xff00dfff}},
	{{0xffb4dce6}},
	{{0xffdf3d25}},
	{{0x1f1fdaff}},
};

// GLOBAL: LEGORACERS 0x004c228c
static LegoU8 g_singleRaceVisualStateMap[] = {
	1, 0, 3, 2, 1, 0, 3, 2, 0, 1, 2, 3, 2, 0, 3, 1, 2, 0, 3, 1, 3, 1, 2, 0, 4,
};

// FUNCTION: LEGORACERS 0x00488750
SingleRaceSelectBase::SingleRaceSelectBase()
{
	MenuGameScreen::Reset();
	m_circuitEntry = 0;
	m_driverModel = 0;
	m_driverName[0] = '\0';
}

// FUNCTION: LEGORACERS 0x00488840
SingleRaceSelectBase::~SingleRaceSelectBase()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x004888f0
LegoBool32 SingleRaceSelectBase::Destroy()
{
	MenuGameContext* context = m_context;

	if (!m_initialized) {
		return TRUE;
	}

	m_driverEntity.ResetModelState();
	if (m_driverModel) {
		m_golExport->DestroyModel(m_driverModel);
		m_driverModel = NULL;
	}

	ClearCosmeticTable();
	m_driverName[0] = '\0';
	m_circuitEntry = NULL;

	LegoBool32 result = MenuSceneScreen::Destroy();
	context->m_carBuildModel.AcquireBuffers();

	return result;
}

// FUNCTION: LEGORACERS 0x00488970
void SingleRaceSelectBase::CreateWidgets()
{
	LegoS32 optionCount = 3 + ((m_menuId != 0x1d) ? 4 : 0);

	CreateFrame(&m_borderFrame, 0x47, 0x47);

	if (g_hashTable) {
		g_hashTable->SetCurrentEntryFromString("MENUDATA\\SINGRACE");
	}

	CreateRegion(&m_sceneWidget, 6);
	m_sceneWidget.m_frame->SetFlags(CutsceneDefinition::Frame::c_flagLoop);

	if (g_hashTable) {
		g_hashTable->SetCurrentEntryFromString("MENUDATA");
	}

	CreateTriangle(&m_sceneOverlay, 0x5e);

	Rect rect;
	rect.m_top = 0;
	rect.m_left = 0;
	rect.m_right = m_sceneWidget.GetRect()->m_right - m_sceneWidget.GetRect()->m_left;
	rect.m_bottom = m_sceneWidget.GetRect()->m_bottom - m_sceneWidget.GetRect()->m_top;
	m_sceneOverlay.SetRect(&rect);
	m_sceneOverlay.SetAlphaOverride(200);
	m_sceneOverlay.ClearFlags(2);

	CreateCarousel(&m_trackCarousel, 0x3e, 0x3b);
	CreateSelector(&m_trackSelector, &m_trackCarousel, 0x95, 0x4d);

	for (LegoS32 i = 0; i < optionCount; i++) {
		CreateImage(&m_trackIcons[i], static_cast<undefined2>(i + 0x61), 0x9d);
		m_trackCarousel.AddItem(&m_trackIcons[i]);
	}

	m_trackCarousel.SetSelection(0);
}

// FUNCTION: LEGORACERS 0x00488ac0
LegoBool32 SingleRaceSelectBase::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	m_circuitEntry = NULL;

	p_context->m_carBuildModel.ReleaseBuffers();
	LegoBool32 result = MenuSceneScreen::Initialize(p_context, p_createParams);
	if (!result) {
		return result;
	}

	m_cursor->SetCursorEnabled(TRUE);

	if (!p_context->m_modelBuilder.HasMenuResources()) {
		LoadPartResources(m_context, 0);
	}

	LoadCosmeticTable();
	m_sceneWidget.m_skippable = FALSE;
	return TRUE;
}

// FUNCTION: LEGORACERS 0x00488b40
void SingleRaceSelectBase::SetPreviewDriver(const LegoChar* p_name)
{
	GolAnimatedEntity* modelEntity = NULL;
	LegoU32 i = 0;
	CutsceneDefinition::Frame* frame = m_sceneWidget.m_frame;

	while (!modelEntity) {
		if (i >= m_sceneWidget.m_definition.GetWorldDatabaseCount()) {
			break;
		}

		GolWorldDatabase* worldDatabase = m_sceneWidget.m_definition.GetWorldDatabase(i);
		if (!modelEntity && worldDatabase->GetAnimatedEntityEntries()) {
			modelEntity = worldDatabase->GetAnimatedEntityByName("guy1");
		}

		i++;
	}

	LegoChar* name = m_driverName;
	if (::strncmp(name, p_name, sizeof(m_driverName)) != 0) {
		::strncpy(name, p_name, sizeof(m_driverName));

		if (m_driverEntity.HasModel()) {
			m_driverEntity.ResetModelState();
			m_context->m_modelBuilder.RefreshMenuResources();
		}

		if (m_driverModel) {
			m_golExport->DestroyModel(m_driverModel);
			m_driverModel = NULL;
		}

		DriverCosmetics cosmetics;
		m_context->m_cosmeticTable.CopyCosmetics(name, &cosmetics);
		m_driverModel = m_context->m_modelBuilder.BuildDriverModel(&cosmetics, NULL, 0);
		m_driverEntity.SetModel(
			m_driverModel,
			modelEntity->GetSceneNode(0),
			modelEntity->GetModelPart(),
			modelEntity->GetModelDistance(0)
		);
	}

	if (m_driverEntity.HasModel()) {
		for (LegoU32 i = 0; i < frame->GetModelEventCount(); i++) {
			CutsceneDefinition::Frame::ModelEvent* model = frame->GetModel(i);
			if (model->GetEntity() == modelEntity) {
				model->SetEntity(&m_driverEntity);
			}
		}
	}
}

// FUNCTION: LEGORACERS 0x00488cb0
void SingleRaceSelectBase::ApplyThemeColor(LegoS32 p_index)
{
	m_borderFrame.SetBorderColors(&g_singleRaceVisualStates[g_singleRaceVisualStateMap[p_index]]);
}

// [library:config] The Single Player / Multiplayer mode picker (see the header). Created by the
// circuit and race screens.
static void SetAscii(GolString* p_string, undefined2* p_buffer, const char* p_text)
{
	LegoS32 length = 0;
	for (; p_text[length]; length++) {
		p_buffer[length] = static_cast<LegoU8>(p_text[length]);
	}
	p_buffer[length] = 0;
	p_string->CopyFromBufSelection(p_buffer, static_cast<undefined2>(length + 1));
}

static void MoveWidget(MenuWidget* p_widget, LegoS32 p_left, LegoS32 p_top)
{
	Rect rect = *p_widget->GetRect();
	rect.m_right += p_left - rect.m_left;
	rect.m_bottom += p_top - rect.m_top;
	rect.m_left = p_left;
	rect.m_top = p_top;
	p_widget->SetRect(&rect);
}

void SingleRaceSelectBase::CreateModeSelector()
{
	CreateCarousel(&m_modeCarousel, 0x3e, 0x3b);
	CreateSelector(&m_modeSelector, &m_modeCarousel, 0x95, 0x4d);
	static const undefined2 c_titles[3] = {0x0d, 0x0e, 0x0f};
	for (LegoS32 i = 0; i < 3; i++) {
		CreateTextLabel(&m_modeLabels[i], 0x3a, 0x3a, c_titles[i]);
	}

	// Multiplayer: number of players (2-4) and laps (1-5), shown only in the Versus flow.
	static const char* const c_counts[c_versusTextCount] =
		{"2 PLAYERS", "3 PLAYERS", "4 PLAYERS", "1 LAP", "2 LAPS", "3 LAPS", "4 LAPS", "5 LAPS"};
	CreateCarousel(&m_playersCarousel, 0x3e, 0x3b);
	CreateSelector(&m_playersSelector, &m_playersCarousel, 0x95, 0x4d);
	CreateCarousel(&m_lapsCarousel, 0x3e, 0x3b);
	CreateSelector(&m_lapsSelector, &m_lapsCarousel, 0x95, 0x4d);
	for (LegoS32 i = 0; i < c_versusTextCount; i++) {
		MenuTextLabel* label = i < 3 ? &m_playersLabels[i] : &m_lapsLabels[i - 3];
		CreateTextLabel(label, 0x3a, 0x37, 0x0d);
		SetAscii(&m_versusStrings[i], m_versusBuffers[i], c_counts[i]);
		label->SetString(&m_versusStrings[i], TRUE);
		(i < 3 ? &m_playersCarousel : &m_lapsCarousel)->AddItem(label);
	}
	m_playersSelector.ClearFlags(2);
	m_lapsSelector.ClearFlags(2);
}

// A compact picker: small arrows either side of a fixed-width value.
static void LayoutSmallSelector(
	MenuSelector* p_selector,
	MenuCarouselNavigator* p_carousel,
	LegoS32 p_center,
	LegoS32 p_top
)
{
	const LegoS32 arrowSize = 32;
	const LegoS32 valueWidth = 130;
	LegoS32 width = valueWidth + 2 * arrowSize;
	LegoS32 carouselHeight = p_carousel->GetRect()->m_bottom - p_carousel->GetRect()->m_top;
	LegoS32 height = carouselHeight > arrowSize ? carouselHeight : arrowSize;
	Rect rect;
	rect.m_left = p_center - (width >> 1);
	rect.m_right = rect.m_left + width;
	rect.m_top = p_top;
	rect.m_bottom = p_top + height;
	p_selector->SetRect(&rect);
	Rect arrow;
	arrow.m_left = 0;
	arrow.m_top = (height - arrowSize) >> 1;
	arrow.m_right = arrowSize;
	arrow.m_bottom = arrow.m_top + arrowSize;
	p_selector->GetPrevButton().SetRect(&arrow);
	arrow.m_left = width - arrowSize;
	arrow.m_right = width;
	p_selector->GetNextButton().SetRect(&arrow);
	Rect value;
	value.m_left = arrowSize;
	value.m_right = arrowSize + valueWidth;
	// The value's text sits low in its box: a taller box, raised, lines it up with the arrows.
	value.m_top = arrow.m_top - 20;
	value.m_bottom = arrow.m_bottom + 4;
	p_carousel->SetRect(&value);
	p_selector->SetAcceptUnfocusedInput(1);
	p_selector->Enable(5);
	p_selector->SetFlags(2);
}

void SingleRaceSelectBase::LayoutModeSelector(MenuWidget* p_title, LegoS32 p_mode)
{
	m_modeVersus = (m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) != 0;
	m_modeSelectorActive = TRUE;
	LegoS32 modeCount = 3;

	if (m_modeVersus) {
		// Multiplayer: MULTIPLAYER SINGLE RACE <-> MULTIPLAYER CIRCUIT RACE (menus 6 and 5); the
		// circuit screen runs every race of the circuit.
		static GolString s_titles[2];
		static undefined2 s_titleBuffers[2][32];
		SetAscii(&s_titles[0], s_titleBuffers[0], "MULTIPLAYER SINGLE RACE");
		SetAscii(&s_titles[1], s_titleBuffers[1], "MULTIPLAYER CIRCUIT RACE");
		modeCount = 2;
		p_mode = p_mode == 0 ? 1 : 0;
		for (LegoS32 i = 0; i < 2; i++) {
			m_modeLabels[i].SetString(&s_titles[i], 0);
		}
		// The third label is not in the carousel, so it would draw on its own over the title.
		m_modeLabels[2].ClearFlags(2);
		MenuTextLabel* title = static_cast<MenuTextLabel*>(p_title);
		title->SetString(&s_titles[p_mode], TRUE);
		LegoS32 titleWidth = title->GetRect()->m_right - title->GetRect()->m_left;
		MoveWidget(title, 320 - (titleWidth >> 1), title->GetRect()->m_top);

		CoopVersusSettings* settings = CoopVersus_GetSettings();
		settings->m_grandPrix = p_mode == 1;
		if (settings->m_playerCount < 2 || settings->m_playerCount > 4) {
			settings->m_playerCount = 2;
		}
		if (settings->m_lapCount < 1 || settings->m_lapCount > 5) {
			settings->m_lapCount = 3;
		}
		m_playersCarousel.SetSelection(settings->m_playerCount - 2);
		m_lapsCarousel.SetSelection(settings->m_lapCount - 1);
		// Bottom row, right of MAIN MENU.
		LayoutSmallSelector(&m_playersSelector, &m_playersCarousel, 312, 420);
		LayoutSmallSelector(&m_lapsSelector, &m_lapsCarousel, 522, 420);
	}
	else {
		// English: the third mode is TIME TRIAL.
		const undefined2* language =
			m_menuTextStrings->GetStringBuffer(static_cast<LegoU16>(0x9d + m_context->m_saveSystem.GetLanguageIndex()));
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
			SetAscii(&s_title, s_titleBuffer, "TIME TRIAL");
			m_modeLabels[2].SetString(&s_title, 0);
		}
	}

	m_modeCarousel.RemoveAllItems();
	for (LegoS32 i = 0; i < modeCount; i++) {
		m_modeCarousel.AddItem(&m_modeLabels[i]);
	}
	m_modeCarousel.SetSelection(p_mode);
	m_modeCurrent = p_mode;
	// Clicks need this (hovering an arrow does not select the picker). Keyboard left/right still
	// reach the race pickers first: they were created earlier.
	m_modeSelector.SetAcceptUnfocusedInput(1);
	m_modeSelector.Enable(5);

	// An arrow either side of the title; the title itself names the mode (the picker's own
	// carousel only holds the selection and is not shown).
	m_modeCarousel.ClearFlags(2);
	const Rect* title = p_title->GetRect();
	const LegoS32 arrowSize = 40;
	const LegoS32 gap = 16;
	LegoS32 titleHeight = title->m_bottom - title->m_top;
	LegoS32 left = title->m_left - gap - arrowSize;
	LegoS32 width = (title->m_right + gap + arrowSize) - left;
	Rect rect;
	rect.m_left = left;
	rect.m_right = left + width;
	rect.m_top = title->m_top + (titleHeight >> 1) - (arrowSize >> 1);
	rect.m_bottom = rect.m_top + arrowSize;
	m_modeSelector.SetRect(&rect);
	Rect arrow;
	arrow.m_left = 0;
	arrow.m_top = 0;
	arrow.m_right = arrowSize;
	arrow.m_bottom = arrowSize;
	m_modeSelector.GetPrevButton().SetRect(&arrow);
	arrow.m_left = width - arrowSize;
	arrow.m_right = width;
	m_modeSelector.GetNextButton().SetRect(&arrow);
}

LegoBool32 SingleRaceSelectBase::HandleModeChanged(MenuWidget* p_source)
{
	if (p_source == &m_playersSelector || p_source == &m_lapsSelector) {
		CoopVersusSettings* settings = CoopVersus_GetSettings();
		settings->m_playerCount = m_playersCarousel.GetSelectedIndex() + 2;
		settings->m_lapCount = static_cast<LegoU8>(m_lapsCarousel.GetSelectedIndex() + 1);
		m_context->m_saveSystem.GetGameState().SetLapCount(settings->m_lapCount);
		m_context->m_context->m_lapCount = settings->m_lapCount;
		return TRUE;
	}
	if (p_source != &m_modeSelector) {
		return FALSE;
	}
	static const LegoU16 c_menus[3] = {5, 6, 0x1d};
	static const LegoU16 c_versusMenus[2] = {6, 5};
	LegoU32 index = m_modeCarousel.GetSelectedIndex();
	LegoU32 count = m_modeVersus ? 2 : 3;
	if (index < count && static_cast<LegoS32>(index) != m_modeCurrent) {
		LegoU16 menu = m_modeVersus ? c_versusMenus[index] : c_menus[index];
		if (m_modeVersus) {
			CoopVersus_GetSettings()->m_grandPrix = menu == 5;
		}
		m_modeSwitchMenuId = menu;
		m_nextMenuId = menu;
		// The picker's own (hidden) carousel never finishes animating, so do not wait on it.
		m_clickedWidget = NULL;
		m_navPending = TRUE;
	}
	return TRUE;
}

LegoBool32 SingleRaceSelectBase::NavigateModeSwitch()
{
	if (!m_modeSwitchMenuId) {
		return FALSE;
	}
	LegoU16 menu = m_modeSwitchMenuId;
	m_modeSwitchMenuId = 0;
	m_context->m_menuStack.Pop();
	m_context->m_menuStack.Push(menu);
	return TRUE;
}
