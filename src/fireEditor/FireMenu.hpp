/* SCENARIO INCENDIE - fire scenario editor: the start screen. Charts on the left (with the
   chart's details), exercises on the right (with the exercise's details), and the same
   management tools as the standard editor: open, delete, import, export. */
#ifndef __FIREMENU_HPP_INCLUDED__
#define __FIREMENU_HPP_INCLUDED__

#include <string>
#include <vector>

#include "irrlicht.h"
#include "FireScenario.hpp"

// Sub-folders of path (no hidden ones), appended to out without duplicates.
void listSubdirectories(irr::IrrlichtDevice* device, std::vector<std::string>& out, const std::string& path);

class FireMenu : public irr::IEventReceiver {
public:
    enum Choice { Choice_Quit, Choice_New, Choice_Open, Choice_Import };

    FireMenu(irr::IrrlichtDevice* device, const std::string& scenariosPath, const std::wstring& message);
    ~FireMenu();

    Choice run();
    std::string chosenWorld() const;      // Choice_New
    std::string chosenScenario() const;   // Choice_Open
    FireScenario imported;                // Choice_Import: the exercise read from the pasted text

    virtual bool OnEvent(const irr::SEvent& event);

private:
    void build(const std::wstring& message);
    void refreshLists();
    void updateMapDetails();
    void updateScenarioDetails();
    void addWrapped(irr::gui::IGUIListBox* box, const std::wstring& text, irr::video::SColor colour);
    void addWrapped(irr::gui::IGUIListBox* box, const std::wstring& text);
    void deleteSelectedScenario();
    void openImportExport(bool importing);
    void closeImportExport();
    void importFromText();
    void setStatus(const std::wstring& text, bool error);

    irr::IrrlichtDevice* device;
    irr::gui::IGUIEnvironment* env;
    std::string scenariosPath;
    std::string userFolder;
    std::vector<std::string> worlds, scenarios;
    std::vector<bool> hasIncident;
    irr::s32 rowH;
    int choice;   // -1 while the menu is up

    irr::gui::IGUIListBox* worldList;
    irr::gui::IGUIListBox* scenarioList;
    irr::gui::IGUIListBox* mapDetails;
    irr::gui::IGUIListBox* scenarioDetails;
    irr::gui::IGUIStaticText* statusText;
    irr::gui::IGUIWindow* ieWindow;      // import / export
    irr::gui::IGUIEditBox* ieText;
    bool ieImporting;
    irr::gui::IGUIFont* previousFont;
    irr::video::SColor previousHighlight;  // skin font before the menu (given back)

    static std::string rememberedWorld, rememberedScenario;   // selection kept between visits
};

#endif
