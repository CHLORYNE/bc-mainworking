//Common launcher program
//This just launches Bridge Command or
//Map Controller executable depending
//on which button the user presses

//TODO:
//Description text
//Copy bc5.ini into user dir if needed
//(Later - drop downs for _OPTION)

#include "irrlicht.h"
#include <iostream>
#include <fstream>
#include "../Lang.hpp"
#include "../IniFile.hpp"
#include "../Utilities.hpp"
#include "../AdminLock.hpp"
#include "../UiTheme.hpp"
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // For GetSystemMetrics
#include <direct.h> //for windows _mkdir
#else
#include <sys/stat.h>
#endif // _WIN32

//Mac OS:
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#endif //__APPLE__

#ifdef _MSC_VER
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#ifdef __linux__
    #include <unistd.h>
#endif

// Irrlicht Namespaces
//using namespace irr;

const irr::s32 SAVE_BUTTON = 1;

//Set up global for ini reader to have access to irrlicht logger if needed.
namespace IniFile {
    irr::ILogger* irrlichtLogger = 0;
}

//=================================================================================================
//Administrator lock: the settings are shown only once the administrator password has been given.
//The password itself is handled in AdminLock (only a salted hash is stored).
//=================================================================================================

const irr::s32 LOCK_PASSWORD_BOX = 20;
const irr::s32 LOCK_UNLOCK_BUTTON = 21;
const irr::s32 LOCK_QUIT_BUTTON = 22;
const irr::s32 PASSWORD_BUTTON = 23;     //settings screen: change the password
const irr::s32 PWD_NEW_BOX = 24;
const irr::s32 PWD_CONFIRM_BOX = 25;
const irr::s32 PWD_SAVE_BUTTON = 26;
const irr::s32 PWD_CANCEL_BUTTON = 27;

struct LockTexts {
    std::wstring title, intro, label, unlock, quit, wrong, wait, footer;
    std::wstring changeTitle, changeIntro, newLabel, confirmLabel, save, cancel, tooShort, noMatch, saved, notSaved, changeButton;
};

LockTexts lockTexts(bool french, const std::string& iniFile)
{
    LockTexts t;
    //What these settings are for, in the sentence of the lock screen.
    std::wstring what = french ? L"du simulateur" : L"simulator";
    if (iniFile == "map.ini") { what = french ? L"de la carte (poste instructeur)" : L"map (instructor station)"; }
    if (iniFile == "mph.ini") { what = french ? L"du hub multijoueur" : L"multiplayer hub"; }
    if (iniFile == "repeater.ini") { what = french ? L"du r\u00E9p\u00E9teur radar" : L"radar repeater"; }
    if (french) {
        t.title = L"Param\u00E8tres prot\u00E9g\u00E9s";
        t.intro = L"Les param\u00E8tres " + what + L" sont r\u00E9serv\u00E9s aux administrateurs. Saisissez le mot de passe pour continuer.";
        t.label = L"Mot de passe administrateur";
        t.unlock = L"D\u00E9verrouiller";
        t.quit = L"Quitter";
        t.wrong = L"Mot de passe incorrect.";
        t.wait = L"Trop d'essais. Nouvel essai possible dans ";
        t.footer = L"NAUTITECH  \u00B7  Simulateur de Navigation Maritime";
        t.changeTitle = L"Mot de passe administrateur";
        t.changeIntro = L"Le nouveau mot de passe s'appliquera aux param\u00E8tres du simulateur, de la carte et du hub multijoueur.";
        t.newLabel = L"Nouveau mot de passe";
        t.confirmLabel = L"Confirmer le mot de passe";
        t.save = L"Enregistrer";
        t.cancel = L"Annuler";
        t.tooShort = L"Au moins 6 caract\u00E8res.";
        t.noMatch = L"Les deux saisies ne correspondent pas.";
        t.saved = L"Mot de passe modifi\u00E9.";
        t.notSaved = L"Impossible d'enregistrer le mot de passe.";
        t.changeButton = L"Mot de passe...";
    }
    else {
        t.title = L"Protected settings";
        t.intro = L"The " + what + L" settings are for administrators only. Enter the password to continue.";
        t.label = L"Administrator password";
        t.unlock = L"Unlock";
        t.quit = L"Quit";
        t.wrong = L"Wrong password.";
        t.wait = L"Too many attempts. Try again in ";
        t.footer = L"NAUTITECH  \u00B7  Simulateur de Navigation Maritime";
        t.changeTitle = L"Administrator password";
        t.changeIntro = L"The new password will apply to the simulator, map and multiplayer hub settings.";
        t.newLabel = L"New password";
        t.confirmLabel = L"Confirm the password";
        t.save = L"Save";
        t.cancel = L"Cancel";
        t.tooShort = L"At least 6 characters.";
        t.noMatch = L"The two entries do not match.";
        t.saved = L"Password changed.";
        t.notSaved = L"The password could not be saved.";
        t.changeButton = L"Password...";
    }
    return t;
}

//Password field: no border or background of its own (drawn by the panel behind it), masked.
irr::gui::IGUIEditBox* addPasswordField(irr::gui::IGUIEnvironment* env, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& r, irr::s32 id)
{
    irr::gui::IGUIEditBox* box = env->addEditBox(L"", r, false, parent, id);
    box->setPasswordBox(true, L'*');
    box->setDrawBackground(false);
    box->setOverrideColor(Ui::text);
    box->setMax(64);
    return box;
}

//Rounded field behind an edit box, outlined when it has the focus.
void drawField(irr::gui::PanelBatch& b, irr::gui::IGUIEnvironment* env, irr::gui::IGUIEditBox* box, bool error)
{
    irr::core::rect<irr::f32> r = Ui::toF(box->getAbsolutePosition());
    r.UpperLeftCorner.X -= 12;
    r.LowerRightCorner.X += 12;
    const bool focus = env->hasFocus(box);
    Ui::roundRect(b, r, 8, Ui::field, Ui::field);
    Ui::roundRectOutline(b, r, 8, 1.0f, error ? Ui::danger : (focus ? Ui::accentHi : Ui::edge));
}

//Background and card of the lock screen; the field and buttons are its children.
class LockPanel : public irr::gui::IGUIElement
{
public:
    LockPanel(irr::gui::IGUIEnvironment* env, const LockTexts& texts, irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
        : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, env->getRootGUIElement(), -1,
            irr::core::rect<irr::s32>(irr::core::position2di(0, 0), env->getVideoDriver()->getScreenSize())),
        texts(texts), titleFont(titleFont), textFont(textFont), smallFont(smallFont), password(0), error(false)
    {
        //Card height from its content: title, wrapped introduction, labelled field, message, buttons.
        const irr::s32 W = AbsoluteRect.getWidth(), H = AbsoluteRect.getHeight();
        const irr::s32 cw = 480;
        const irr::s32 introH = (irr::s32)(Ui::wrap(textFont, texts.intro, (irr::f32)(cw - 80)).size() * (Ui::textHeight(textFont) + 2));
        const irr::s32 fieldH = (irr::s32)Ui::textHeight(textFont) + 18;
        const irr::s32 fieldOffset = 136 + introH + 36;
        const irr::s32 ch = fieldOffset + fieldH + 34 + 42 + 24;
        cardRect = irr::core::rect<irr::s32>((W - cw) / 2, (H - ch) / 2, (W + cw) / 2, (H + ch) / 2);
        const irr::s32 x0 = cardRect.UpperLeftCorner.X + 40, x1 = cardRect.LowerRightCorner.X - 40;
        const irr::s32 fieldY = cardRect.UpperLeftCorner.Y + fieldOffset;
        password = addPasswordField(env, this, irr::core::rect<irr::s32>(x0 + 12, fieldY, x1 - 12, fieldY + fieldH), LOCK_PASSWORD_BOX);
        const irr::s32 by = cardRect.LowerRightCorner.Y - 24 - 42;
        unlockButton = new Ui::Button(env, this, LOCK_UNLOCK_BUTTON, irr::core::rect<irr::s32>(x1 - 190, by, x1, by + 42), texts.unlock.c_str(), Ui::Button::Primary);
        quitButton = new Ui::Button(env, this, LOCK_QUIT_BUTTON, irr::core::rect<irr::s32>(x1 - 190 - 12 - 130, by, x1 - 190 - 12, by + 42), texts.quit.c_str(), Ui::Button::Secondary);
        unlockButton->setFont(textFont);
        quitButton->setFont(textFont);
        unlockButton->drop();
        quitButton->drop();
        env->setFocus(password);
    }

    irr::gui::IGUIEditBox* passwordBox() const { return password; }
    void setMessage(const std::wstring& m, bool isError) { message = m; error = isError; }
    void setBusy(bool busy) { password->setEnabled(!busy); unlockButton->setEnabled(!busy); }

    virtual void draw()
    {
        if (!IsVisible) { return; }
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();
        const irr::s32 W = AbsoluteRect.getWidth(), H = AbsoluteRect.getHeight();
        driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, W, H), irr::video::SColor(255, 16, 32, 58), irr::video::SColor(255, 16, 32, 58),
            Ui::backgroundDeep, Ui::backgroundDeep);

        const irr::core::rect<irr::f32> c = Ui::toF(cardRect);
        irr::gui::PanelBatch b;
        b.begin(driver);
        Ui::card(b, c, 18, Ui::panelTop, Ui::panelBottom);
        const irr::core::vector2df badge(c.getCenter().X, c.UpperLeftCorner.Y + 58);
        b.disc(badge, 32, irr::video::SColor(60, 64, 156, 240), irr::video::SColor(60, 64, 156, 240));
        Ui::padlock(b, irr::core::vector2df(badge.X, badge.Y + 2), 11, Ui::accentHi);
        drawField(b, Environment, password, error);
        b.flush();

        const irr::f32 x0 = c.UpperLeftCorner.X + 40, x1 = c.LowerRightCorner.X - 40;
        Ui::drawText(titleFont, texts.title, irr::core::rect<irr::f32>(c.UpperLeftCorner.X, c.UpperLeftCorner.Y + 100, c.LowerRightCorner.X, c.UpperLeftCorner.Y + 128), Ui::text, Ui::Centre);
        const std::vector<std::wstring> intro = Ui::wrap(textFont, texts.intro, x1 - x0);
        irr::f32 y = c.UpperLeftCorner.Y + 136;
        for (size_t i = 0; i < intro.size(); i++) {
            Ui::drawText(textFont, intro[i], irr::core::rect<irr::f32>(x0, y, x1, y + Ui::textHeight(textFont) + 2), Ui::textDim, Ui::Centre);
            y += Ui::textHeight(textFont) + 2;
        }
        const irr::core::rect<irr::s32> field = password->getAbsolutePosition();
        Ui::drawText(smallFont, texts.label, irr::core::rect<irr::f32>(x0, (irr::f32)field.UpperLeftCorner.Y - 24, x1, (irr::f32)field.UpperLeftCorner.Y - 6), Ui::textDim);
        Ui::drawText(smallFont, message, irr::core::rect<irr::f32>(x0, (irr::f32)field.LowerRightCorner.Y + 6, x1, (irr::f32)field.LowerRightCorner.Y + 26),
            error ? irr::video::SColor(255, 255, 128, 128) : Ui::success);
        Ui::drawText(smallFont, texts.footer, irr::core::rect<irr::f32>(0, (irr::f32)H - 34, (irr::f32)W, (irr::f32)H - 10), irr::video::SColor(150, 200, 214, 230), Ui::Centre);

        IGUIElement::draw(); //field text and buttons
    }

private:
    LockTexts texts;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    irr::core::rect<irr::s32> cardRect;
    irr::gui::IGUIEditBox* password;
    Ui::Button* unlockButton;
    Ui::Button* quitButton;
    std::wstring message;
    bool error;
};

class LockReceiver : public irr::IEventReceiver
{
public:
    LockReceiver() : attempt(false), quit(false) {}
    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (event.EventType == irr::EET_GUI_EVENT) {
            const irr::s32 id = event.GUIEvent.Caller ? event.GUIEvent.Caller->getID() : -1;
            if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED && id == LOCK_UNLOCK_BUTTON) { attempt = true; }
            if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED && id == LOCK_QUIT_BUTTON) { quit = true; }
            if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER && id == LOCK_PASSWORD_BOX) { attempt = true; }
        }
        if (event.EventType == irr::EET_KEY_INPUT_EVENT && event.KeyInput.Key == irr::KEY_ESCAPE && !event.KeyInput.PressedDown) {
            quit = true;
        }
        return false;
    }
    bool attempt, quit;
};

//Lock screen. True once the right password has been given, false if the user quits.
bool unlockSettings(irr::IrrlichtDevice* device, const LockTexts& texts, irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
{
    irr::gui::IGUIEnvironment* env = device->getGUIEnvironment();
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    LockPanel* panel = new LockPanel(env, texts, titleFont, textFont, smallFont);
    LockReceiver receiver;
    device->setEventReceiver(&receiver);

    bool unlocked = false;
    int failures = 0;
    irr::u32 blockedUntil = 0;
    while (device->run() && !unlocked && !receiver.quit) {
        const irr::u32 now = device->getTimer()->getRealTime();
        if (blockedUntil > 0) {
            if (now >= blockedUntil) {
                blockedUntil = 0;
                panel->setBusy(false);
                panel->setMessage(L"", false);
                env->setFocus(panel->passwordBox());
            }
            else if (failures >= 3) {
                panel->setMessage(texts.wait + std::to_wstring((blockedUntil - now + 999) / 1000) + L" s", true);
            }
            receiver.attempt = false;
        }
        if (receiver.attempt && blockedUntil == 0) {
            receiver.attempt = false;
            if (AdminLock::check(panel->passwordBox()->getText())) {
                unlocked = true;
                break;
            }
            //Wrong: a short pause after each try, growing after the third.
            failures++;
            panel->passwordBox()->setText(L"");
            panel->setMessage(texts.wrong, true);
            panel->setBusy(true);
            blockedUntil = now + (failures >= 3 ? 5000u * (irr::u32)(failures - 2) : 900u);
        }
        driver->beginScene(true, false, Ui::background);
        env->drawAll();
        driver->endScene();
        device->sleep(15);
    }
    device->setEventReceiver(0);
    panel->remove();
    panel->drop();
    return unlocked;
}

//Change-password dialog shown over the settings (opened by the "Password..." button).
class PasswordDialog : public irr::gui::IGUIElement
{
public:
    PasswordDialog(irr::gui::IGUIEnvironment* env, const LockTexts& texts, irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
        : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, env->getRootGUIElement(), -1,
            irr::core::rect<irr::s32>(irr::core::position2di(0, 0), env->getVideoDriver()->getScreenSize())),
        texts(texts), titleFont(titleFont), textFont(textFont), smallFont(smallFont), error(false)
    {
        const irr::s32 W = AbsoluteRect.getWidth(), H = AbsoluteRect.getHeight();
        const irr::s32 cw = 500, ch = 400;
        cardRect = irr::core::rect<irr::s32>((W - cw) / 2, (H - ch) / 2, (W + cw) / 2, (H + ch) / 2);
        const irr::s32 x0 = cardRect.UpperLeftCorner.X + 40, x1 = cardRect.LowerRightCorner.X - 40;
        const irr::s32 fieldH = (irr::s32)Ui::textHeight(textFont) + 18;
        const irr::s32 y1 = cardRect.UpperLeftCorner.Y + 150, y2 = y1 + fieldH + 46;
        newBox = addPasswordField(env, this, irr::core::rect<irr::s32>(x0 + 12, y1, x1 - 12, y1 + fieldH), PWD_NEW_BOX);
        confirmBox = addPasswordField(env, this, irr::core::rect<irr::s32>(x0 + 12, y2, x1 - 12, y2 + fieldH), PWD_CONFIRM_BOX);
        const irr::s32 by = cardRect.LowerRightCorner.Y - 64;
        Ui::Button* save = new Ui::Button(env, this, PWD_SAVE_BUTTON, irr::core::rect<irr::s32>(x1 - 170, by, x1, by + 42), texts.save.c_str(), Ui::Button::Primary);
        Ui::Button* cancel = new Ui::Button(env, this, PWD_CANCEL_BUTTON, irr::core::rect<irr::s32>(x1 - 170 - 12 - 130, by, x1 - 170 - 12, by + 42), texts.cancel.c_str(), Ui::Button::Secondary);
        save->setFont(textFont);
        cancel->setFont(textFont);
        save->drop();
        cancel->drop();
        setVisible(false);
    }

    void open()
    {
        newBox->setText(L"");
        confirmBox->setText(L"");
        message.clear();
        error = false;
        setVisible(true);
        Parent->bringToFront(this);
        Environment->setFocus(newBox);
    }

    //Checks the entries and saves; true when done (the dialog can close).
    bool trySave()
    {
        const std::wstring a = newBox->getText(), b = confirmBox->getText();
        error = true;
        if (a.size() < 6) { message = texts.tooShort; Environment->setFocus(newBox); return false; }
        if (a != b) { message = texts.noMatch; confirmBox->setText(L""); Environment->setFocus(confirmBox); return false; }
        if (!AdminLock::change(a)) { message = texts.notSaved; return false; }
        error = false;
        message = texts.saved;
        return true;
    }

    irr::gui::IGUIEditBox* confirmField() const { return confirmBox; }
    const std::wstring& savedMessage() const { return texts.saved; }

    virtual void draw()
    {
        if (!IsVisible) { return; }
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();
        driver->draw2DRectangle(irr::video::SColor(170, 2, 8, 18), AbsoluteRect);
        const irr::core::rect<irr::f32> c = Ui::toF(cardRect);
        irr::gui::PanelBatch b;
        b.begin(driver);
        Ui::card(b, c, 18, Ui::panelTop, Ui::panelBottom);
        b.disc(irr::core::vector2df(c.UpperLeftCorner.X + 58, c.UpperLeftCorner.Y + 52), 22, irr::video::SColor(60, 64, 156, 240), irr::video::SColor(60, 64, 156, 240));
        Ui::padlock(b, irr::core::vector2df(c.UpperLeftCorner.X + 58, c.UpperLeftCorner.Y + 54), 8, Ui::accentHi);
        drawField(b, Environment, newBox, error && message == texts.tooShort);
        drawField(b, Environment, confirmBox, error && message == texts.noMatch);
        b.flush();

        const irr::f32 x0 = c.UpperLeftCorner.X + 40, x1 = c.LowerRightCorner.X - 40;
        Ui::drawText(titleFont, texts.changeTitle, irr::core::rect<irr::f32>(c.UpperLeftCorner.X + 92, c.UpperLeftCorner.Y + 38, x1, c.UpperLeftCorner.Y + 66), Ui::text);
        Ui::drawWrapped(smallFont, texts.changeIntro, irr::core::rect<irr::f32>(x0, c.UpperLeftCorner.Y + 84, x1, c.UpperLeftCorner.Y + 124), Ui::textDim);
        const irr::core::rect<irr::s32> f1 = newBox->getAbsolutePosition(), f2 = confirmBox->getAbsolutePosition();
        Ui::drawText(smallFont, texts.newLabel, irr::core::rect<irr::f32>(x0, (irr::f32)f1.UpperLeftCorner.Y - 24, x1, (irr::f32)f1.UpperLeftCorner.Y - 6), Ui::textDim);
        Ui::drawText(smallFont, texts.confirmLabel, irr::core::rect<irr::f32>(x0, (irr::f32)f2.UpperLeftCorner.Y - 24, x1, (irr::f32)f2.UpperLeftCorner.Y - 6), Ui::textDim);
        Ui::drawText(smallFont, message, irr::core::rect<irr::f32>(x0, (irr::f32)f2.LowerRightCorner.Y + 8, x1, (irr::f32)f2.LowerRightCorner.Y + 28),
            error ? irr::video::SColor(255, 255, 128, 128) : Ui::success);
        IGUIElement::draw();
    }

private:
    LockTexts texts;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    irr::core::rect<irr::s32> cardRect;
    irr::gui::IGUIEditBox* newBox;
    irr::gui::IGUIEditBox* confirmBox;
    std::wstring message;
    bool error;
};

//Structures to hold ini file contents
struct IniFileEntry {
    std::string settingName;
    std::string settingValue;
    std::string description;
    std::vector<std::string> settingOption;
};

struct IniFileTab {
    std::string tabName;
    std::vector<IniFileEntry> settings;
};

void saveFile(irr::IrrlichtDevice* device, std::string iniFilename, irr::gui::IGUITabControl* tabbedPane) {

    bool successSaving = false;
    std::ofstream file (iniFilename.c_str());
    if (file.is_open())
    {
        //For each tab, get tab name and child table
        for (int i = 0; i<tabbedPane->getTabCount(); i++) {

            //Get tab name - convert from stringw to std::string, loosing any extended character info.
            std::string sectionName(irr::core::stringc(tabbedPane->getTab(i)->getText()).c_str());
            file << "[" + sectionName + "]" << std::endl;

            //For each table, get contents, including description
            irr::core::list<irr::gui::IGUIElement*> tabChildren = tabbedPane->getTab(i)->getChildren();
            //There should just be one child, but iterate through all, and do stuff with tables
            for (irr::core::list<irr::gui::IGUIElement*>::Iterator it = tabChildren.begin(); it != tabChildren.end(); ++it) {

                if ((*it)->getType() == irr::gui::EGUIET_TABLE ) {
                    irr::gui::IGUITable* thisTable = (irr::gui::IGUITable*)(*it);
                    irr::u32 numberOfRows = thisTable->getRowCount();
                    for (int j = 0; j<numberOfRows; j++) {
                        std::string varName(irr::core::stringc(thisTable->getCellText(j,0)).c_str());
                        std::string varValue(irr::core::stringc(thisTable->getCellText(j,1)).c_str());
                        std::string desc(irr::core::stringc(thisTable->getCellText(j,2)).c_str());
                        file << varName << "=" << "\"" << varValue << "\"" << std::endl;
                        file <<varName << "_DESC=\"" << desc << "\"" << std::endl;
                    }
                }
            }



            //Collate these
            //Save (or warn if not possible)
        }
    if (file.good()) {successSaving=true;}
    file.close();
    }

    if (successSaving) {
        device->closeDevice();
    }
}

//Event receiver: This does the actual launching
class Receiver : public irr::IEventReceiver
{
public:
    Receiver(irr::IrrlichtDevice* device, irr::gui::IGUIEnvironment* environment, irr::gui::IGUITabControl* tabbedPane, std::string iniFilename)
    {
        this->environment = environment;
        this->tabbedPane = tabbedPane;
        this->iniFilename = iniFilename;
        this->device = device;
    }

    //Change-password dialog, and the line where its result is shown for a few seconds.
    void setPasswordDialog(PasswordDialog* dialog, irr::gui::IGUIStaticText* status, irr::u32* statusUntil)
    {
        passwordDialog = dialog;
        this->status = status;
        this->statusUntil = statusUntil;
    }

private:
    irr::IrrlichtDevice* device;
    irr::gui::IGUIEnvironment* environment;
    irr::core::position2di mousePos;
    irr::gui::IGUIEditBox* valueEntryBox = 0;
    irr::gui::IGUITable* selectedTable = 0; //Keep track of which table was last selected
    irr::s32 selectedRow = 0; //In the selected table, which row was last selected?
    irr::gui::IGUITabControl* tabbedPane;
    std::string iniFilename;
    PasswordDialog* passwordDialog = 0;
    irr::gui::IGUIStaticText* status = 0;
    irr::u32* statusUntil = 0;

    void savePassword()
    {
        if (passwordDialog && passwordDialog->trySave()) {
            passwordDialog->setVisible(false);
            if (status && statusUntil) {
                status->setText(passwordDialog->savedMessage().c_str());
                *statusUntil = device->getTimer()->getRealTime() + 5000;
            }
        }
    }

    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
            if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED) {
                mousePos.X = event.MouseInput.X;
                mousePos.Y = event.MouseInput.Y;
            }
        }

        if (event.EventType == irr::EET_KEY_INPUT_EVENT && event.KeyInput.Key == irr::KEY_ESCAPE && !event.KeyInput.PressedDown &&
            passwordDialog && passwordDialog->isVisible()) {
            passwordDialog->setVisible(false);
            return true;
        }

        if (event.EventType == irr::EET_GUI_EVENT) {

            //Change-password dialog
            const irr::s32 callerId = event.GUIEvent.Caller ? event.GUIEvent.Caller->getID() : -1;
            if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED && passwordDialog) {
                if (callerId == PASSWORD_BUTTON) { passwordDialog->open(); return true; }
                if (callerId == PWD_CANCEL_BUTTON) { passwordDialog->setVisible(false); return true; }
                if (callerId == PWD_SAVE_BUTTON) { savePassword(); return true; }
            }
            if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER && passwordDialog) {
                if (callerId == PWD_NEW_BOX) { environment->setFocus(passwordDialog->confirmField()); return true; }
                if (callerId == PWD_CONFIRM_BOX) { savePassword(); return true; }
            }

            if ((event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER && event.GUIEvent.Caller == valueEntryBox) || (event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST && event.GUIEvent.Caller == valueEntryBox ) ) {
                //Enter or edit box has lost focus
                if (valueEntryBox!=0) {

                    //Update the table
                    selectedTable->setCellText(selectedRow,1,irr::core::stringw(valueEntryBox->getText())/*,video::SColor (255, 255, 255, 255)*/);

                    //Remove the edit box
                    valueEntryBox->remove();
                    valueEntryBox = 0;
                }
            }

            if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED ) {
                irr::s32 id = event.GUIEvent.Caller->getID();
                if (id == SAVE_BUTTON) {

                    //Save to the ini file
                    saveFile(device,iniFilename,tabbedPane);

                }

            }

            if (event.GUIEvent.EventType == irr::gui::EGET_TABLE_SELECTED_AGAIN  ) {
                selectedTable = ((irr::gui::IGUITable*)event.GUIEvent.Caller);
                irr::s32 id = event.GUIEvent.Caller->getID();
                selectedRow = selectedTable->getSelected();
                irr::core::stringw selectedValue = irr::core::stringw(selectedTable->getCellText(selectedRow,1));

                if (valueEntryBox==0) {
                    valueEntryBox = environment->addEditBox(selectedValue.c_str(),irr::core::rect<irr::s32>(mousePos.X, mousePos.Y, mousePos.X + 100, mousePos.Y + 30 )); //FIXME: Hardcoding of size
                    environment->setFocus(valueEntryBox);
                }

            }

            //When edit box looses focus or enter pressed (selected again?), save this and use

        }
        return false;
    }
};

int findCharOccurrences(std::string inputString, std::string findStr)
{
    int occurrences = 0;
    std::string::size_type charFound = -1; //FIXME: Valid?

    while (true) {
        charFound = inputString.find(findStr,charFound+1);
        if (charFound != std::string::npos) {
            occurrences++;
        } else {
            return occurrences;
        }
    }

}

int main (int argc, char ** argv)
{

    #ifdef FOR_DEB
    chdir("/usr/share/bridgecommand");
    #endif // FOR_DEB

    //Choose the file to edit, with default of bc5.ini, change to map.ini if '-M' is used as first argument, or mph.ini if -H, or repeater.ini -f -R
    std::string iniFilename = "bc5.ini";
    if ((argc>1)&&(strcmp(argv[1],"-M")==0)) {
        iniFilename = "map.ini";
    }
    if ((argc>1)&&(strcmp(argv[1],"-H")==0)) {
        iniFilename = "mph.ini";
    }
    if ((argc>1)&&(strcmp(argv[1],"-R")==0)) {
        iniFilename = "repeater.ini";
    }

    bool autoMode = false;
    if (((argc > 1) && (strcmp(argv[1], "-auto") == 0)) || 
        ((argc > 2) && (strcmp(argv[2], "-auto") == 0))) {
        autoMode = true;
    }

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
    //change up from BridgeCommand.app/Contents/MacOS/ini.app/Contents/MacOS to BridgeCommand.app/Contents/Resources
    exeFolderPath.append("/../../../../Resources");
    //change to this path now
    chdir(exeFolderPath.c_str());
    //Note, we use this again after the createDevice call
	#endif

    //User read/write location - look in here first and the exe folder second for files
    std::string userFolder = Utilities::getUserDir();
    std::cout << "User folder is " << userFolder << std::endl;


    //Copy into userdir if not already there
    if (!Utilities::pathExists(userFolder + iniFilename)) {
        if (!Utilities::pathExists(Utilities::getUserDirBase())) {
            std::string pathToMake = Utilities::getUserDirBase();
            if (pathToMake.size() > 1) {pathToMake.erase(pathToMake.size()-1);} //Remove trailing slash
            #ifdef _WIN32
            _mkdir(pathToMake.c_str());
            #else
            mkdir(pathToMake.c_str(),0755);
            #endif // _WIN32
        }
        if (!Utilities::pathExists(Utilities::getUserDir())) {
            std::string pathToMake = Utilities::getUserDir();
            if (pathToMake.size() > 1) {pathToMake.erase(pathToMake.size()-1);} //Remove trailing slash
            #ifdef _WIN32
            _mkdir(pathToMake.c_str());
            #else
            mkdir(pathToMake.c_str(),0755);
            #endif // _WIN32
        }

        //Copy ini file from main into user dir
        std::ifstream iniFileIn (iniFilename.c_str());
        std::ofstream iniFileOut ((userFolder + iniFilename).c_str());
        if (iniFileIn.is_open()) {
            if (iniFileOut.is_open()) {
                //Copy line by line
                std::string line;
                while ( std::getline (iniFileIn,line) ) {
                    iniFileOut << line << std::endl ;
                }
                iniFileOut.close();
            }
            iniFileIn.close();
        }
    }
    //end copy

    //Store a copy of the global ini filename
    std::string globalIniFilename = iniFilename;

    //Use local ini file if it exists
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }

    //Vector to hold values read into file
    std::vector<IniFileTab> iniFileStructure;

    IniFileTab* thisSection = 0;

    //open the ini file:
    std::ifstream file (iniFilename.c_str());
    if (file.is_open())
    {
        std::string line;
        while ( std::getline (file,line) )
        {
            line = Utilities::trim(line);

            if (findCharOccurrences(line,"[") == 1 && findCharOccurrences(line,"]") == 1 ) {
                //A section heading

                //Store the previous tab if it exists, and create a new one
                if (thisSection!=0) {
                    iniFileStructure.push_back(*thisSection);
                    delete thisSection; //TODO: Check if this is appropriate
                }
                thisSection = new IniFileTab;
                thisSection->tabName=line;

            } else if (findCharOccurrences(line,"=") == 1 && findCharOccurrences(line,"_DESC") == 0 && findCharOccurrences(line,"_OPTION") == 0 ) {
                //A normal entry

                //Create a section if it doesn't exist
                if (thisSection==0) {
                    thisSection = new IniFileTab;
                    thisSection->tabName="[General]";
                }

                //Store this line
                std::vector<std::string> splitLine = Utilities::split(line,'=');
                if (splitLine.size() == 2) {
                    IniFileEntry thisEntry;
                    thisEntry.settingName = splitLine.at(0);
                    thisEntry.settingValue = Utilities::trim(splitLine.at(1),"\"");
                    //check if a '_DESC' setting is available
                    thisEntry.description = IniFile::iniFileToString(iniFilename,thisEntry.settingName+"_DESC");

                    thisSection->settings.push_back(thisEntry);
                }
            }
        }
        file.close();
        //Store the final tab if it exists
        if (thisSection!=0) {
            iniFileStructure.push_back(*thisSection);
            delete thisSection; //TODO: Check if this is appropriate
        }
    }

    //Read the global ini file. If there are any lines that don't exist in the user's ini file, copy in
    std::ifstream globalFile (globalIniFilename.c_str());
    if (globalFile.is_open())
    {
		//std::cout << "Opened global ini file " << globalIniFilename << std::endl;
        std::string line;
		std::string currentTabName = "[General]";
		while ( std::getline (globalFile,line) )
		{
			line = Utilities::trim(line);
			if (findCharOccurrences(line,"[") == 1 && findCharOccurrences(line,"]") == 1 ) {
				//A section heading
				currentTabName=line;
			} else if (findCharOccurrences(line,"=") == 1 && findCharOccurrences(line,"_DESC") == 0 && findCharOccurrences(line,"_OPTION") == 0 ) {
				//A normal entry
				std::vector<std::string> splitLine = Utilities::split(line,'=');
				if (splitLine.size() == 2) {
					IniFileEntry thisEntry;
					thisEntry.settingName = splitLine.at(0);
					thisEntry.settingValue = Utilities::trim(splitLine.at(1),"\"");
					//check if a '_DESC' setting is available
					thisEntry.description = IniFile::iniFileToString(globalIniFilename,thisEntry.settingName+"_DESC");
					//Check if this exists in the main iniFileStructure
					bool found = false;

                    //std::cout << "Entry: " << thisEntry.settingName << ":" << thisEntry.settingValue << std::endl;

					//TODO: Ignore 'joystick_map*' cases, ie force these to return found = true;

					for (int i = 0; i < iniFileStructure.size(); i++) {
						for (int j = 0; j < iniFileStructure.at(i).settings.size(); j++) {
							std::string compare1 = iniFileStructure.at(i).settings.at(j).settingName;
							std::string compare2 = thisEntry.settingName;
							Utilities::to_lower(compare1);
							Utilities::to_lower(compare2);
							//Ignore 'joystick_map*' cases, as we don't want to copy these across even if different
							if (compare2.find("joystick_map") != std::string::npos  || compare1.compare(compare2)==0 ) {
								found = true;
								break;
							}
						}
						if (found) {
							break;
						}
					}

					//If not, find the corresponding tab, or add a new tab and add there
					if (!found) {
						//Add to corresponding tab
                        int whichTab = -1;
						for (int i = 0; i < iniFileStructure.size(); i++) {
							if (currentTabName.compare(iniFileStructure.at(i).tabName) == 0) {
								whichTab = i;
							}
						}
                        if (whichTab < 0) {
                            // Tab not found, create a new one
                            IniFileTab newTab;
                            newTab.tabName = currentTabName;
                            newTab.settings.push_back(thisEntry);
                            iniFileStructure.push_back(newTab);
                        } else {
                            // Found existing tab, use this
                            iniFileStructure.at(whichTab).settings.push_back(thisEntry);
                        }
					}

				}
			}
		}
		globalFile.close();
    }

    std::string modifier = IniFile::iniFileToString(iniFilename, "lang");
    if (modifier.length()==0) {
        modifier = "en"; //Default
    }
    std::string languageFile = "languageIniEditor-";
    languageFile.append(modifier);
    languageFile.append(".txt");
    if (Utilities::pathExists(userFolder + languageFile)) {
        languageFile = userFolder + languageFile;
    }

    Lang language(languageFile);

    int fontSize = 12;
    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale > 1) {
        fontSize = (int)(fontSize * fontScale + 0.5);
    } else {
	    fontScale = 1.0;
    }
    
    irr::u32 graphicsWidth;
    irr::u32 graphicsHeight;
    irr::core::dimension2d<irr::u32> deskres;
    #ifdef _WIN32
    // Get the resolution (of the primary screen). Will be scaled as DPI unaware on Windows.
    deskres.Width=GetSystemMetrics(SM_CXSCREEN);
    deskres.Height=GetSystemMetrics(SM_CYSCREEN);
    #else
    // For other OSs, use Irrlicht's resolution call
    irr::IrrlichtDevice *nulldevice = irr::createDevice(irr::video::EDT_NULL);
	deskres = nulldevice->getVideoModeList()->getDesktopResolution();
	nulldevice->drop();
    #endif

    //(The desktop size can read as 0 x 0 when it cannot be found; then keep the default size.)
    graphicsWidth = 1200 * fontScale;
    if (deskres.Width > 0 && graphicsWidth > deskres.Width*0.90) {
        graphicsWidth = deskres.Width*0.90;
    }
    graphicsHeight = 900 * fontScale;
    if (deskres.Height > 0 && graphicsHeight > deskres.Height*0.90) {
        graphicsHeight = deskres.Height*0.90;
    }
    
    irr::u32 graphicsDepth = 32;
    bool fullScreen = false;

    irr::IrrlichtDevice* device = irr::createDevice(irr::video::EDT_OPENGL, irr::core::dimension2d<irr::u32>(graphicsWidth,graphicsHeight),graphicsDepth,fullScreen,false,false,0);
    irr::video::IVideoDriver* driver = device->getVideoDriver();

    irr::gui::IGUIEnvironment* environment = device->getGUIEnvironment();

    #ifdef __APPLE__
    //Mac OS - cd back to original dir - seems to be changed during createDevice
    irr::io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem==0) {
        exit(EXIT_FAILURE); //Could not get file system TODO: Message for user
        std::cout << "Could not get filesystem" << std::endl;
    }
    fileSystem->changeWorkingDirectoryTo(exeFolderPath.c_str());
    #endif
    //icon - kyara 
    device->setWindowCaption(L"Simulateur de Navigation Maritime");

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
    // ------------------------------------------------------

    std::string fontName = IniFile::iniFileToString(iniFilename, "font");
    std::string fontPath = "media/fonts/" + fontName + "/" + fontName + "-" + std::to_string(fontSize) + ".xml";
    irr::gui::IGUIFont *font = environment->getFont(fontPath.c_str());
    if (font == NULL) {
        std::cout << "Could not load font, using fallback" << std::endl;
    } else {
        //set skin default font
        environment->getSkin()->setFont(font);
    }

    //Dark navy look, as the launcher.
    irr::gui::IGUISkin* skin = environment->getSkin();
    Ui::applySkin(skin);

    //Larger fonts for the lock screen and the password dialog.
    const std::string uiFontName = fontName.empty() ? std::string("noto-sans") : fontName;
    auto uiFont = [&](int size) -> irr::gui::IGUIFont* {
        size = (int)(size * fontScale + 0.5f);
        if (size > 36) { size = 36; }
        irr::gui::IGUIFont* f = environment->getFont(("media/fonts/" + uiFontName + "/" + uiFontName + "-" + std::to_string(size) + ".xml").c_str());
        return f ? f : environment->getSkin()->getFont();
    };
    irr::gui::IGUIFont* titleFont = uiFont(20);
    irr::gui::IGUIFont* textFont = uiFont(15);
    irr::gui::IGUIFont* smallFont = uiFont(13);
    const LockTexts lockText = lockTexts(modifier == "fr", globalIniFilename);

    //Administrator password first (not in -auto mode, which only brings the user's file up to date).
    if (!autoMode && !unlockSettings(device, lockText, titleFont, textFont, smallFont)) {
        device->drop();
        return 0;
    }

    irr::core::dimension2d<irr::u32> screenSize = driver->getScreenSize();
    irr::u32 width = screenSize.Width;
    irr::u32 height = screenSize.Height;
    //Do set-up here

    int pad = 10;

    //Save, change the administrator password, and a line for the result of the latter.
    const irr::s32 buttonH = (irr::s32)(40 * fontScale);
    const irr::s32 saveW = (irr::s32)(240 * fontScale), passwordW = (irr::s32)(170 * fontScale);
    Ui::Button* saveButton = new Ui::Button(environment, 0, SAVE_BUTTON,
        irr::core::rect<irr::s32>(pad, height - pad - buttonH, pad + saveW, height - pad), language.translate("save").c_str(), Ui::Button::Primary);
    saveButton->drop();
    Ui::Button* passwordButton = new Ui::Button(environment, 0, PASSWORD_BUTTON,
        irr::core::rect<irr::s32>(pad + saveW + 12, height - pad - buttonH, pad + saveW + 12 + passwordW, height - pad), lockText.changeButton.c_str(), Ui::Button::Secondary);
    passwordButton->drop();
    irr::gui::IGUIStaticText* statusText = environment->addStaticText(L"",
        irr::core::rect<irr::s32>(pad + saveW + passwordW + 40, height - pad - buttonH, width - pad, height - pad));
    statusText->setOverrideColor(Ui::success);
    statusText->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);

    irr::gui::IGUITabControl* tabbedPane = environment->addTabControl( irr::core::rect<irr::s32>(pad,pad,width-pad,height-(50*fontScale)-pad),0,true);

    //Add tab entry here
    for(int i = 0; i<iniFileStructure.size(); i++) {
        std::string tabName = iniFileStructure.at(i).tabName;
        irr::gui::IGUITab* thisTab = tabbedPane->addTab((irr::core::stringw(tabName.substr(1,tabName.length()-2).c_str())).c_str());

        //Add tab contents in a table
        irr::gui::IGUITable* thisTable = environment->addTable(irr::core::rect<irr::s32>(pad,pad,width-(3*pad),height-(50*fontScale)-2*(3*pad)),thisTab);

        thisTable->addColumn(language.translate("name").c_str());
        thisTable->addColumn(language.translate("value").c_str());
        thisTable->addColumn(language.translate("description").c_str());
        thisTable->setColumnWidth(0,300*fontScale);
        thisTable->setColumnWidth(2,2*width);
        thisTable->setToolTipText(language.translate("doubleClick").c_str());

        for (int j = 0; j<iniFileStructure.at(i).settings.size(); j++) {
            thisTable->addRow(j);
            thisTable->setCellText(j,0,irr::core::stringw(iniFileStructure.at(i).settings.at(j).settingName.c_str()).c_str()/*,video::SColor (255, 255, 255, 255)*/ );
            thisTable->setCellText(j,1,irr::core::stringw(iniFileStructure.at(i).settings.at(j).settingValue.c_str()).c_str()/*,video::SColor (255, 255, 255, 255)*/ );
            thisTable->setCellText(j,2,irr::core::stringw(iniFileStructure.at(i).settings.at(j).description.c_str()).c_str()/*,video::SColor (255, 255, 255, 255)*/ );
        }

    }

    Receiver receiver(device, environment, tabbedPane, iniFilename);
    device->setEventReceiver(&receiver);

    //Change-password dialog, over everything else.
    PasswordDialog* passwordDialog = new PasswordDialog(environment, lockText, titleFont, textFont, smallFont);
    passwordDialog->drop();
    irr::u32 statusUntil = 0;
    receiver.setPasswordDialog(passwordDialog, statusText, &statusUntil);

    if (autoMode) {
        // Automatically save and close. This mode is used to ensure we have user settings file updated with any new global ini settings
        saveFile(device, iniFilename, tabbedPane);
    }
    else {
        while (device->run()) {
            if (statusUntil && device->getTimer()->getRealTime() > statusUntil) {
                statusText->setText(L"");
                statusUntil = 0;
            }
            driver->beginScene(true, true, Ui::background);
            device->getGUIEnvironment()->drawAll();
            driver->endScene();
        }
    }
    return(0);
}
