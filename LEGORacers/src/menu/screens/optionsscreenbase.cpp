#include "menu/screens/optionsscreenbase.h"

#include "app/coopcontrols.h"
#include "audio/musicinstance.h"
#include "audio/soundmanager.h"
#include "input/inputmanager.h"
#include "menu/menudialog.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"

DECOMP_SIZE_ASSERT(OptionsScreenBase, 0x51ac)

// FUNCTION: LEGORACERS 0x004829a0
OptionsScreenBase::OptionsScreenBase()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x00482be0
OptionsScreenBase::~OptionsScreenBase()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x00482de0
void OptionsScreenBase::Reset()
{
	m_page = FALSE;
	m_nextPage = FALSE;
	m_unk0x36c = 0;
	m_gameState = NULL;
	MenuGameScreen::Reset();
}

// FUNCTION: LEGORACERS 0x00482e00
void OptionsScreenBase::CreateHeaderWidgets()
{
	CreateImage(&m_photoImage, 0x49, 0x49);
	CreateTextLabel(&m_infoLabel, 0x3a, 0x3a, 0x10);
	m_infoLabel.WrapText(20);
}

// FUNCTION: LEGORACERS 0x00482e40
void OptionsScreenBase::CreateMainButtons()
{
	CreateTextButton(&m_gameOptionsButton, 0xed, 0x42, 0x11);
	CreateVideoWidgets();
	CreateTextButton(&m_audioButton, 0xef, 0x42, 0x13);
	CreateTextButton(&m_controls1Button, 0xf0, 0x42, 0x17);
	CreateTextButton(&m_controls2Button, 0xf1, 0x42, 0x18);
	CreateTextButton(&m_languageButton, 0xf2, 0x42, 0x9c);
	CreateTextButton(&m_creditsButton, 0x27, 0x42, 0x58);
	CreateTextButton(&m_backButton, 0x3f, 0x43, 2);
}

// FUNCTION: LEGORACERS 0x00482ef0
void OptionsScreenBase::CreateGameOptionWidgets()
{
	CreateCarousel(&m_racerCountCarousel, 0x3d, 0x3b);
	CreateCarousel(&m_lapCountCarousel, 0x3d, 0x3b);
	CreateSelector(&m_racerCountSelector, &m_racerCountCarousel, 0xf3, 0x4c);
	CreateSelector(&m_lapCountSelector, &m_lapCountCarousel, 0xf4, 0x4c);
	CreateTextLabel(&m_gameOptionLabels[0], 0x100, 0x37, 0x59);
	CreateTextLabel(&m_gameOptionLabels[1], 0x101, 0x37, 0x5a);

	LegoS32 i;
	for (i = 0; i < 6; i++) {
		CreateTextLabel(&m_racerCountLabels[i], 0x96, 0x37, static_cast<undefined2>(i + 0x60));
		m_racerCountCarousel.AddItem(&m_racerCountLabels[i]);
	}

	m_racerCountCarousel.SetSelection(m_gameState->GetRacerCount());
	OnWidgetValueChanged(&m_racerCountSelector);

	for (i = 0; i < 5; i++) {
		CreateTextLabel(&m_lapCountLabels[i], 0x96, 0x37, static_cast<undefined2>(i + 0x61));
		m_lapCountCarousel.AddItem(&m_lapCountLabels[i]);
	}

	m_lapCountCarousel.SetSelection(m_gameState->GetLapCount() - 1);
	OnWidgetValueChanged(&m_lapCountSelector);
	HideGameOptionWidgets();
}

// FUNCTION: LEGORACERS 0x00483030
void OptionsScreenBase::CreateAudioOptionWidgets()
{
	CreateOptionsRow(&m_musicVolumeSlider, 0xf9, 0xec);
	CreateOptionsRow(&m_soundVolumeSlider, 0xfa, 0xec);
	CreateCarousel(&m_stereoCarousel, 0xfb, 0x3b);
	CreateSelector(&m_stereoSelector, &m_stereoCarousel, 0xfc, 0x4c);

	LegoFloat musicVolume = m_gameState->GetMusicVolume() * 0.003921569f;
	m_musicVolumeSlider.SetValue(static_cast<LegoS32>(musicVolume * 20.0f + 0.5f));
	LegoFloat soundVolume = m_gameState->GetSoundVolume() * 0.003921569f;
	m_soundVolumeSlider.SetValue(static_cast<LegoS32>(soundVolume * 20.0f + 0.5f));
	OnWidgetValueChanged(&m_musicVolumeSlider);
	OnWidgetValueChanged(&m_soundVolumeSlider);

	for (LegoS32 i = 0; i < 3; i++) {
		CreateTextLabel(&m_audioLabels[i], static_cast<undefined2>(i + 0x104), 0x37, static_cast<undefined2>(i + 0x5d));
	}

	CreateTextLabel(&m_stereoLabel, 0x107, 0x37, 0xa7);
	CreateTextLabel(&m_monoLabel, 0x107, 0x37, 0xa8);
	m_stereoCarousel.AddItem(&m_stereoLabel);
	m_stereoCarousel.AddItem(&m_monoLabel);
	OnWidgetValueChanged(&m_stereoSelector);
	HideAudioOptionWidgets();
}

// FUNCTION: LEGORACERS 0x004831a0
void OptionsScreenBase::CreateWidgets()
{
	// [library:config] LegoRacers4K: one page instead of Game/Video/Audio sub-pages and the
	// Pick Language screen. Versus laps moved to the Versus setup; the display driver is auto-picked.
	CreateHeaderWidgets();
	CreateSinglePageWidgets();
	ShowMainButtons();
}

// [library:config] LegoRacers4K: lays every option out on the main options page. Widgets are
// created in navigation order (top to bottom), then moved into rows.
// [library:config] LegoRacers4K: move a volume bar, shrink its minifig-head buttons to
// 70% (the track keeps its height), widen the bar a little, then lay it out again so
// the thumb and the mouse-drag bounds (the track's screen rect) follow.
static void ScaleButton(MenuButton* p_button, LegoS32 p_height)
{
	Rect rect = *p_button->GetRect();
	LegoS32 width = ((rect.m_right - rect.m_left) * 7) / 10;
	rect.m_right = rect.m_left + width;
	rect.m_bottom = rect.m_top + p_height;
	p_button->SetRect(&rect);
}

static void PlaceVolumeBar(OptionsRowBase* p_row, LegoS32 p_top)
{
	Rect rect = *p_row->GetRect();
	LegoS32 width = rect.m_right - rect.m_left;
	LegoS32 height = rect.m_bottom - rect.m_top;
	LegoS32 newHeight = (height * 7) / 10;
	rect.m_left = 8;
	rect.m_right = 20 + width + 6;
	rect.m_top = p_top + ((height - newHeight) >> 1);
	rect.m_bottom = rect.m_top + newHeight;
	p_row->SetRect(&rect);
	ScaleButton(&p_row->GetPrevButton(), newHeight);
	ScaleButton(&p_row->GetNextButton(), newHeight);
	p_row->LayoutButtons();
	p_row->LayoutTrack();
	p_row->LayoutThumb();
}

// [library:config] LegoRacers4K: "MUSIC VOLUME" / "SOUND VOLUME" on one line: the
// localized title with line breaks turned into spaces and a leading "ADJUST" dropped.
static void SetOneLineVolumeTitle(
	MenuTextLabel* p_label,
	undefined2* p_source,
	GolString* p_string,
	undefined2* p_buffer
)
{
	if (!p_source) {
		return;
	}

	static const char c_adjust[] = "ADJUST";
	LegoS32 start = 0;
	LegoS32 i;
	for (i = 0; c_adjust[i]; i++) {
		LegoU16 c = p_source[i];
		if (c >= 'a' && c <= 'z') {
			c -= 'a' - 'A';
		}
		if (c != static_cast<LegoU8>(c_adjust[i])) {
			break;
		}
	}
	if (!c_adjust[i] && (p_source[i] == ' ' || p_source[i] == '\n' || p_source[i] == '\r')) {
		start = i + 1;
	}

	LegoS32 length = 0;
	for (i = start; p_source[i] && length < 63; i++) {
		LegoU16 c = p_source[i];
		if (c == '\r') {
			continue;
		}
		if (c == '\n') {
			c = ' ';
		}
		if (c == ' ' && (length == 0 || p_buffer[length - 1] == ' ')) {
			continue;
		}
		p_buffer[length++] = c;
	}
	while (length > 0 && p_buffer[length - 1] == ' ') {
		length--;
	}
	p_buffer[length] = 0;
	p_string->CopyFromBufSelection(p_buffer, static_cast<undefined2>(length + 1));
	p_label->SetString(p_string, 0);
}

static LegoBool32 g_focusLanguageAfterReload = FALSE;

static LegoBool32 g_languageReloadPending = FALSE;

static void MoveWidget(MenuWidget* p_widget, LegoS32 p_left, LegoS32 p_top)
{
	Rect rect = *p_widget->GetRect();
	rect.m_right += p_left - rect.m_left;
	rect.m_bottom += p_top - rect.m_top;
	rect.m_left = p_left;
	rect.m_top = p_top;
	p_widget->SetRect(&rect);
}

void OptionsScreenBase::CreateSinglePageWidgets()
{
	LegoS32 i;
	const LegoS32 c_rowOpponents = 74;
	const LegoS32 c_rowMusic = 138;
	const LegoS32 c_rowSound = 204;
	const LegoS32 c_rowLanguage = 276;

	CreateCarousel(&m_racerCountCarousel, 0x3d, 0x3b);
	CreateSelector(&m_racerCountSelector, &m_racerCountCarousel, 0xf3, 0x4c);
	CreateTextLabel(&m_gameOptionLabels[0], 0x100, 0x37, 0x59);
	for (i = 0; i < 6; i++) {
		CreateTextLabel(&m_racerCountLabels[i], 0x96, 0x37, static_cast<undefined2>(i + 0x60));
		m_racerCountCarousel.AddItem(&m_racerCountLabels[i]);
	}
	m_racerCountCarousel.SetSelection(m_gameState->GetRacerCount());
	OnWidgetValueChanged(&m_racerCountSelector);
	MoveWidget(&m_racerCountSelector, 72, c_rowOpponents);
	MoveWidget(&m_gameOptionLabels[0], 330, c_rowOpponents - 2);

	CreateOptionsRow(&m_musicVolumeSlider, 0xf9, 0xec);
	CreateOptionsRow(&m_soundVolumeSlider, 0xfa, 0xec);
	// Place the bars before laying them out again, so the thumb and the
	// mouse-drag bounds (the track's screen rect) follow the new position.
	PlaceVolumeBar(&m_musicVolumeSlider, c_rowMusic);
	PlaceVolumeBar(&m_soundVolumeSlider, c_rowSound);
	LegoFloat musicVolume = m_gameState->GetMusicVolume() * 0.003921569f;
	m_musicVolumeSlider.SetValue(static_cast<LegoS32>(musicVolume * 20.0f + 0.5f));
	LegoFloat soundVolume = m_gameState->GetSoundVolume() * 0.003921569f;
	m_soundVolumeSlider.SetValue(static_cast<LegoS32>(soundVolume * 20.0f + 0.5f));
	OnWidgetValueChanged(&m_musicVolumeSlider);
	OnWidgetValueChanged(&m_soundVolumeSlider);
	for (i = 0; i < 2; i++) {
		CreateTextLabel(&m_audioLabels[i], static_cast<undefined2>(i + 0x104), 0x37, static_cast<undefined2>(i + 0x5d));
		SetOneLineVolumeTitle(
			&m_audioLabels[i],
			m_menuTextStrings->GetStringBuffer(static_cast<LegoU16>(i + 0x5d)),
			&m_audioTitleStrings[i],
			m_audioTitleBuffers[i]
		);
	}
	MoveWidget(&m_audioLabels[0], 330, c_rowMusic + 4);
	MoveWidget(&m_audioLabels[1], 330, c_rowSound + 4);

	CreateCarousel(&m_languageCarousel, 0x3d, 0x3b);
	CreateSelector(&m_languageSelector, &m_languageCarousel, 0xf3, 0x4c);
	for (i = 0; i < 9; i++) {
		CreateTextLabel(&m_languageLabels[i], 0x96, 0x37, static_cast<undefined2>(i + 0x9d));
		m_languageCarousel.AddItem(&m_languageLabels[i]);
	}
	m_languageCarousel.SetSelection(m_context->m_saveSystem.GetLanguageIndex());
	CreateTextLabel(&m_languageTitle, 0x100, 0x37, 0x9c);
	MoveWidget(&m_languageSelector, 72, c_rowLanguage);
	MoveWidget(&m_languageTitle, 330, c_rowLanguage - 2);

	CreateTextButton(&m_controls1Button, 0xf0, 0x42, 0x17);
	CreateTextButton(&m_controls2Button, 0xf1, 0x42, 0x18);
	CreateTextButton(&m_creditsButton, 0x27, 0x42, 0x58);
	CreateTextButton(&m_backButton, 0x3f, 0x43, 2);
	MoveWidget(&m_controls1Button, 3, 340);
	MoveWidget(&m_controls2Button, 3, 368);
	MoveWidget(&m_creditsButton, 3, 396);
	MoveWidget(&m_backButton, 16, 436);
}

// FUNCTION: LEGORACERS 0x004831d0
LegoBool32 OptionsScreenBase::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	m_gameState = &p_context->m_saveSystem.GetGameState();

	if (!MenuGameScreen::Initialize(p_context, p_createParams)) {
		return FALSE;
	}

	if (!IsMenuMusicPlaying(p_context)) {
		StartMenuMusic(p_context, 0, TRUE);
	}

	return TRUE;
}

// FUNCTION: LEGORACERS 0x00483220
void OptionsScreenBase::Navigate()
{
	switch (m_nextMenuId) {
	case 2:
		m_context->m_menuStack.Pop();
		m_context->m_menuStack.Push(0x30);
		break;
	case 0x0a:
	case 0x0b:
	case 0x27:
	case 0x2c:
		m_context->m_menuStack.Push(m_nextMenuId);
		// intentional fallthrough
	default:
		m_context->m_menuStack.Push(0x30);
		break;
	}
}

// FUNCTION: LEGORACERS 0x004832d0
void OptionsScreenBase::OnIconUnfocused(MenuWidget* p_widget)
{
	if (p_widget == &m_gameOptionsButton) {
		m_nextPage = 1;
	}
	else if (p_widget == &m_audioButton) {
		CreateTextButton(&m_audioNoticePopup, 0x99, 0x46, 0x72);
		ShowPopupDialog(&m_audioNoticePopup, 0xbd);
	}
	else if (p_widget == &m_controls1Button) {
		m_nextMenuId = 0x0a;
	}
	else if (p_widget == &m_controls2Button) {
		m_nextMenuId = 0x0b;
	}
	else if (p_widget == &m_languageButton) {
		m_nextMenuId = 0x2c;
	}
	else if (p_widget == &m_creditsButton) {
		m_nextMenuId = 0x27;
	}
	else if (p_widget == &m_backButton) {
		if (m_page == 0) {
			m_nextMenuId = 2;
		}
		else {
			m_nextPage = 0;
		}
	}
	else if (p_widget == &m_audioNoticePopup) {
		m_nextPage = 2;
		m_dialog->DismissTop();
	}

	if (m_nextMenuId != 0xffff) {
		m_navPending = TRUE;
	}

	m_clickedWidget = p_widget;
}

// FUNCTION: LEGORACERS 0x004833e0
void OptionsScreenBase::OnWidgetValueChanged(MenuWidget* p_widget)
{
	if (p_widget == &m_musicVolumeSlider) {
		LegoFloat volume = m_musicVolumeSlider.GetValue() * 0.05f;
		m_gameState->SetMusicVolume(static_cast<LegoU8>(volume * 255.0f));

		if (m_context->m_context->m_soundManager) {
			m_context->m_context->m_soundManager->SetMusicVolumeScale(1.0f);

			MusicInstance* musicInstance = m_context->m_modelBuilder.GetMusicInstance();
			if (musicInstance) {
				musicInstance->SetVolume(volume);
			}

			m_context->m_context->m_soundManager->SetMusicVolumeScale(volume);
		}
	}
	else if (p_widget == &m_soundVolumeSlider) {
		LegoFloat volume = m_soundVolumeSlider.GetValue() * 0.05f;
		m_gameState->SetSoundVolume(static_cast<LegoU8>(volume * 255.0f));

		SoundManager* soundManager = m_context->m_context->m_soundManager;
		if (soundManager) {
			soundManager->SetVolumeScale(volume);
		}
	}
	else if (p_widget == &m_racerCountSelector) {
		m_context->m_context->m_racerCount = m_racerCountCarousel.GetSelectedIndex();
		GameState* gameState = m_gameState;
		gameState->SetRacerCount(m_context->m_context->m_racerCount);
	}
	else if (p_widget == &m_lapCountSelector) {
		m_context->m_context->m_lapCount = static_cast<LegoU8>(m_lapCountCarousel.GetSelectedIndex() + 1);
		GameState* gameState = m_gameState;
		gameState->SetLapCount(m_context->m_context->m_lapCount);
	}
	else if (p_widget == &m_languageSelector) {
		// [library:config] LegoRacers4K: inline language picker (saved when leaving options).
		m_gameState->SetLanguageIndex(static_cast<LegoU8>(m_languageCarousel.GetSelectedIndex()));
		m_gameState->SetDirty(TRUE);
		ReloadForLanguage();
	}
	else if (p_widget == &m_stereoSelector) {
		m_gameState->SetStereo(m_stereoCarousel.GetSelectedIndex() == 0);

		SoundManager* soundManager = m_context->m_context->m_soundManager;
		if (soundManager) {
			if (m_gameState->GetStereo()) {
				soundManager->ClearMonoFlag();
			}
			else {
				soundManager->SetMonoFlag();
			}
		}
	}
}

// FUNCTION: LEGORACERS 0x004164c0 FOLDED
void OptionsScreenBase::CreateVideoWidgets()
{
}

// [library:config] LegoRacers4K: changing the language reopens the options screen,
// which reloads the localized strings and fonts (MenuManager::OpenScreen).
void OptionsScreenBase::ReloadForLanguage()
{
	if (m_languageCarousel.GetSelectedIndex() == static_cast<LegoS32>(m_context->m_context->m_languageIndex)) {
		return;
	}

	g_focusLanguageAfterReload = TRUE;
	g_languageReloadPending = TRUE;
}

// FUNCTION: LEGORACERS 0x004835a0
void OptionsScreenBase::ShowMainButtons()
{
	// [library:config] LegoRacers4K: single options page.
	m_controls1Button.SetFlags(2);
	m_controls2Button.SetFlags(2);
	m_creditsButton.SetFlags(2);
	m_racerCountSelector.SetFlags(2);
	m_gameOptionLabels[0].SetFlags(2);
	m_musicVolumeSlider.SetFlags(2);
	m_soundVolumeSlider.SetFlags(2);
	m_audioLabels[0].SetFlags(2);
	m_audioLabels[1].SetFlags(2);
	m_languageSelector.SetFlags(2);
	m_languageTitle.SetFlags(2);

	if (g_focusLanguageAfterReload) {
		// Reopened for a live language change: keep the language row focused.
		g_focusLanguageAfterReload = FALSE;
		m_languageSelector.Select(4);
	}
	else {
		m_racerCountSelector.Select(4);
	}
	m_infoLabel.SetStringByIndex(0x10, TRUE);
	m_infoLabel.WrapText(20);
	m_backButton.SetTextByIndex(2);
}

// FUNCTION: LEGORACERS 0x00483640
void OptionsScreenBase::HideMainButtons()
{
	m_gameOptionsButton.ClearFlags(2);
	m_audioButton.ClearFlags(2);
	m_controls1Button.ClearFlags(2);
	m_controls2Button.ClearFlags(2);
	m_languageButton.ClearFlags(2);
	m_creditsButton.ClearFlags(2);
}

// FUNCTION: LEGORACERS 0x004836a0
void OptionsScreenBase::ShowGameOptionWidgets()
{
	m_infoLabel.SetStringByIndex(0x11, TRUE);
	m_infoLabel.WrapText(20);
	m_backButton.SetTextByIndex(0x10);

	m_racerCountSelector.SetFlags(2);
	m_lapCountSelector.SetFlags(2);
	m_gameOptionLabels[0].SetFlags(2);
	m_gameOptionLabels[1].SetFlags(2);

	OnWidgetValueChanged(&m_racerCountSelector);
	OnWidgetValueChanged(&m_lapCountSelector);
	m_racerCountSelector.Select(4);
}

// FUNCTION: LEGORACERS 0x00483730
void OptionsScreenBase::HideGameOptionWidgets()
{
	m_racerCountSelector.ClearFlags(2);
	m_lapCountSelector.ClearFlags(2);
	m_gameOptionLabels[0].ClearFlags(2);
	m_gameOptionLabels[1].ClearFlags(2);
}

// FUNCTION: LEGORACERS 0x00483770
void OptionsScreenBase::ShowAudioOptionWidgets()
{
	m_musicVolumeSlider.SetFlags(2);
	m_soundVolumeSlider.SetFlags(2);

	for (LegoS32 i = 0; i < 3; i++) {
		m_audioLabels[i].SetFlags(2);
	}

	OnWidgetValueChanged(&m_stereoSelector);
	m_musicVolumeSlider.Select(4);
	m_infoLabel.SetStringByIndex(0x13, TRUE);
	m_infoLabel.WrapText(20);
	m_backButton.SetTextByIndex(0x10);
}

// FUNCTION: LEGORACERS 0x00483800
void OptionsScreenBase::HideAudioOptionWidgets()
{
	m_musicVolumeSlider.ClearFlags(2);
	m_soundVolumeSlider.ClearFlags(2);
	m_stereoSelector.ClearFlags(2);

	for (LegoS32 i = 0; i < 3; i++) {
		m_audioLabels[i].ClearFlags(2);
	}
}

// FUNCTION: LEGORACERS 0x00483850
void OptionsScreenBase::ApplyPageChange()
{
	switch (m_page) {
	case 0:
		HideMainButtons();
		break;
	case 1:
		HideGameOptionWidgets();
		break;
	case 2:
		HideAudioOptionWidgets();
		break;
	}

	switch (m_nextPage) {
	case 0:
		ShowMainButtons();
		break;
	case 1:
		ShowGameOptionWidgets();
		break;
	case 2:
		ShowAudioOptionWidgets();
		break;
	}

	m_page = m_nextPage;
}

// FUNCTION: LEGORACERS 0x004838e0
LegoBool32 OptionsScreenBase::Update(undefined4 p_elapsed)
{
	if (m_nextPage != m_page && !(m_clickedWidget->GetAnimFlags() & 1)) {
		ApplyPageChange();
	}

	if (m_page == 0) {
		if (m_inputManager->GetJoystickCount() <= 1 && !m_inputManager->IsKeyboardAvailable()) {
			m_controls2Button.Disable(5);
		}
		else {
			m_controls2Button.Enable(5);
		}
	}

	if (g_languageReloadPending) {
		// [library:config] LegoRacers4K: live language change (see ReloadForLanguage).
		g_languageReloadPending = FALSE;
		m_nextMenuId = 0xffff;
		Navigate();
		return FALSE;
	}

	return MenuGameScreen::Update(p_elapsed);
}
