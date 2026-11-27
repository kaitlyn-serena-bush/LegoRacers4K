#ifndef VERSUSSETUPSCREEN_H
#define VERSUSSETUPSCREEN_H

// [library:config] LegoRacers4K: Versus race setup (portable build only).
// Race type (single race or Grand Prix), number of players and laps, then the
// track or circuit screen and the simultaneous racer pick. Uses options.mib.

#include "golstring.h"
#include "menu/screens/menugamescreen.h"
#include "menu/widgets/menucarouselnavigator.h"
#include "menu/widgets/menuimage.h"
#include "menu/widgets/menuselector.h"
#include "menu/widgets/menutextbutton.h"
#include "menu/widgets/menutextlabel.h"

class VersusSetupScreen : public MenuGameScreen {
public:
	enum {
		c_rowCount = 3,
		c_maxItems = 5,
	};

	VersusSetupScreen();
	~VersusSetupScreen() override;

	void OnIconUnfocused(MenuWidget*) override;
	void OnWidgetValueChanged(MenuWidget*) override;
	void CreateWidgets() override;
	void Navigate() override;
	LegoBool32 Initialize(MenuGameContext*, MenuScreenCreateParams*) override;

private:
	MenuImage m_photoImage;
	MenuTextLabel m_infoLabel;
	MenuCarouselNavigator m_carousels[c_rowCount];
	MenuSelector m_selectors[c_rowCount];
	MenuTextLabel m_items[c_rowCount][c_maxItems];
	MenuTextLabel m_titles[c_rowCount];
	GolString m_titleStrings[c_rowCount];
	undefined2 m_titleBuffers[c_rowCount][32];
	MenuTextButton m_okButton;
	MenuTextButton m_backButton;
};

#endif // VERSUSSETUPSCREEN_H
