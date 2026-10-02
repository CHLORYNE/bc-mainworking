/* SCENARIO INCENDIE - fire scenario editor: the editing screen. The chart on the left, with the
   own ship, the burning ship, the SAR boats and their routes, the survivors and the helicopter
   bases / pads drawn on it; the settings panel on the right, one tab per part of the exercise. */
#ifndef __FIREEDITOR_HPP_INCLUDED__
#define __FIREEDITOR_HPP_INCLUDED__

#include <string>
#include <vector>

#include "irrlicht.h"
#include "../chartView/ChartView.hpp"
#include "FireScenario.hpp"

class FireEditor : public irr::IEventReceiver {
public:
    FireEditor(irr::IrrlichtDevice* device, FireScenario* scenario, ChartView* map, const std::string& scenariosPath,
        const std::vector<std::string>& ownShipTypes, const std::vector<std::string>& otherShipTypes,
        const std::vector<std::string>& rescueShipTypes, bool isNewScenario);
    ~FireEditor();

    virtual bool OnEvent(const irr::SEvent& event);

    // Runs until the user goes back to the menu (true) or closes the window (false).
    bool run();

private:
    enum PickType { Pick_None, Pick_Own, Pick_Casualty, Pick_Boat, Pick_Out, Pick_Ret, Pick_Survivor, Pick_HeloBase, Pick_HeloPad, Pick_Traffic };
    struct Pick {
        PickType type; int index; int sub;
        Pick() : type(Pick_None), index(-1), sub(-1) {}
        Pick(PickType t, int i, int s = -1) : type(t), index(i), sub(s) {}
        bool operator==(const Pick& o) const { return type == o.type && index == o.index && sub == o.sub; }
    };
    enum Tool { Tool_Select, Tool_PlaceOwn, Tool_PlaceCasualty, Tool_PlaceBoat, Tool_AddOut, Tool_AddRet,
        Tool_AddMob, Tool_AddLiferaft, Tool_AddRadeau, Tool_PlaceHeloBase, Tool_PlaceHeloPad };

    // --- building the panel
    void buildGui();
    irr::gui::IGUIStaticText* label(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, const wchar_t* text);
    irr::gui::IGUIEditBox* edit(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, irr::s32 id);
    irr::gui::IGUIButton* button(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, irr::s32 id, const wchar_t* text);
    irr::gui::IGUIComboBox* combo(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, irr::s32 id);
    void fillTypeCombo(irr::gui::IGUIComboBox* box, const std::vector<std::string>& types, const std::string& current);

    // --- keeping the panel in step with the data
    void refreshAll();
    void refreshScenarioTab();
    void refreshCasualtyTab();
    void refreshSurvivorTab();
    void refreshBoatTab();
    void refreshHeloTab();
    void refreshInfo();
    void setTool(Tool t);
    void select(const Pick& p, bool switchTab);

    // --- edits
    bool onGuiEvent(const irr::SEvent::SGUIEvent& e);
    void onEditChanged(irr::s32 id, const std::wstring& text);
    void onComboChanged(irr::s32 id, irr::gui::IGUIComboBox* box);
    void onButton(irr::s32 id);
    void clickMap(irr::core::position2di at);
    void deleteSelected();
    void removeBoat(int index);
    void moveObject(const Pick& p, const IncidentPoint& to);
    IncidentPoint* positionOf(const Pick& p);
    float* headingOf(const Pick& p);
    bool save(bool confirmedOverwrite);
    void markDirty();

    // --- map
    Pick pickAt(irr::core::position2di at);
    bool overMap(irr::core::position2di at) const;
    void drawMap();
    void drawOverlay();
    void drawTimeline();
    void drawShip(const IncidentPoint& p, float heading, irr::video::SColor fill, irr::video::SColor edge, float size);
    void fillPolygon(const std::vector<irr::core::position2df>& pts, irr::video::SColor color);
    void drawPolyline(const std::vector<irr::core::position2di>& pts, irr::video::SColor color, bool dashed, irr::s32 width);
    // Map labels sit on a dark translucent box so they read on any chart background.
    void drawText(const std::wstring& text, irr::core::position2di at, irr::video::SColor color, bool centred = false, bool boxed = true);
    void drawMarker(irr::core::position2di at, int shape, irr::video::SColor fill, irr::video::SColor edge, irr::s32 r);
    void drawHighlight(const Pick& p, irr::video::SColor color);
    irr::video::SColor boatColour(int index) const;
    std::wstring survivorName(int kind) const;
    std::wstring toolHint() const;
    std::wstring boatLabel(int index) const;
    std::wstring rescueModelsLine() const;

    irr::IrrlichtDevice* device;
    irr::video::IVideoDriver* driver;
    irr::gui::IGUIEnvironment* guienv;
    irr::gui::IGUIFont* font;
    FireScenario* scn;
    ChartView* map;
    std::string scenariosPath;
    std::string loadedName;          // folder the scenario came from ("" for a new one)
    std::vector<std::string> ownTypes, otherTypes;
    std::vector<std::string> rescueTypes;   // other-ship models with FireFighting=1: the only ones offered for SAR boats

    irr::s32 screenW, screenH, panelX, rowH;
    bool quit, backToMenu, dirty, needRefresh;
    Tool tool;
    Pick selected, hover;
    int selBoat, selHelo, selSurvivor;
    bool moveGroupWithCasualty;

    // mouse
    bool leftDown, rightDown, panning, dragging, rightMoved;
    irr::core::position2di lastMouse, dragOffset, rightDownAt;
    irr::core::position2di mouse;

    // panel widgets
    irr::gui::IGUIEditBox* nameBox;
    irr::gui::IGUIStaticText* statusText;
    irr::gui::IGUIButton* styleButton;   // chart background
    irr::gui::IGUITabControl* tabs;
    irr::gui::IGUITab* tabScenario; irr::gui::IGUITab* tabCasualty; irr::gui::IGUITab* tabSurvivors;
    irr::gui::IGUITab* tabBoats; irr::gui::IGUITab* tabHelos;
    irr::gui::IGUIEditBox *dayBox, *monthBox, *yearBox, *startBox, *sunriseBox, *sunsetBox, *weatherBox, *rainBox,
        *visBox, *windDirBox, *windSpdBox, *mrscBox, *descBox, *ownHdgBox, *ownSpdBox;
    irr::gui::IGUIComboBox* ownTypeBox;
    irr::gui::IGUIStaticText* trafficText;
    irr::gui::IGUIComboBox* casTypeBox;
    irr::gui::IGUIEditBox *casHdgBox, *abandonBox, *spreadBox, *durationBox, *sinkLeadBox, *permListBox, *intervalBox;
    irr::gui::IGUICheckBox* moveGroupBox;
    irr::gui::IGUIStaticText* casInfoText;
    irr::core::recti timelineArea;   // relative to tabCasualty
    irr::gui::IGUIListBox* survList;
    irr::gui::IGUIComboBox* survBoatBox;
    irr::gui::IGUIStaticText* survInfoText;
    irr::gui::IGUIListBox* boatList;
    irr::gui::IGUIComboBox* boatTypeBox;
    irr::gui::IGUIEditBox *boatHdgBox, *boatSpeedBox, *boatDelayBox, *boatMoorBox;
    irr::gui::IGUIStaticText* boatInfoText;
    irr::gui::IGUIEditBox *heloDelayBox, *heloSpeedBox, *heloBaseHBox, *heloPadHBox;
    irr::gui::IGUIListBox* heloList;
    irr::gui::IGUIComboBox* heloModelBox;
    irr::gui::IGUIStaticText* heloInfoText;
};

#endif
