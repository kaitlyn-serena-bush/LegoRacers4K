#ifndef COOPPICKSCREEN_H
#define COOPPICKSCREEN_H

// [library:input] LegoRacers4K: Versus racer pick for 2-4 players at once (portable build only).
//
// The screen is split into one panel per player. A player joins by pressing
// "accelerate" on a gamepad or on one of the four keyboard sets in controls.ini;
// the panel then shows that preset's keys and the player uses their own
// left/right to change racer, accelerate to lock in and brake to undo or leave.
// When every player is ready the race (or Grand Prix) starts.

#include "app/coopcontrols.h"
#include "golstring.h"
#include "menu/screens/racermodelscreenbase.h"
#include "menu/widgets/menuimage.h"
#include "menu/widgets/menutextlabel.h"

class InputDevice;

class CoopPickScreen : public RacerModelScreenBase {
public:
	enum {
		c_panelCount = 4,
		c_lineCount = 5, // title, name, status/legend x2, hint
		c_textLength = 64,
	};

	CoopPickScreen();
	~CoopPickScreen() override;

	LegoBool32 HandleKeyDown(MenuWidget*, InputEventQueue::Event*, undefined4, undefined4) override;
	void CreateWidgets() override;
	void Reset() override;
	LegoBool32 Update(undefined4) override;
	LegoBool32 Destroy() override;
	void Navigate() override;
	LegoBool32 Initialize(MenuGameContext*, MenuScreenCreateParams*) override;

private:
	LegoS32 PresetForEvent(InputEventQueue::Event* p_event, LegoS32* p_action);
	void SetLine(LegoS32 p_panel, LegoS32 p_line, const char* p_text);
	void RefreshPanel(LegoS32 p_panel);
	void SetSlotDimmed(LegoS32 p_panel, LegoBool32 p_dimmed);
	void ChangeRacer(LegoS32 p_panel, LegoBool32 p_next);

	MenuImage m_photoImage;
	MenuTextLabel m_lines[c_panelCount][c_lineCount];
	GolString m_lineStrings[c_panelCount][c_lineCount];
	undefined2 m_lineBuffers[c_panelCount][c_lineCount][c_textLength];
	LegoU32 m_presetEvents[c_coopPresetCount][c_coopActionCount];
	LegoS32 m_panelPresets[c_panelCount];
	LegoBool32 m_panelReady[c_panelCount];
	LegoBool32 m_panelDimmed[c_panelCount];
	LegoS32 m_playerCount;
};

#endif // COOPPICKSCREEN_H
