#include "menu/screens/splashcinematicscreen.h"

#include "audio/musicgroup.h"
#include "audio/musicinstance.h"
#include "font/golfont.h"
#include "golfontbase.h"
#include "golhashtable.h"
#include "golstream.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"
#include "menu/menuscreenid.h"
#include "render/golrenderdevice.h"

#include <miniwin/miniwinapp.h>
#include <string.h>

DECOMP_SIZE_ASSERT(SplashCinematicScreen, 0x658)

// [library:config] LegoRacers4K: the credits are a smooth scrolling list (the original showed one
// page at a time, fading between them). Text and grouping come from the original credits.
namespace
{
enum CreditKind {
	c_head,
	c_sub,
	c_role,
	c_name,
	c_text,
	c_para
};

struct CreditLine {
	CreditKind m_kind;
	const char* m_text;
};

const CreditLine g_credits[] = {
	{c_head, "HIGH VOLTAGE SOFTWARE"},
	{c_role, "ORIGINAL CONCEPT"},
	{c_name, "KERRY J. GANOFSKY"},
	{c_role, "EXECUTIVE PRODUCER"},
	{c_name, "KEITH MORTON"},
	{c_role, "PROGRAMMING DIRECTOR"},
	{c_name, "SCOTT CORLEY"},
	{c_role, "LEAD PROGRAMMER"},
	{c_name, "DWIGHT LUETSCHER"},
	{c_role, "PROGRAMMING"},
	{c_name, "HAL BOUMA"},
	{c_name, "ANDREW FALTH"},
	{c_name, "JEFF MARSHALL"},
	{c_name, "ADISAK POCHANAYON"},
	{c_name, "DAVE SCHEELE"},
	{c_role, "LEAD ARTIST"},
	{c_name, "CARY PENCZEK"},
	{c_role, "ARTISTS"},
	{c_name, "GABE BOTT"},
	{c_name, "BILL ENG"},
	{c_name, "PETE MCLENNON"},
	{c_name, "GARY OLIVERIO"},
	{c_name, "BRIAN SCHULTZ"},
	{c_name, "KELLY SEIDER"},
	{c_name, "JOE STINCHCOMB"},
	{c_name, "BRIAN WESTERGAARD"},
	{c_role, "LEAD SOUND ENGINEER"},
	{c_name, "ERIC NOFSINGER"},
	{c_role, "SOUND ENGINEERS"},
	{c_name, "D.CHADD PORTWINE"},
	{c_name, "MICHAEL CAISLEY"},
	{c_role, "TESTING"},
	{c_name, "KEVIN SHELLER"},
	{c_name, "WILLIAM GOLZ"},
	{c_head, "LEGO MEDIA INTERNATIONAL"},
	{c_role, "WORLDWIDE MANAGING DIRECTOR"},
	{c_name, "MARK LIVINGSTONE"},
	{c_sub, "PRODUCT DEVELOPMENT"},
	{c_role, "HEAD OF CONSTRUCTIVE, GAMES AND GIRL'S SOFTWARE"},
	{c_name, "LAURENCE SCOTFORD"},
	{c_role, "SENIOR PRODUCER"},
	{c_name, "TOMAS GILLO"},
	{c_sub, "TESTING AND QA"},
	{c_role, "QA MANAGER"},
	{c_name, "TONY MILLER"},
	{c_role, "LEAD TESTER"},
	{c_name, "SCOTT MACKINTOSH"},
	{c_sub, "LOCALISATION"},
	{c_role, "LOCALISATION MANAGER"},
	{c_name, "CARA MCMULLAN"},
	{c_sub, "PRODUCTION"},
	{c_role, "LOGISTICS MANAGER"},
	{c_name, "NIC ASHFORD"},
	{c_sub, "INTERNATIONAL MARKETING"},
	{c_role, "MARKETING DIRECTOR- EUROPE AND ASIA"},
	{c_name, "PETRA BEDFORD"},
	{c_role, "MARKETING MANAGER FOR THE TITLE"},
	{c_name, "PHILIPPE OSSWALD"},
	{c_role, "MARKETING MANAGER"},
	{c_name, "ROB GIBSON"},
	{c_sub, "INTERNATIONAL SALES"},
	{c_role, "SALES DIRECTOR - EUROPE AND ASIA"},
	{c_name, "LEAH KALBOUSSI"},
	{c_role, "US SALES AND MARKETING DIRECTOR"},
	{c_name, "GREGG SAUTER"},
	{c_para, "SPECIAL THANKS TO ALL THE FAMILY MEMBERS"},
	{c_text, "OF THE DEVELOPMENT TEAM FOR THEIR"},
	{c_text, "PATIENCE AND SUPPORT."},
	{c_para, "SPECIAL THANKS TO ALL OF THE CHILDREN"},
	{c_text, "WHO GAVE THEIR VALUABLE INPUT"},
	{c_text, "THROUGHOUT THE DEVELOPMENT OF THIS TITLE."},
	{c_sub, "ADDITIONAL THANKS"},
	{c_text, "THANKS ALSO TO JAN BLAESILD, S0REN DYRH0J,"},
	{c_text, "MIKKEL FRISTOFT AND JAKOB NORDMAN"},
	{c_text, "FOR THEIR VALUABLE CONTRIBUTION."},
};

const LegoS32 c_creditCount = sizeof(g_credits) / sizeof(g_credits[0]);
const float c_creditSpeed = 44.0f; // pixels per second
float g_creditsTime = 0.0f;
float g_creditsHeight = 0.0f;

void CreditStyle(CreditKind p_kind, LegoU32* p_color, float* p_scale, float* p_before, float* p_step)
{
	switch (p_kind) {
	case c_head:
		*p_color = 0xff1616c0;
		*p_scale = 1.0f;
		*p_before = 70.0f;
		*p_step = 44.0f;
		break;
	case c_sub:
		*p_color = 0xffdf3d25;
		*p_scale = 1.0f;
		*p_before = 54.0f;
		*p_step = 42.0f;
		break;
	case c_role:
		*p_color = 0xff00dfff;
		*p_scale = 0.9f;
		*p_before = 34.0f;
		*p_step = 34.0f;
		break;
	case c_name:
		*p_color = 0xffc8c8c8;
		*p_scale = 0.8f;
		*p_before = 0.0f;
		*p_step = 27.0f;
		break;
	case c_para:
		*p_color = 0xffc8c8c8;
		*p_scale = 0.8f;
		*p_before = 50.0f;
		*p_step = 27.0f;
		break;
	default:
		*p_color = 0xffc8c8c8;
		*p_scale = 0.8f;
		*p_before = 0.0f;
		*p_step = 27.0f;
		break;
	}
}
} // namespace

// FUNCTION: LEGORACERS 0x00488380
SplashCinematicScreen::SplashCinematicScreen()
{
}

// FUNCTION: LEGORACERS 0x004883c0
SplashCinematicScreen::~SplashCinematicScreen()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x00488410
LegoBool32 SplashCinematicScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	Destroy();

	g_creditsTime = 0.0f;

	m_menuId = p_createParams->m_menuId;
	p_createParams->m_menuId = 1;

	if (!MenuSceneScreen::Initialize(p_context, p_createParams)) {
		return FALSE;
	}

	undefined4 nextMenu = 0;
	undefined4 pushExtra = FALSE;

	switch (m_menuId) {
	case 43:
		nextMenu = 0x15;
		break;
	case 41:
		nextMenu = 0x14;
		break;
	case 1:
		nextMenu = 0x12;
		break;
	case 39:
		nextMenu = 0x11;
		pushExtra = TRUE;
		break;
	}

	if (nextMenu) {
		StartMenuMusic(p_context, nextMenu, pushExtra);
	}

	m_sceneWidget.m_skippable = 0;
	return TRUE;
}

// FUNCTION: LEGORACERS 0x004884f0 FOLDED
LegoBool32 SplashCinematicScreen::Destroy()
{
	if (m_context) {
		if (m_context->m_modelBuilder.GetMusicGroup()) {
			if (m_context->m_modelBuilder.GetMusicInstance()) {
				m_context->m_modelBuilder.GetMusicInstance()->Stop();
				m_context->m_modelBuilder.GetMusicGroup()->DestroyMusicInstance(
					m_context->m_modelBuilder.GetMusicInstance()
				);
				m_context->m_modelBuilder.SetMusicInstance(NULL);
			}
		}
	}

	return MenuSceneScreen::Destroy();
}

// FUNCTION: LEGORACERS 0x00488550
void SplashCinematicScreen::CreateWidgets()
{
	switch (m_menuId) {
	case c_menuN64Intro:
		if (g_hashTable) {
			g_hashTable->SetCurrentEntryFromString("MENUDATA\\N64INTR");
		}
		break;
	case c_menuLegoIntro:
		if (g_hashTable) {
			g_hashTable->SetCurrentEntryFromString("MENUDATA\\LEGOINTR");
		}
		break;
	case c_menuSplash:
		if (g_hashTable) {
			g_hashTable->SetCurrentEntryFromString("MENUDATA\\HVSINTRO");
		}
		break;
	case c_menuLegal:
		if (g_hashTable) {
			g_hashTable->SetCurrentEntryFromString("MENUDATA\\LEGAL");
		}
		break;
	case c_menuCredits:
		if (g_hashTable) {
			g_hashTable->SetCurrentEntryFromString("MENUDATA\\CREDITS");
		}
		break;
	}

	CreateRegion(&m_sceneWidget, m_menuId);
}

// FUNCTION: LEGORACERS 0x00488630
void SplashCinematicScreen::Navigate()
{
	m_context->m_menuStack.Pop();
	m_cursor->SetCursorEnabled(TRUE);

	switch (m_menuId) {
	case c_menuN64Intro:
		m_context->m_menuStack.Push(c_menuLegoIntro);
		return;
	case c_menuLegoIntro:
		m_context->m_menuStack.Push(c_menuSplash);
		return;
	case c_menuSplash:
		m_context->m_menuStack.Push(c_menuMainIntro);
		return;
	case c_menuLegal:
		m_context->m_menuStack.Push(c_menuMainMenu);
		m_context->m_menuStack.Push(c_menuLoadAll);
		return;
	case c_menuCredits:
		if (!m_context->m_menuStack.GetSize()) {
			m_context->m_menuStack.Push(c_menuMainMenu);
		}
		return;
	}
}

// FUNCTION: LEGORACERS 0x00488720
LegoBool32 SplashCinematicScreen::Update(undefined4 p_elapsedMs)
{
	if (m_menuId != c_menuLegoIntro) {
		m_sceneWidget.m_skippable = 1;
	}

	if (m_menuId == c_menuCredits) {
		g_creditsTime += static_cast<float>(p_elapsedMs) / 1000.0f;
		// A skip finishes the scene early; otherwise the scroll decides when the credits end.
		if (g_creditsHeight > 0.0f && g_creditsTime * c_creditSpeed > g_creditsHeight + 480.0f) {
			m_sceneWidget.m_finished = TRUE;
		}
	}

	return MenuSceneScreen::Update(p_elapsedMs);
}

// [library:config] The credits scene still supplies the black backdrop and the skip handling; its
// own text pages are hidden and replaced by one continuous scrolling list.
LegoBool32 SplashCinematicScreen::Draw(Rect* p_arg1, Rect* p_arg2)
{
	if (m_menuId != c_menuCredits) {
		return MenuSceneScreen::Draw(p_arg1, p_arg2);
	}

	MiniwinSetSceneTextHidden(true);
	LegoBool32 result = MenuSceneScreen::Draw(p_arg1, p_arg2);
	MiniwinSetSceneTextHidden(false);

	GolFont* font = m_renderer->FindFontByName("font_ths");
	if (!font) {
		return result;
	}

	float y = 480.0f - g_creditsTime * c_creditSpeed;
	float cursor = 0.0f;
	for (LegoS32 i = 0; i < c_creditCount; i++) {
		LegoU32 color;
		float scale;
		float before;
		float step;
		CreditStyle(g_credits[i].m_kind, &color, &scale, &before, &step);
		cursor += before;
		float top = y + cursor;
		cursor += step;

		if (top > 480.0f || top < -48.0f) {
			continue;
		}

		LegoS32 width;
		LegoS32 height;
		font->MeasureString(g_credits[i].m_text, &width, &height);
		LegoS32 x = 320 - static_cast<LegoS32>(static_cast<float>(width) * scale * 0.5f);
		font->SetColor(color);
		m_renderer->DrawCString(g_credits[i].m_text, font, x, static_cast<LegoS32>(top), scale, scale, NULL, 0);
	}
	g_creditsHeight = cursor;

	return result;
}
