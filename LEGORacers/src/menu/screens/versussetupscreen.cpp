// [library:config] LegoRacers4K: Versus race setup; see versussetupscreen.h.

#include "menu/screens/versussetupscreen.h"

#include "app/coopcontrols.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"
#include "menu/menuscreenid.h"

namespace
{
enum {
	c_rowMode = 0,
	c_rowPlayers,
	c_rowLaps,
};

const char* const c_rowTitles[VersusSetupScreen::c_rowCount] = {"RACE TYPE", "NUMBER OF PLAYERS", "NUMBER OF LAPS"};

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

VersusSetupScreen::VersusSetupScreen()
{
	Reset();
}

VersusSetupScreen::~VersusSetupScreen()
{
	Destroy();
}

void VersusSetupScreen::CreateWidgets()
{
	CreateImage(&m_photoImage, 0x49, 0x49);
	CreateTextLabel(&m_infoLabel, 0x3a, 0x3a, 0x19);
	m_infoLabel.WrapText(20);
	{
		const Rect* titleRect = m_infoLabel.GetRect();
		MoveTo(&m_infoLabel, 320 - ((titleRect->m_right - titleRect->m_left) >> 1), titleRect->m_top);
	}

	static const LegoS32 c_itemCounts[c_rowCount] = {2, 3, 5};
	static const undefined2 c_firstItem[c_rowCount] = {0, 0x62, 0x61};
	static const undefined2 c_modeItems[2] = {0x22, 0x21};

	for (LegoS32 row = 0; row < c_rowCount; row++) {
		LegoS32 top = 98 + row * 80;
		CreateCarousel(&m_carousels[row], 0x3d, 0x3b);
		CreateSelector(&m_selectors[row], &m_carousels[row], 0xf3, 0x4c);
		for (LegoS32 i = 0; i < c_itemCounts[row]; i++) {
			undefined2 textId = row == c_rowMode ? c_modeItems[i] : static_cast<undefined2>(c_firstItem[row] + i);
			CreateTextLabel(&m_items[row][i], 0x96, 0x37, textId);
			m_carousels[row].AddItem(&m_items[row][i]);
		}
		MoveTo(&m_selectors[row], 72, top);

		CreateTextLabel(&m_titles[row], 0x100, 0x37, 0x59);
		const char* text = c_rowTitles[row];
		LegoU32 length = 0;
		for (; text[length]; length++) {
			m_titleBuffers[row][length] = static_cast<LegoU8>(text[length]);
		}
		m_titleBuffers[row][length] = 0;
		m_titleStrings[row].CopyFromBufSelection(m_titleBuffers[row], static_cast<undefined2>(length + 1));
		m_titles[row].SetString(&m_titleStrings[row], 0);
		MoveTo(&m_titles[row], 330, top + 10);
	}

	CreateTextButton(&m_okButton, 0xed, 0x46, 0x72);
	MoveTo(&m_okButton, 16, 378);
	CreateTextButton(&m_backButton, 0x3f, 0x43, 2);
}

LegoBool32 VersusSetupScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	CoopVersusSettings* settings = CoopVersus_GetSettings();
	GameState& gameState = p_context->m_saveSystem.GetGameState();
	settings->m_lapCount = static_cast<LegoU8>(gameState.GetLapCount());
	if (settings->m_lapCount < 1 || settings->m_lapCount > 5) {
		settings->m_lapCount = 3;
	}

	// Widgets come from the options screen's layout (options.mib).
	p_createParams->m_menuId = c_menuOptions;
	if (!MenuGameScreen::Initialize(p_context, p_createParams)) {
		return FALSE;
	}

	if (!IsMenuMusicPlaying(p_context)) {
		StartMenuMusic(p_context, 0, TRUE);
	}

	m_carousels[c_rowMode].SetSelection(settings->m_grandPrix ? 1 : 0);
	m_carousels[c_rowPlayers].SetSelection(settings->m_playerCount - 2);
	m_carousels[c_rowLaps].SetSelection(settings->m_lapCount - 1);
	m_selectors[c_rowMode].Select(4);
	return TRUE;
}

void VersusSetupScreen::OnWidgetValueChanged(MenuWidget* p_widget)
{
	CoopVersusSettings* settings = CoopVersus_GetSettings();

	if (p_widget == &m_selectors[c_rowMode]) {
		settings->m_grandPrix = m_carousels[c_rowMode].GetSelectedIndex() == 1;
	}
	else if (p_widget == &m_selectors[c_rowPlayers]) {
		settings->m_playerCount = m_carousels[c_rowPlayers].GetSelectedIndex() + 2;
	}
	else if (p_widget == &m_selectors[c_rowLaps]) {
		settings->m_lapCount = static_cast<LegoU8>(m_carousels[c_rowLaps].GetSelectedIndex() + 1);
		m_context->m_saveSystem.GetGameState().SetLapCount(settings->m_lapCount);
		m_context->m_context->m_lapCount = settings->m_lapCount;
	}
}

void VersusSetupScreen::OnIconUnfocused(MenuWidget* p_widget)
{
	if (p_widget == &m_okButton) {
		m_context->m_modelBuilder.SetMenuFlowFlags(
			m_context->m_modelBuilder.GetMenuFlowFlags() | DriverModelBuilder::c_menuFlowVersus
		);
		m_nextMenuId = c_menuSingleRace;
	}
	else if (p_widget == &m_backButton) {
		m_nextMenuId = c_menuMainMenu;
	}

	if (m_nextMenuId != 0xffff) {
		m_navPending = TRUE;
	}

	m_clickedWidget = p_widget;
}

void VersusSetupScreen::Navigate()
{
	if (m_nextMenuId == c_menuMainMenu) {
		m_context->m_modelBuilder.SetMenuFlowFlags(
			m_context->m_modelBuilder.GetMenuFlowFlags() & ~DriverModelBuilder::c_menuFlowVersus
		);
		m_context->m_menuStack.Pop();
		return;
	}

	m_context->m_menuStack.Push(m_nextMenuId);
}
