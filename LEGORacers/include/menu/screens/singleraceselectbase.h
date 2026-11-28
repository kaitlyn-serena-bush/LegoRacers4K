#ifndef SINGLERACESELECTBASE_H
#define SINGLERACESELECTBASE_H

#include "golanimatedentity.h"
#include "golstringtable.h"
#include "menu/screens/menuscenescreen.h"
#include "menu/widgets/menucarousel.h"
#include "menu/widgets/menucarouselnavigator.h"
#include "menu/widgets/menuframe.h"
#include "menu/widgets/menuselector.h"
#include "menu/widgets/menutextlabel.h"
#include "race/data/circuitdefinitionlist.h"

class GolModelBase;

// VTABLE: LEGORACERS 0x004b3f00
// SIZE 0x1908
class SingleRaceSelectBase : public MenuSceneScreen {
public:
	SingleRaceSelectBase();

	void CreateWidgets() override;                                             // vtable+0x4c
	~SingleRaceSelectBase() override;                                          // vtable+0x68
	LegoBool32 Destroy() override;                                             // vtable+0x74
	LegoBool32 Initialize(MenuGameContext*, MenuScreenCreateParams*) override; // vtable+0x8c

	// SYNTHETIC: LEGORACERS 0x00488820
	// SingleRaceSelectBase::`scalar deleting destructor'

protected:
	void SetPreviewDriver(const LegoChar* p_name);
	void ApplyThemeColor(LegoS32 p_index);
	void CreateModeSelector();                                    // [library:config]
	void LayoutModeSelector(MenuWidget* p_title, LegoS32 p_mode); // [library:config]
	LegoBool32 HandleModeChanged(MenuWidget* p_source);           // [library:config]
	LegoBool32 NavigateModeSwitch();                              // [library:config]

	LegoChar m_driverName[8];                                 // 0x0658
	GolAnimatedEntity m_driverEntity;                         // 0x0660
	GolModelBase* m_driverModel;                              // 0x0754
	MenuFrame m_borderFrame;                                  // 0x0758
	MenuCarouselNavigator m_trackCarousel;                    // 0x0b54
	MenuSelector m_trackSelector;                             // 0x0be8
	MenuImage m_trackIcons[7];                                // 0x15dc
	MenuTriangle m_sceneOverlay;                              // 0x1860
	CircuitDefinitionList::CircuitDefinition* m_circuitEntry; // 0x1904

	// [library:config] LegoRacers4K: SINGLE PLAYER is one menu; arrows either side of the title
	// cycle Circuit Race -> Single Race -> Time Trial (menus 5, 6, 0x1d).
	MenuCarouselNavigator m_modeCarousel;
	MenuSelector m_modeSelector;
	MenuTextLabel m_modeLabels[3];
	LegoU16 m_modeSwitchMenuId = 0;
	LegoS32 m_modeCurrent = 0;
	LegoBool32 m_modeSelectorActive = FALSE;
	LegoBool32 m_modeVersus = FALSE;

	// Multiplayer: players and laps pickers on the race screens.
	enum {
		c_versusTextCount = 8
	};
	MenuCarouselNavigator m_playersCarousel;
	MenuSelector m_playersSelector;
	MenuTextLabel m_playersLabels[3];
	MenuCarouselNavigator m_lapsCarousel;
	MenuSelector m_lapsSelector;
	MenuTextLabel m_lapsLabels[5];
	GolString m_versusStrings[c_versusTextCount];
	undefined2 m_versusBuffers[c_versusTextCount][12];
};

#endif // SINGLERACESELECTBASE_H
