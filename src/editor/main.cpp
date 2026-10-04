#define _CRT_SECURE_NO_WARNINGS
#include "irrlicht.h"
#include <iostream>
#include "irrlicht.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string>
//pc clock
#include <ctime>
#include "PositionDataStruct.hpp"
#include "StartupEventReceiver.hpp"
//#include "Network.hpp"
#include "ControllerModel.hpp"
#include "GUI.hpp"
#include "ImportExportGUI.hpp"
#include "EventReceiver.hpp"

#include "../Constants.hpp"
#include "../IniFile.hpp"
#include "../Lang.hpp"
#include "../Utilities.hpp"
#include "../ScenarioDataStructure.hpp"
#include "../EditorStartScreen.hpp"


//Mac OS:
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#endif //__APPLE__

#ifdef _MSC_VER
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

//Includes for copying scenario files
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // Also for GetSystemMetrics
#include <Shellapi.h>
#else // _WIN32
#ifdef __APPLE__
#include <copyfile.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#endif
#endif // __APPLE__

#ifdef __linux__
#include <unistd.h>
#endif

// Irrlicht Namespaces
//using namespace irr;


//Set up global for ini reader to have access to irrlicht logger if needed.
namespace IniFile {
    irr::ILogger* irrlichtLogger = 0;
}

//To do: Utility function to find scenario list
void getDirectoryList(irr::IrrlichtDevice* device, std::vector<std::string>& dirList, std::string path) {

    irr::io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem == 0) {
        std::cout << "Failed to get access to file system" << std::endl;
        exit(EXIT_FAILURE);
    }
    //store current dir
    irr::io::path cwd = fileSystem->getWorkingDirectory();

    //change to dir
    if (!fileSystem->changeWorkingDirectoryTo(path.c_str())) {
        std::cout << "Failed to change to scenario directory" << std::endl;
        exit(EXIT_FAILURE); //Couldn't change to dir
    }

    irr::io::IFileList* fileList = fileSystem->createFileList();
    if (fileList == 0) {
        std::cout << "Could not get scenario list" << std::endl;
        exit(EXIT_FAILURE); //Could not get file list for scenarios TODO: Message for user
    }

    //List here
    for (irr::u32 i = 0; i < fileList->getFileCount(); i++) {
        if (fileList->isDirectory(i)) {
            const irr::io::path& fileName = fileList->getFileName(i);
            if (fileName.findFirst('.') != 0) { //Check it doesn't start with '.' (., .., or hidden)
                //std::cout << fileName.c_str() << std::endl;
                dirList.push_back(fileName.c_str());
            }
        }
    }

    //change back
    if (!fileSystem->changeWorkingDirectoryTo(cwd)) {
        std::cout << "Could not return to normal working directory" << std::endl;
        exit(EXIT_FAILURE); //Couldn't change dir back
    }
    fileList->drop();
}

void findWhatToLoad(irr::IrrlichtDevice* device, ScenarioData* scenarioData, std::string& worldName, std::string& scenarioName, bool& multiplayer, Lang* language, std::string userFolder)
//Will fill one of worldName of scenarioName, depending on user's selection.
{

    irr::video::IVideoDriver* driver = device->getVideoDriver();

    //Get screen width
    irr::u32 su = driver->getScreenSize().Width;
    irr::u32 sh = driver->getScreenSize().Height;

    //Find list of scenarios, and list of available world models
    std::string scenarioPath = "Scenarios/";
    if (Utilities::pathExists(userFolder + scenarioPath)) {
        scenarioPath = userFolder + scenarioPath;
    }
    std::vector<std::string> scenarioDirList;
    getDirectoryList(device, scenarioDirList, scenarioPath); //Populates scenarioDirList

    std::string worldPath = "World/";
    std::vector<std::string> worldDirList;
    getDirectoryList(device, worldDirList, worldPath); //Populates worldDirList
    if (Utilities::pathExists(userFolder + worldPath)) {
        worldPath = userFolder + worldPath;
        getDirectoryList(device, worldDirList, worldPath); //Append to worldDirList, for user specific world models
    }
    // --- ADD THIS BLOCK TO LOAD YOUR CUSTOM WINDOW ICON ---
#ifdef _WIN32
    // Load your custom .ico file from the media folder
    HICON hIcon = (HICON)LoadImageA(NULL, "media/myIcon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    if (hIcon) {
        // Extract the native Windows window handle (HWND) from the Irrlicht engine
        irr::video::SExposedVideoData videoData = driver->getExposedVideoData();
        HWND hwnd = reinterpret_cast<HWND>(videoData.OpenGLWin32.HWnd);

        // Attach the icon to the window's title bar and taskbar
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }
#endif

    // MODERN MARITIME COLOR CHANGES
    irr::gui::IGUIEnvironment* guienv = device->getGUIEnvironment();
    irr::gui::IGUISkin* skin = guienv->getSkin();

    // Palette: the launcher's navy (A, R, G, B)
    irr::video::SColor bgDark(240, 14, 26, 44);       // Main window bg (slight transparency)
    irr::video::SColor panelColor(255, 28, 46, 72);   // Buttons, lists, tabs
    irr::video::SColor borderDark(255, 12, 22, 38);   // Shadows/Borders
    irr::video::SColor borderLight(255, 46, 72, 106); // Highlights
    irr::video::SColor textMain(255, 240, 245, 250);  // Off-white text
    irr::video::SColor highlightBlue(255, 36, 122, 222); // Selection highlight
    irr::video::SColor editBg(255, 8, 16, 30);        // Darker inset for text inputs

    // Apply Base & Windows
    skin->setColor(irr::gui::EGDC_WINDOW, bgDark);
    skin->setColor(irr::gui::EGDC_3D_FACE, panelColor);

    // Flatten the old Windows 95-style 3D effects
    skin->setColor(irr::gui::EGDC_3D_SHADOW, borderDark);
    skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, borderDark);
    skin->setColor(irr::gui::EGDC_3D_HIGH_LIGHT, borderLight);

    // Text Styling
    skin->setColor(irr::gui::EGDC_BUTTON_TEXT, textMain);
    skin->setColor(irr::gui::EGDC_GRAY_TEXT, borderLight);
    skin->setColor(irr::gui::EGDC_TOOLTIP, textMain);

    // Selections / Highlighting
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT, highlightBlue);
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT_TEXT, irr::video::SColor(255, 255, 255, 255));

    // Checkbox tick / window symbols (also combo + scrollbar arrows) -> azure blue.
    // Irrlicht draws the checkbox tick via drawIcon(), which uses EGDC_WINDOW_SYMBOL.
    skin->setColor(irr::gui::EGDC_WINDOW_SYMBOL, highlightBlue);

    // Edit Boxes & Inputs
    skin->setColor(irr::gui::EGDC_EDITABLE, editBg);
    skin->setColor(irr::gui::EGDC_FOCUSED_EDITABLE, borderLight);
    skin->setColor(irr::gui::EGDC_GRAY_EDITABLE, bgDark);

    //KYARA EDITOR UI CHANGE 
    const irr::s32 SCENARIO_BOX_ID = 101;
    const irr::s32 WORLD_BOX_ID = 102;
    const irr::s32 OK_SCENARIO_BUTTON_ID = 103;
    const irr::s32 OK_WORLD_BUTTON_ID = 104;
    const irr::s32 IMPORT_SCENARIO_BUTTON_ID = 105;
    const irr::s32 EXPORT_SCENARIO_BUTTON_ID = 106;
    const irr::s32 IMPORT_EXPORT_OK_BUTTON_ID = 107;
    const irr::s32 DELETE_SCENARIO_BUTTON_ID = 108; // Include the delete ID

    // --- Start screen: maps on the left, the chart in the middle, the exercises on the right ---
    irr::gui::IGUIWindow* scnWorldChoiceWindow = device->getGUIEnvironment()->addWindow(irr::core::rect<irr::s32>(0, 0, su, sh), false);
    scnWorldChoiceWindow->getCloseButton()->setVisible(false);
    scnWorldChoiceWindow->setDrawTitlebar(false);
    scnWorldChoiceWindow->setDrawBackground(false);
    scnWorldChoiceWindow->setDraggable(false); //else dragging an empty spot moves the lists off their cards

    //Drawn first: header, cards and chart behind the lists and buttons.
    EditorStartScreen* screen = new EditorStartScreen(device, scnWorldChoiceWindow, L"\u00C9diteur de sc\u00E9nario",
        L"NAUTITECH  \u00B7  Conception des exercices de navigation", irr::video::SColor(255, 40, 130, 235), true);
    screen->drop();
    screen->setCardTitles(L"CARTES", L"LA CARTE", L"EXERCICES EXISTANTS", L"D\u00C9TAILS DE L'EXERCICE");
    screen->setFooter(L"Choisissez une carte pour cr\u00E9er un exercice, ou un exercice existant pour le modifier (double-clic pour l'ouvrir).");
    irr::gui::IGUIFont* listFont = screen->font(15);
    irr::gui::IGUIFont* editorFont = skin->getFont(); //given back when the editor opens
    if (listFont) { skin->setFont(listFont); }
    irr::gui::IGUIStaticText* headerText = 0;
    irr::gui::IGUIStaticText* footerText = 0;
    (void)headerText;
    (void)footerText;

    const irr::s32 buttonH = (irr::s32)screen->buttonHeight();
    //Maps: the list, then the multiplayer box and "new exercise" at the bottom of the card
    const irr::core::rect<irr::f32> left = screen->leftCard();
    const irr::core::rect<irr::s32> leftArea = screen->content(left, buttonH * 2 + 14);
    irr::gui::IGUIListBox* worldListBox = device->getGUIEnvironment()->addListBox(leftArea, scnWorldChoiceWindow, WORLD_BOX_ID);
    worldListBox->setDrawBackground(false);
    const irr::s32 lx0 = leftArea.UpperLeftCorner.X, lx1 = leftArea.LowerRightCorner.X, ly = leftArea.LowerRightCorner.Y + 8;
    irr::gui::IGUICheckBox* multiplayerBox = device->getGUIEnvironment()->addCheckBox(false, irr::core::rect<irr::s32>(lx0 + 4, ly, lx1, ly + buttonH),
        scnWorldChoiceWindow, -1, language->translate("multiplayer").c_str());
    Ui::Button* worldOK = new Ui::Button(device->getGUIEnvironment(), scnWorldChoiceWindow, OK_WORLD_BUTTON_ID,
        irr::core::rect<irr::s32>(lx0, ly + buttonH + 6, lx1, ly + 2 * buttonH + 6), L"Nouveau sc\u00E9nario sur cette carte", Ui::Button::Primary);
    worldOK->drop();

    //Middle: the map's description under the chart
    irr::gui::IGUIListBox* mapThumbText = device->getGUIEnvironment()->addListBox(screen->content(screen->mapInfoCard()), scnWorldChoiceWindow, -1, true);
    mapThumbText->setDrawBackground(false);

    //Exercises: list, open / delete, details, import / export
    irr::gui::IGUIListBox* scenarioListBox = device->getGUIEnvironment()->addListBox(screen->content(screen->rightCard()), scnWorldChoiceWindow, SCENARIO_BOX_ID);
    scenarioListBox->setDrawBackground(false);
    Ui::Button* scenarioOK = new Ui::Button(device->getGUIEnvironment(), scnWorldChoiceWindow, OK_SCENARIO_BUTTON_ID,
        screen->half(screen->actionRow(), 0), L"\u00C9diter le sc\u00E9nario", Ui::Button::Primary);
    scenarioOK->drop();
    Ui::Button* deleteScenario = new Ui::Button(device->getGUIEnvironment(), scnWorldChoiceWindow, DELETE_SCENARIO_BUTTON_ID,
        screen->half(screen->actionRow(), 1), L"Supprimer le sc\u00E9nario", Ui::Button::Danger);
    deleteScenario->drop();
    irr::gui::IGUIListBox* metadataPanel = device->getGUIEnvironment()->addListBox(screen->content(screen->detailsCard()), scnWorldChoiceWindow, -1, true);
    metadataPanel->setDrawBackground(false);
    Ui::Button* importScenario = new Ui::Button(device->getGUIEnvironment(), scnWorldChoiceWindow, IMPORT_SCENARIO_BUTTON_ID,
        screen->half(screen->bottomRow(), 0), L"Importer", Ui::Button::Secondary);
    importScenario->drop();
    Ui::Button* exportScenario = new Ui::Button(device->getGUIEnvironment(), scnWorldChoiceWindow, EXPORT_SCENARIO_BUTTON_ID,
        screen->half(screen->bottomRow(), 1), L"Exporter", Ui::Button::Secondary);
    exportScenario->drop();
    const irr::gui::IGUIElement* buttons[5] = { worldOK, scenarioOK, deleteScenario, importScenario, exportScenario };
    for (int i = 0; i < 5; i++) { ((Ui::Button*)buttons[i])->setFont(listFont); }

    //The chart follows the map selected, or the map of the exercise selected.
    screen->follow(worldListBox, scenarioListBox, [&scenarioDirList, scenarioPath](irr::s32 index) -> std::string {
        if (index < 0 || index >= (irr::s32)scenarioDirList.size()) { return ""; }
        return IniFile::iniFileToString(scenarioPath + scenarioDirList[index] + "/environment.ini", "Setting");
    });
    //Add scenarios to list box
    for (std::vector<std::string>::iterator it = scenarioDirList.begin(); it != scenarioDirList.end(); ++it) {
        scenarioListBox->addItem(irr::core::stringw(it->c_str()).c_str());
    }
    if (scenarioListBox->getItemCount() > 0) {
        scenarioListBox->setSelected(0);
    }

    //Add world to list box
    for (std::vector<std::string>::iterator it = worldDirList.begin(); it != worldDirList.end(); ++it) {
        worldListBox->addItem(irr::core::stringw(it->c_str()).c_str());
    }
    if (worldListBox->getItemCount() > 0) {
        worldListBox->setSelected(0);
    }

    GUIImportExport importExport(device, language, su, sh, IMPORT_EXPORT_OK_BUTTON_ID);
    importExport.setVisible(false, 0);

    // Link to our event receiver
    StartupEventReceiver startupReceiver(
        scenarioListBox,
        worldListBox,
        scnWorldChoiceWindow,
        SCENARIO_BOX_ID,
        WORLD_BOX_ID,
        OK_SCENARIO_BUTTON_ID,
        OK_WORLD_BUTTON_ID,
        IMPORT_SCENARIO_BUTTON_ID,
        EXPORT_SCENARIO_BUTTON_ID,
        IMPORT_EXPORT_OK_BUTTON_ID,
        DELETE_SCENARIO_BUTTON_ID,
        &importExport,
        scenarioData, metadataPanel, mapThumbText, driver); // REMOVED mapThumbnail from the end
    device->setEventReceiver(&startupReceiver);

    // NEW: Force an immediate update of both metadata panels right before we run the loop
    startupReceiver.forceUpdatePreview();

    // Run until we know scenario data to use
    while (device->run() && startupReceiver.getScenarioSelected() < 0 && startupReceiver.getWorldSelected() < 0 && scenarioData->dataPopulated == false) {
        driver->beginScene(true, true, irr::video::SColor(255, 12, 24, 42));
        device->getGUIEnvironment()->drawAll();
        driver->endScene();
    }

    if (scenarioData->dataPopulated == false) {
        // Normal case
        irr::s32 selectedScenario = startupReceiver.getScenarioSelected();
        if (selectedScenario >= 0 && selectedScenario < scenarioDirList.size()) {
            scenarioName = scenarioDirList.at(selectedScenario); //Get scenario name

            //check if name ends in _mp, and if so, record that this is a multiplayer scenario
            multiplayer = false;
            if (scenarioName.length() >= 3) {
                std::string endChars = scenarioName.substr(scenarioName.length() - 3, 3);
                if (endChars == "_mp" || endChars == "_MP") {
                    multiplayer = true;
                }
            }
        }

        irr::s32 selectedWorld = startupReceiver.getWorldSelected();
        if (selectedWorld >= 0 && selectedWorld < worldDirList.size()) {
            worldName = worldDirList.at(selectedWorld);
            multiplayer = multiplayerBox->isChecked();
        }
        //Store multiplayer status if new scenario    
    }
    else {
        // Scenario data already populated directly (by import), just set worldName, scenarioName, and multiplayer
        worldName = scenarioData->worldName;
        scenarioName = scenarioData->worldName;
        // multiplayerName isn't set in deserialise, so check and update here
        if (scenarioData->scenarioName.length() >= 3) {
            std::string endChars = scenarioData->scenarioName.substr(scenarioData->scenarioName.length() - 3, 3);
            if (endChars == "_mp" || endChars == "_MP") {
                scenarioData->multiplayerName = true;
            }
        }
        multiplayer = scenarioData->multiplayerName;
    }

    //Clean up
   //Clean up
    scenarioListBox->remove();
    worldListBox->remove();
    skin->setFont(editorFont);
    scenarioOK->remove();
    worldOK->remove();
    multiplayerBox->remove();
    device->setEventReceiver(0);

    //Show patience message
    irr::gui::IGUIStaticText* patienceText = device->getGUIEnvironment()->addStaticText(language->translate("loadingMap").c_str(), irr::core::rect<irr::s32>(0.01 * su, 0.04 * sh, 0.95 * su, 0.95 * sh), false, true, scnWorldChoiceWindow);
    if (device->run()) {
        driver->beginScene(true, true, irr::video::SColor(255, 12, 24, 42));
        device->getGUIEnvironment()->drawAll();
        driver->endScene();
    }

    //Finish cleaning up
    patienceText->remove();
    scnWorldChoiceWindow->remove();

}

int copyDir(std::string source, std::string dest)
{

    //Copy contents of source dir into dest dir

#ifdef _WIN32
    //Windows version: Creates dest dir automatically
    source.append(1, '\0'); //Add an extra null to end of string
    dest.append(1, '\0');
    replace(dest.begin(), dest.end(), '/', '\\'); //Replace / with \ in dest (think about network paths??)

    SHFILEOPSTRUCT fileOp;
    fileOp.wFunc = FO_COPY;
    fileOp.pFrom = source.c_str();
    fileOp.pTo = dest.c_str();
    fileOp.fFlags = /*FOF_SILENT | */FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;

    return SHFileOperation(&fileOp);
#else
#ifdef __APPLE__
    //Apple version: Requires that dest dir exists
    copyfile_state_t s;
    s = copyfile_state_alloc();
    //use copyfile here to do recursive copy
    int returnValue = copyfile(source.c_str(), dest.c_str(), s, COPYFILE_DATA | COPYFILE_RECURSIVE);
    copyfile_state_free(s);
    return returnValue;
#else // __APPLE__
    //Other posix
    //Note: Not implemented yet for other posix: need to implement recursive directory copy.
    //Requires that dest dir exists
    //std::cout << "Copying from:" << source << " to:" << dest << std::endl;
    if (!Utilities::pathExists(dest)) {
        return -1;
    }

    //For each folder at root level, create new folder in dest, and call copyDir on this
    DIR* dir = opendir(source.c_str());
    if (!dir) { return -1; }
    struct dirent* entry = readdir(dir);
    while (entry != NULL) {
        if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
            std::string newDir = dest;

            newDir.append(source);
            newDir.append("/");
            newDir.append(entry->d_name);
            //newDir.append("/");

            //std::cout << "Dest: " << dest << std::endl;
            //std::cout << "Trying to create '" << newDir << "'" << std::endl;
            if (mkdir(newDir.c_str(), 0755) == 0) {
                //Recursive here
                std::string fromDir = source;
                fromDir.append("/");
                fromDir.append(entry->d_name);

                std::string toDir = dest;

                copyDir(fromDir, toDir);
            }
            else {
                return -1;
            }
        }
        else if (entry->d_type == DT_REG) {
            //Copy file
            //entry->d_name;
            std::string newFile = dest;
            newFile.append(source);
            newFile.append("/");
            newFile.append(entry->d_name);

            std::string fromFile = source;
            fromFile.append("/");
            fromFile.append(entry->d_name);

            //std::cout << "About to try and create >>" << newFile << "<< from >>" << fromFile << "<<" << std::endl;

            std::ifstream fromStream(fromFile.c_str(), std::ios::binary);
            std::ofstream destStream(newFile.c_str(), std::ios::binary);
            if (fromStream && destStream) {
                destStream << fromStream.rdbuf();
            }

        }

        entry = readdir(dir);
    }

    //For each file at root level, create the file and copy contents


#endif // __APPLE__
#endif // _WIN32

    return -1;
}

void checkUserScenarioDir(void)
{
    //Check if scenarios are in the user dir, and if not, try to copy in
    std::string userFolder = Utilities::getUserDir();

    std::string scenarioPath = "Scenarios";

    if (!Utilities::pathExists(userFolder + scenarioPath)) {

#ifdef _WIN32
        std::cout << "Copying scenario files into " << userFolder + scenarioPath << std::endl;
        copyDir("Scenarios", userFolder + scenarioPath);
#else
        //Make sure destination folder for scenarios exists. Not needed on windows as the copy method creates the output folder and directories above it.
        if (!Utilities::pathExists(Utilities::getUserDirBase())) {
            std::string pathToMake = Utilities::getUserDirBase();
            if (pathToMake.size() > 1) { pathToMake.erase(pathToMake.size() - 1); } //Remove trailing slash
            mkdir(pathToMake.c_str(), 0755);
        }
        if (!Utilities::pathExists(Utilities::getUserDir())) {
            std::string pathToMake = Utilities::getUserDir();
            if (pathToMake.size() > 1) { pathToMake.erase(pathToMake.size() - 1); } //Remove trailing slash
            mkdir(pathToMake.c_str(), 0755);
        }
        if (!Utilities::pathExists(Utilities::getUserDir() + "Scenarios/")) {
            std::string pathToMake = Utilities::getUserDir() + "Scenarios";
            mkdir(pathToMake.c_str(), 0755);
        }
        std::cout << "Copying scenario files into " << userFolder << std::endl;
        copyDir("Scenarios", userFolder);
#endif // __APPLE__


    }
}

int main(int argc, char** argv)
{

#ifdef FOR_DEB
    chdir("/usr/share/bridgecommand");
#endif // FOR_DEB

    //Mac OS:
    //Find starting folder
#ifdef __APPLE__
    char exePath[1024];
    uint32_t pathSize = sizeof(exePath);
    std::string exeFolderPath = "";
    if (_NSGetExecutablePath(exePath, &pathSize) == 0) {
        std::string exePathString(exePath);
        size_t pos = exePathString.find_last_of("\\/");
        if (std::string::npos != pos) {
            exeFolderPath = exePathString.substr(0, pos);
        }
    }
    //change up from BridgeCommand.app/Contents/MacOS/ed.app/Contents/MacOS to BridgeCommand.app/Contents/Resources
    exeFolderPath.append("/../../../../Resources");
    //change to this path now, so ini file is read
    chdir(exeFolderPath.c_str());
    //Note, we use this again after the createDevice call
#endif

//User read/write location - look in here first and the exe folder second for files
    std::string userFolder = Utilities::getUserDir();

    std::string iniFilename = "map.ini";
    //Use local ini file if it exists
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }

    int fontSize = 12;
    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale > 1) {
        fontSize = (int)(fontSize * fontScale + 0.5);
    }
    else {
        fontScale = 1.0;
    }

    irr::u32 graphicsWidth = IniFile::iniFileTou32(iniFilename, "graphics_width");
    irr::u32 graphicsHeight = IniFile::iniFileTou32(iniFilename, "graphics_height");
    irr::u32 graphicsDepth = IniFile::iniFileTou32(iniFilename, "graphics_depth");
    bool fullScreen = (IniFile::iniFileTou32(iniFilename, "graphics_mode") == 1); //1 for full screen

    irr::core::dimension2d<irr::u32> deskres;
#ifdef _WIN32
    // Get the resolution (of the primary screen). Will be scaled as DPI unaware on Windows.
    deskres.Width = GetSystemMetrics(SM_CXSCREEN);
    deskres.Height = GetSystemMetrics(SM_CYSCREEN);
#else
    // For other OSs, use Irrlicht's resolution call
    irr::IrrlichtDevice* nulldevice = irr::createDevice(irr::video::EDT_NULL);
    deskres = nulldevice->getVideoModeList()->getDesktopResolution();
    nulldevice->drop();
#endif
    if (graphicsWidth == 0) {
        graphicsWidth = 1200 * fontScale;
        if (deskres.Width > 0 && graphicsWidth > deskres.Width * 0.90) { //(0 x 0 when the desktop size cannot be found)
            graphicsWidth = deskres.Width * 0.90;
        }
    }
    if (graphicsHeight == 0) {
        graphicsHeight = 900 * fontScale;
        if (deskres.Height > 0 && graphicsHeight > deskres.Height * 0.90) {
            graphicsHeight = deskres.Height * 0.90;
        }
    }

    irr::IrrlichtDevice* device = irr::createDevice(irr::video::EDT_OPENGL, irr::core::dimension2d<irr::u32>(graphicsWidth, graphicsHeight), graphicsDepth, fullScreen, false, false, 0);
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    //scene::ISceneManager* smgr = device->getSceneManager();

    std::string fontName = IniFile::iniFileToString(iniFilename, "font");
    std::string fontPath = "media/fonts/" + fontName + "/" + fontName + "-" + std::to_string(fontSize) + ".xml";
    irr::gui::IGUIFont* font = device->getGUIEnvironment()->getFont(fontPath.c_str());
    if (font == NULL) {
        std::cout << "Could not load font, using fallback" << std::endl;
    }
    else {
        //set skin default font
        device->getGUIEnvironment()->getSkin()->setFont(font);
    }

#ifdef __APPLE__
    //Mac OS - cd back to original dir - seems to be changed during createDevice
    irr::io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem == 0) {
        std::cout << "Could not get filesystem" << std::endl;
        exit(EXIT_FAILURE); //Could not get file system TODO: Message for user
    }
    fileSystem->changeWorkingDirectoryTo(exeFolderPath.c_str());
#endif

    //load language
    std::string modifier = IniFile::iniFileToString(iniFilename, "lang");
    if (modifier.length() == 0) {
        modifier = "en"; //Default
    }
    std::string languageFile = "languageController-";
    languageFile.append(modifier);
    languageFile.append(".txt");
    if (Utilities::pathExists(userFolder + languageFile)) {
        languageFile = userFolder + languageFile;
    }
    Lang language(languageFile);

    //Check if user scenario dir exists. If not, try to copy scenarios into the user dir.
    checkUserScenarioDir();

    //Flush old key/clicks etc, with a 0.2s pause
    device->sleep(200);
    device->clearSystemMessages();
    // KYARA METADATA UPDATE 

    while (device->run()) {

        //Create data structures to hold own ship, other ship and buoy data
        ScenarioData scenarioData;
        std::vector<PositionData> buoysData;
        //---------END


    //Classes:  Data structures created in main, and shared with controller by pointer. Controller then pushes data to the GUI

    //Create data structures to hold own ship, other ship and buoy data


    //Query which scenario or world to start with
        std::string worldName;
        std::string scenarioName;
        bool multiplayer;
        findWhatToLoad(device, &scenarioData, worldName, scenarioName, multiplayer, &language, userFolder); //worldName or scenarioName updated by reference
        //check that one of worldName and scenarioName have been set
        if (worldName.length() == 0 && scenarioName.length() == 0) {
            std::cout << "Failed to select a scenario or world model to use" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (multiplayer) {
            std::cout << "Multiplayer mode" << std::endl;
        }

        //if worldName isn't set, we need to find it from the scenario.
        if (worldName.length() == 0) {

            //Find scenario path
            std::string scenarioPath = "Scenarios/";
            if (Utilities::pathExists(userFolder + scenarioPath)) {
                scenarioPath = userFolder + scenarioPath;
            }
            scenarioPath.append(scenarioName);

            std::string environmentIniFilename = scenarioPath;
            environmentIniFilename.append("/environment.ini");
            worldName = IniFile::iniFileToString(environmentIniFilename, "Setting");

            if (worldName.length() == 0) {
                std::cout << "Could not find world model name from scenario file: " << environmentIniFilename << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        //Get a list of available boat models (own and other ships)
        std::vector<std::string> ownShipTypes;
        std::vector<std::string> otherShipTypes;

        std::string otherShipModelPath;
        std::string ownShipModelPath = "Models/Ownship/";
        if (multiplayer) {
            otherShipModelPath = "Models/Ownship/"; //If in multiplayer mode, use own ship list for both own and others
        }
        else {
            otherShipModelPath = "Models/Othership/";
        }

        getDirectoryList(device, ownShipTypes, ownShipModelPath);
        if (Utilities::pathExists(userFolder + ownShipModelPath)) {
            ownShipModelPath = userFolder + ownShipModelPath;
            getDirectoryList(device, ownShipTypes, ownShipModelPath); //Append models from userFolder
        }

        getDirectoryList(device, otherShipTypes, otherShipModelPath);
        if (Utilities::pathExists(userFolder + otherShipModelPath)) {
            otherShipModelPath = userFolder + otherShipModelPath;
            getDirectoryList(device, otherShipTypes, otherShipModelPath); //Append models from userFolder
        }

        //GUI class
        GUIMain guiMain(device, &language, ownShipTypes, otherShipTypes, multiplayer);
        guiMain.setWorldName(worldName);
        //kyara update to display pc clock 
        if (scenarioData.dataPopulated == false) {
            // Get current PC system time
            std::time_t t = std::time(nullptr);
            std::tm* now = std::localtime(&t);

            // Initialise defaults for new scenario
            scenarioData.startTime = (now->tm_hour + (now->tm_min / 60.0f)) * SECONDS_IN_HOUR;
            scenarioData.sunRise = 6;
            scenarioData.sunSet = 18;

            // Zero out all Météo values
            // Zero out all Météo values
            scenarioData.weather = 1.5;             // Updated default
            scenarioData.windDirection = 0.0;
            scenarioData.windSpeed = 0.0; // Nm/h
            scenarioData.visibilityRange = 10.0;    // Updated default
            scenarioData.rainIntensity = 0.0;

            // Set date from PC clock (tm_year is years since 1900, tm_mon is 0-11)
            scenarioData.startDay = now->tm_mday;
            scenarioData.startMonth = now->tm_mon + 1;
            scenarioData.startYear = now->tm_year + 1900;

            scenarioData.description = "Scenario description";

            scenarioData.multiplayerName = false;
            scenarioData.willOverwrite = false;
            scenarioData.scenarioName = "New Scenario";

            //Change default scenario name if in multiplayer mode
            if (multiplayer) {
                scenarioData.scenarioName.append("_mp");
            }

            //Make default name the first in the list, in case it isn't set by an update later
            if (ownShipTypes.size() > 0) {
                scenarioData.ownShipData.ownShipName = ownShipTypes.at(0);
            }
            if (otherShipTypes.size() > 0) {
                for (int i = 0; i < scenarioData.otherShipsData.size(); i++)
                    scenarioData.otherShipsData.at(i).shipName = otherShipTypes.at(0);
            }
        }

        //Main model
        ControllerModel controller(device, &language, &guiMain, worldName, &scenarioData, &buoysData);

        if (scenarioData.dataPopulated == false) {
            //If an existing scenario, load data into these structures
            if (scenarioName.length() != 0) {
                //Find scenario path
                std::string scenarioPath = "Scenarios/";
                if (Utilities::pathExists(userFolder + scenarioPath)) {
                    scenarioPath = userFolder + scenarioPath;
                }
                scenarioPath.append(scenarioName);

                //Need to read in ownship.ini, othership.ini, environment.ini
                std::string environmentIniFilename = scenarioPath;
                environmentIniFilename.append("/environment.ini");

                std::string ownShipIniFilename = scenarioPath;
                ownShipIniFilename.append("/ownship.ini");

                std::string otherShipIniFilename = scenarioPath;
                otherShipIniFilename.append("/othership.ini");

                std::string descriptionFilename = scenarioPath;
                descriptionFilename.append("/description.ini");

                //Load general information
                scenarioData.startTime = SECONDS_IN_HOUR * IniFile::iniFileTof32(environmentIniFilename, "StartTime"); //Time since start of day
                scenarioData.startDay = IniFile::iniFileTou32(environmentIniFilename, "StartDay");
                scenarioData.startMonth = IniFile::iniFileTou32(environmentIniFilename, "StartMonth");
                scenarioData.startYear = IniFile::iniFileTou32(environmentIniFilename, "StartYear");
                scenarioData.sunRise = IniFile::iniFileTof32(environmentIniFilename, "SunRise");
                scenarioData.sunSet = IniFile::iniFileTof32(environmentIniFilename, "SunSet");
                scenarioData.weather = IniFile::iniFileTof32(environmentIniFilename, "Weather");
                scenarioData.visibilityRange = IniFile::iniFileTof32(environmentIniFilename, "VisibilityRange");
                scenarioData.rainIntensity = IniFile::iniFileTof32(environmentIniFilename, "Rain");
                scenarioData.scenarioName = scenarioName;
                //defaults
                if (scenarioData.sunRise == 0.0) { scenarioData.sunRise = 6; }
                if (scenarioData.sunSet == 0.0) { scenarioData.sunSet = 18; }

                // Load wind information
                scenarioData.windDirection = IniFile::iniFileTof32(environmentIniFilename, "WindDirection");
                scenarioData.windSpeed = IniFile::iniFileTof32(environmentIniFilename, "WindSpeed");

                //Load own ship information
                scenarioData.ownShipData.initialX = controller.longToX(IniFile::iniFileTof32(ownShipIniFilename, "InitialLong"));
                scenarioData.ownShipData.initialZ = controller.latToZ(IniFile::iniFileTof32(ownShipIniFilename, "InitialLat"));
                scenarioData.ownShipData.initialBearing = IniFile::iniFileTof32(ownShipIniFilename, "InitialBearing");
                scenarioData.ownShipData.ownShipName = IniFile::iniFileToString(ownShipIniFilename, "ShipName");
                scenarioData.ownShipData.initialSpeed = IniFile::iniFileTof32(ownShipIniFilename, "InitialSpeed");

                //Load other ship information
                int numberOfOtherShips = IniFile::iniFileTou32(otherShipIniFilename, "Number");
                for (irr::u32 i = 1; i <= numberOfOtherShips; i++) {

                    //Temporary structure to load data
                    OtherShipData thisShip;

                    //Get initial position update kyara MMSI auto
                    thisShip.initialX = controller.longToX(IniFile::iniFileTof32(otherShipIniFilename, IniFile::enumerate1("InitLong", i)));
                    thisShip.initialZ = controller.latToZ(IniFile::iniFileTof32(otherShipIniFilename, IniFile::enumerate1("InitLat", i)));
                    thisShip.shipName = IniFile::iniFileToString(otherShipIniFilename, IniFile::enumerate1("Type", i));
                    thisShip.mmsi = IniFile::iniFileTou32(otherShipIniFilename, IniFile::enumerate1("mmsi", i));

                    // --- AUTO-ASSIGN MMSI IF MISSING OR ZERO (GLOBALLY SCANNED) ---
                    irr::u32 highestMmsi = 242000100;
                    for (size_t j = 0; j < scenarioData.otherShipsData.size(); j++) {
                        if (scenarioData.otherShipsData[j].mmsi > highestMmsi) {
                            highestMmsi = scenarioData.otherShipsData[j].mmsi;
                        }
                    }

                    if (thisShip.mmsi == 0) {
                        thisShip.mmsi = highestMmsi + 1;
                    }
                    // -------------------------------------------

                    if (IniFile::iniFileTou32(otherShipIniFilename, IniFile::enumerate1("Drifting", i)) == 1) {

                        thisShip.drifting = true;
                    }
                    else {
                        thisShip.drifting = false;
                    }

                    int numberOfLegs = IniFile::iniFileTof32(otherShipIniFilename, IniFile::enumerate1("Legs", i));

                    irr::f32 legStartTime = scenarioData.startTime; //Legs start at the start of the scenario
                    for (irr::u32 currentLegNo = 1; currentLegNo <= numberOfLegs; currentLegNo++) {
                        //go through each leg (if any), and load
                        LegData currentLeg;
                        currentLeg.bearing = IniFile::iniFileTof32(otherShipIniFilename, IniFile::enumerate2("Bearing", i, currentLegNo));
                        currentLeg.speed = IniFile::iniFileTof32(otherShipIniFilename, IniFile::enumerate2("Speed", i, currentLegNo));
                        currentLeg.startTime = legStartTime;

                        //Use distance to calculate startTime of next leg, and stored for later reference.
                        irr::f32 distance = IniFile::iniFileTof32(otherShipIniFilename, IniFile::enumerate2("Distance", i, currentLegNo));
                        currentLeg.distance = distance;

                        //Add the leg to the array
                        thisShip.legs.push_back(currentLeg);

                        //find the start time for the next leg
                        legStartTime = legStartTime + SECONDS_IN_HOUR * (distance / fabs(currentLeg.speed)); // nm/kts -> hours, so convert to seconds
                    }
                    //add a final 'stop' leg, which the ship will remain on after it has passed the other legs.

                    LegData stopLeg;
                    stopLeg.bearing = 0;
                    stopLeg.speed = 0;
                    stopLeg.distance = 0;
                    stopLeg.startTime = legStartTime;
                    thisShip.legs.push_back(stopLeg);

                    //Add to array.
                    scenarioData.otherShipsData.push_back(thisShip);


                }

                //Load description information
                std::ifstream descriptionStream(descriptionFilename.c_str());
                //Set UTF-8 on Linux/OSX etc
#ifndef _WIN32
                try {
#  ifdef __APPLE__
                    char* thisLocale = setlocale(LC_ALL, "");
                    if (thisLocale) {
                        descriptionStream.imbue(std::locale(thisLocale));
                    }
#  else
                    descriptionStream.imbue(std::locale("en_US.UTF8"));
#  endif
                }
                catch (const std::runtime_error& runtimeError) {
                    descriptionStream.imbue(std::locale(""));
                }
#endif

                std::string descriptionLines = "";
                if (descriptionStream.is_open()) {
                    std::string descriptionLine;
                    while (std::getline(descriptionStream, descriptionLine))
                    {
                        descriptionLines.append(descriptionLine);
                        descriptionLines.append("\n");
                    }
                    descriptionStream.close();
                }
                scenarioData.description = descriptionLines;

            } else {
                //New scenario: start the own ship in the middle of the chart
                scenarioData.ownShipData.initialX = controller.chartWidth() / 2;
                scenarioData.ownShipData.initialZ = controller.chartHeight() / 2;
            }
        }
        else {
            // Populate scenario editor specific data here

            // TODO: StartTime as used in scenario editor is in seconds, but in hours elsewhere that scenarioDataStructure is used, convert here
            scenarioData.startTime = scenarioData.startTime * SECONDS_IN_HOUR;

            // Own ship irr::f32 initialX, initialZ;
            scenarioData.ownShipData.initialX = controller.longToX(scenarioData.ownShipData.initialLong);
            scenarioData.ownShipData.initialZ = controller.latToZ(scenarioData.ownShipData.initialLat);

            // Other ship irr::f32 initialX, initialZ, leg stop times, and add final 'stop leg'
            for (int i = 0; i < scenarioData.otherShipsData.size(); i++) {

                // --- AUTO-ASSIGN MMSI FOR EDITOR SCENARIOS ---
                if (scenarioData.otherShipsData.at(i).mmsi == 0) {
                    scenarioData.otherShipsData.at(i).mmsi = 242000100 + i;
                }
                // ---------------------------------------------

                // Position
                scenarioData.otherShipsData.at(i).initialX = controller.longToX(scenarioData.otherShipsData.at(i).initialLong);
                scenarioData.otherShipsData.at(i).initialZ = controller.latToZ(scenarioData.otherShipsData.at(i).initialLat);

                // Legs
                irr::f32 legStartTime = scenarioData.startTime; // Legs start at the start of the scenario
                for (int thisLeg = 0; thisLeg < scenarioData.otherShipsData.at(i).legs.size(); thisLeg++) {
                    scenarioData.otherShipsData.at(i).legs.at(thisLeg).startTime = legStartTime;
                    irr::f32 thisLegDistance = scenarioData.otherShipsData.at(i).legs.at(thisLeg).distance;
                    irr::f32 thisLegSpeed = scenarioData.otherShipsData.at(i).legs.at(thisLeg).speed;
                    //Update legStart time for start of next leg:
                    legStartTime += SECONDS_IN_HOUR * (thisLegDistance / fabs(thisLegSpeed)); // nm/kts -> hours, so convert to seconds
                }
                //add a final 'stop' leg, which the ship will remain on after it has passed the other legs.
                LegData stopLeg;
                stopLeg.bearing = 0;
                stopLeg.speed = 0;
                stopLeg.distance = 0;
                stopLeg.startTime = legStartTime;
                scenarioData.otherShipsData.at(i).legs.push_back(stopLeg);
            }
        }

        //Load buoy data
        //construct path to world model
        std::string worldPath = "World/";
        worldPath.append(worldName);
        //Check if this world model exists in the user dir.
        if (Utilities::pathExists(userFolder + worldPath)) {
            worldPath = userFolder + worldPath;
        }
        std::string scenarioBuoyFilename = worldPath;
        scenarioBuoyFilename.append("/buoy.ini");
        //Find number of buoys
        irr::u32 numberOfBuoys;
        numberOfBuoys = IniFile::iniFileTou32(scenarioBuoyFilename, "Number");
        for (irr::u32 currentBuoy = 1; currentBuoy <= numberOfBuoys; currentBuoy++) {

            PositionData thisBuoy;
            //Get buoy position
            thisBuoy.X = controller.longToX(IniFile::iniFileTof32(scenarioBuoyFilename, IniFile::enumerate1("Long", currentBuoy)));
            thisBuoy.Z = controller.latToZ(IniFile::iniFileTof32(scenarioBuoyFilename, IniFile::enumerate1("Lat", currentBuoy)));
            buoysData.push_back(thisBuoy);
        }

        //Check if pre-set scenario name will cause an overwrite when saved
        controller.checkName();

        //create event receiver, linked to model
        EventReceiver receiver(device, &controller, &guiMain/*, &network*/);
        device->setEventReceiver(&receiver);

        while (device->run()) {
            driver->beginScene(true, true, irr::video::SColor(255, 12, 24, 42));
            //network.update(time, ownShipData, otherShipsData, buoysData);
            controller.update();
            driver->endScene();

            // --- BREAK INNER LOOP IF BACK BUTTON CLICKED ---
            if (guiMain.wantsToReturnToMenu()) {
                break;
            }
        }

        // --- CLEANUP AND RESTART ---
        if (device->run() && guiMain.wantsToReturnToMenu()) {
            device->getGUIEnvironment()->clear();
            device->getSceneManager()->clear();
            device->setEventReceiver(0);
            continue; // Safely restart outer loop with fresh memory
        }
        else {
            break; // User closed the application window natively
        }

    } // --- CLOSE OUTER LOOP ADDED IN STEP A ---

    return(0);
}