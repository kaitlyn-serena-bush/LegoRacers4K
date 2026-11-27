#ifndef PICKRACERSCREEN_H
#define PICKRACERSCREEN_H

#include "compat.h"
#include "decomp.h"
#include "golstring.h"
#include "menu/screens/racerpickscreenbase.h"
#include "menu/widgets/menuimage.h"
#include "menu/widgets/menutextbutton.h"

// VTABLE: LEGORACERS 0x004b3b68
// SIZE 0x4c8c
class PickRacerScreen : public RacerPickScreenBase {
public:
	PickRacerScreen();

	void OnIconUnfocused(MenuWidget*) override;                                // vtable+0x38
	void OnWidgetValueChanged(MenuWidget*) override;                           // vtable+0x44
	void CreateWidgets() override;                                             // vtable+0x4c
	~PickRacerScreen() override;                                               // vtable+0x68
	void Navigate() override;                                                  // vtable+0x84
	LegoBool32 Initialize(MenuGameContext*, MenuScreenCreateParams*) override; // vtable+0x8c

	// SYNTHETIC: LEGORACERS 0x00484cc0
	// PickRacerScreen::`scalar deleting destructor'

protected:
	MenuImage m_photoImage;            // 0x3ff4
	MenuTextLabel m_infoLabel;         // 0x4050
	MenuTextButton m_startButton;      // 0x40c8
	MenuTextButton m_nextPlayerButton; // 0x43b8
	MenuTextButton m_controlsButton;   // 0x46a8
	MenuTextButton m_backButton;       // 0x4998
	undefined4 m_unk0x4c88;            // 0x4c88

	// [library:input] 4-player couch co-op: player 3/4 titles and the "add player" button.
	void SetCoopText(GolString* p_string, undefined2* p_buffer, LegoU32 p_size, const LegoChar* p_text);
	void SetupCoopVersusButtons();
	GolString m_coopTitle;
	undefined2 m_coopTitleBuffer[64];
	GolString m_coopAddText;
	undefined2 m_coopAddBuffer[32];
};

#endif // PICKRACERSCREEN_H
