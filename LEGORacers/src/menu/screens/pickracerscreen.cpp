#include "menu/screens/pickracerscreen.h"

#include "app/coopcontrols.h"
#include "golstringtable.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreenid.h"

#include <SDL3/SDL.h>

DECOMP_SIZE_ASSERT(PickRacerScreen, 0x4c8c)

// FUNCTION: LEGORACERS 0x00484c10
PickRacerScreen::PickRacerScreen()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x00484ce0
PickRacerScreen::~PickRacerScreen()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x00484d90
void PickRacerScreen::CreateWidgets()
{
	undefined2 textId = 0x14;

	CreateImage(&m_photoImage, 0x49, 0x49);
	RacerPickScreenBase::CreateWidgets();

	if (m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) {
		textId = m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCountAsU16();
		// [library:input] Players 3-4 reuse the player 2 title (digit swapped in Initialize).
		if (textId > 1) {
			textId = 1;
		}
		textId += 0x15;
	}

	CreateTextLabel(&m_infoLabel, 0x3a, 0x3a, textId);
	m_infoLabel.WrapText(0x14);
	CreateTextButton(&m_controlsButton, 9, 0x42, 0xb9);
	CreateTextButton(&m_startButton, 0x41, 0x46, 0x72);
	CreateTextButton(&m_nextPlayerButton, 0x40, 0x46, 0x72);
	CreateTextButton(&m_backButton, 0x3f, 0x45, 0x1f);
}

// FUNCTION: LEGORACERS 0x00484e40
LegoBool32 PickRacerScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	undefined4 params[3];
	params[0] = 1;
	params[1] = 1;
	params[2] = 0xffff3;

	if (!p_context->m_modelBuilder.HasMenuResources()) {
		LoadPartResources(p_context, FALSE);
	}

	if (!RacerPickScreenBase::Initialize(p_context, p_createParams, params)) {
		return FALSE;
	}

	SetupLighting();

	if (p_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) {
		m_nameSelectors[0].SetAcceptUnfocusedInput(TRUE);
		// [library:input] 4-player couch co-op flow (players 1-4, "add player" button).
		SetupCoopVersusButtons();
	}
	else {
		m_nextPlayerButton.ClearFlags(2);
		m_startButton.Select(0);
	}

	return TRUE;
}

// [library:input] Points p_string at p_buffer holding p_text.
void PickRacerScreen::SetCoopText(GolString* p_string, undefined2* p_buffer, LegoU32 p_size, const LegoChar* p_text)
{
	LegoU32 i = 0;
	for (; p_text[i] && i < p_size - 1; i++) {
		p_buffer[i] = static_cast<LegoU8>(p_text[i]);
	}
	p_buffer[i] = 0;
	p_string->CopyFromBufSelection(p_buffer, static_cast<undefined2>(i + 1));
}

// [library:input] Versus with up to 4 players. Each player picks in turn; from player 2
// on, "OK" starts the race and "ADD PLAYER n" (in place of CONFIG, since Versus controls
// come from controls.ini) brings in the next player.
void PickRacerScreen::SetupCoopVersusButtons()
{
	LegoU32 count = m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount();

	if (count >= 2) {
		GolString title;
		LegoChar text[128];
		m_menuTextStrings->CopyStringByIndex(&title, 0x16);
		title.CopyToString(text);
		for (LegoU32 i = 0; text[i]; i++) {
			if (text[i] == '2') {
				text[i] = static_cast<LegoChar>('1' + count);
				break;
			}
		}
		SetCoopText(&m_coopTitle, m_coopTitleBuffer, sizeOfArray(m_coopTitleBuffer), text);
		m_infoLabel.SetString(&m_coopTitle, TRUE);
	}

	if (count == 0) {
		m_controlsButton.ClearFlags(2);
		m_startButton.ClearFlags(2);
		m_nextPlayerButton.Select(0);
	}
	else if (count + 1 < c_coopMaxPlayers) {
		// The CONFIG button becomes "ADD PLAYER n" (see OnIconUnfocused).
		LegoChar text[32];
		SDL_snprintf(text, sizeof(text), "ADD PLAYER %u", count + 2);
		SetCoopText(&m_coopAddText, m_coopAddBuffer, sizeOfArray(m_coopAddBuffer), text);
		m_controlsButton.SetText(&m_coopAddText);
		m_nextPlayerButton.ClearFlags(2);
		m_startButton.Select(0);
	}
	else {
		m_controlsButton.ClearFlags(2);
		m_nextPlayerButton.ClearFlags(2);
		m_startButton.Select(0);
	}
}

// FUNCTION: LEGORACERS 0x00484f40
void PickRacerScreen::OnIconUnfocused(MenuWidget* p_source)
{
	// [library:input] Versus players 2-3: the CONFIG button is "ADD PLAYER n".
	if (p_source == &m_controlsButton &&
		(m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) &&
		m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount() > 0) {
		p_source = &m_nextPlayerButton;
	}

	if (p_source == &m_controlsButton) {
		m_nextMenuId = static_cast<LegoU16>(
			m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCountAsU16() + c_menuControl1
		);
		m_driverEntities[0].SetFlags(m_driverEntities[0].GetFlags() & ~0x10000);
	}
	else if (p_source == &m_startButton) {
		if (m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) {
			m_context->m_context->m_racerCount = 0;
		}

		PlayRandomNamedAnimation(0);
		MenuGameContext* context = m_context;
		SaveRecordList::Record* record = m_recordCursors[0].GetSelectedRecord();
		context->m_saveSystem.GetActiveRecord().SetSelectedRecord(
			context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount(),
			record
		);
		m_context->m_saveSystem.GetActiveRecord().SetSelectedRecordCount(
			m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount() + 1
		);
		m_nextMenuId = 0x41;
	}
	else if (p_source == &m_nextPlayerButton) {
		PlayRandomNamedAnimation(0);
		MenuGameContext* context = m_context;
		SaveRecordList::Record* record = m_recordCursors[0].GetSelectedRecord();
		context->m_saveSystem.GetActiveRecord().SetSelectedRecord(
			context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount(),
			record
		);
		m_context->m_saveSystem.GetActiveRecord().SetSelectedRecordCount(
			m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount() + 1
		);
		m_nextMenuId = c_menuPickRacerP2;
	}
	else if (p_source == &m_backButton) {
		m_nextMenuId = 0x3f;

		if (m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount() != 0) {
			m_context->m_saveSystem.GetActiveRecord().SetSelectedRecordCount(
				m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount() - 1
			);
		}

		m_driverEntities[0].SetFlags(m_driverEntities[0].GetFlags() & ~0x10000);
	}

	if (m_nextMenuId != 0xffff) {
		m_navPending = TRUE;
	}

	m_clickedWidget = p_source;
}

// FUNCTION: LEGORACERS 0x00485090
void PickRacerScreen::OnWidgetValueChanged(MenuWidget* p_source)
{
	RacerPickScreenBase::OnWidgetValueChanged(p_source);
}

// FUNCTION: LEGORACERS 0x004850a0
void PickRacerScreen::Navigate()
{
	switch (m_nextMenuId) {
	case c_menuControl1:
	case c_menuControl2:
	case c_menuPickRacerP2:
		m_context->m_menuStack.Push(m_nextMenuId);
		CommitRecordSelections();
		return;
	case 0x3f:
		m_context->m_menuStack.Pop();
		CommitRecordSelections();
		return;
	case 0x41:
		if (m_context->m_context->m_raceMode == LegoRacers::Context::c_raceModeCircuit &&
			m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount() == 1) {
			CircuitDefinitionList::CircuitDefinition* circuitDefinition =
				m_context->m_circuitList.FindCircuit(m_context->m_context->m_circuitName);
			LegoU32 raceIndex = m_context->m_circuitList.GetEntryIndex(circuitDefinition);
			m_context->m_menuStack.Push(static_cast<LegoU16>(raceIndex + c_menuCircuit1));
			m_context->m_context->m_racerCount = 5;
		}
		else {
			m_context->m_menuStack.ResetSize();
		}

		if (m_context->m_modelBuilder.GetMenuFlowFlags() & DriverModelBuilder::c_menuFlowVersus) {
			m_context->m_context->m_racerCount = 0;
		}
		else {
			m_context->m_context->m_racerCount += m_context->m_saveSystem.GetActiveRecord().GetSelectedRecordCount();
		}

		m_navPending = TRUE;
		m_context->m_modelBuilder.SetMenuFlowFlags(
			m_context->m_modelBuilder.GetMenuFlowFlags() & ~DriverModelBuilder::c_menuFlowVersus
		);
		break;
	}

	CommitRecordSelections();
}
