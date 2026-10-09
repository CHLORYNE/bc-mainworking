#ifdef WITH_PROFILING
#include "iprof.hpp"
#else
#define IPROF(a) //intentionally empty placeholder
#endif

// Include the Irrlicht header
#include "irrlicht.h"
#include <chrono>
#include <thread>
#include "simple_license.h"
#include "DefaultEventReceiver.hpp"
#include "GUIMain.hpp"
#include "ScenarioDataStructure.hpp"
#include "SimulationModel.hpp"
#include "ScenarioChoice.hpp"
#include "CentreScreen.hpp"
#include "MyEventReceiver.hpp"
#include "LoadingScreen.hpp" //KYARA CHARGEMENT
#include "Network.hpp"
#include "IniFile.hpp"
#include "Constants.hpp"
#include "Lang.hpp"
#include "NMEA.hpp"
#include "Sound.hpp"
#include "Utilities.hpp"
#include "OperatingModeEnum.hpp"
#include <chrono>
#include <cstdlib> //For rand(), srand()
#include <cstdio> //KYARA CHARGEMENT: snprintf
#include <vector>
#include <sstream>
#include <fstream> //To save to log
#include <asio.hpp> //To display hostname

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // For GetSystemMetrics
#include <direct.h> //for windows _mkdir
#include <shellapi.h>
#include <dbghelp.h> //crash report (MiniDumpWriteDump, loaded at run time)
#else
#include <sys/stat.h>
#endif // _WIN32

#include "VRInterface.hpp"
#include "ScreenChooser.hpp"

#include "profile.hpp"

//Mac OS:
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <sys/prctl.h> //the radar station ends with the simulator
#include <signal.h>
#include <unistd.h>
#endif


using namespace irr;

// No console window. What the simulator prints goes to log.txt in the user folder instead;
// bc5.ini debug_console=1 opens a console window for it when debugging.
#ifdef _MSC_VER
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#ifdef _WIN32
//From https://superkogito.github.io/blog/LoopMonitorsDetailsInCplusplus.html
// Structure that includes all screen hanldes and rectangles
struct cMonitorsVec
{
    std::vector<int>       iMonitors;
    std::vector<HMONITOR>  hMonitors;
    std::vector<HDC>       hdcMonitors;
    std::vector<RECT>      rcMonitors;

    static BOOL CALLBACK MonitorEnum(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
    {
        cMonitorsVec* pThis = reinterpret_cast<cMonitorsVec*>(pData);

        pThis->hMonitors.push_back(hMon);
        pThis->hdcMonitors.push_back(hdc);
        pThis->rcMonitors.push_back(*lprcMonitor);
        pThis->iMonitors.push_back(pThis->hdcMonitors.size());
        return TRUE;
    }

    cMonitorsVec()
    {
        EnumDisplayMonitors(0, 0, MonitorEnum, (LPARAM)this);
    }
};

//Unexpected stop: what happened and where (module and offset, then the calls that led there) goes to
//the log, a minidump (crash.dmp, which Visual Studio opens with the program's .pdb) is written next
//to it, and the user is told where both are.
namespace CrashReport {
    std::string logPath;  //where stderr goes, if redirected
    std::string dumpPath;

    void describeAddress(const void* address, char* out, size_t size)
    {
        HMODULE module = 0;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module) && module) {
            char path[MAX_PATH] = "";
            GetModuleFileNameA(module, path, MAX_PATH);
            const char* name = strrchr(path, '\\');
            snprintf(out, size, "%s+0x%llx", name ? name + 1 : path, (unsigned long long)((const char*)address - (const char*)module));
        }
        else {
            snprintf(out, size, "0x%llx", (unsigned long long)(size_t)address);
        }
    }

    //The calls that led to the crash, unwound from the crash's own registers. (No C++ objects in here,
    //so that MSVC can guard it: a damaged stack must not stop the rest of the report.)
    void writeCallStack(const CONTEXT* crashContext)
    {
#ifdef _MSC_VER
        __try {
#endif
#if defined(_M_X64) || defined(__x86_64__)
            CONTEXT context = *crashContext;
            for (int i = 0; i < 40 && context.Rip != 0; i++) {
                char where[MAX_PATH + 32];
                describeAddress((const void*)context.Rip, where, sizeof(where));
                fprintf(stderr, "    %2d  %s\n", i, where);
                DWORD64 imageBase = 0;
                PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, NULL);
                if (function) {
                    PVOID handlerData = 0;
                    DWORD64 establisherFrame = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData, &establisherFrame, NULL);
                }
                else { //leaf function: the return address is on top of the stack
                    context.Rip = *(DWORD64*)context.Rsp;
                    context.Rsp += 8;
                }
            }
#else
            (void)crashContext;
            void* frames[40];
            const USHORT count = RtlCaptureStackBackTrace(0, 40, frames, NULL);
            for (USHORT i = 0; i < count; i++) {
                char where[MAX_PATH + 32];
                describeAddress(frames[i], where, sizeof(where));
                fprintf(stderr, "    %2d  %s\n", (int)i, where);
            }
#endif
#ifdef _MSC_VER
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            fprintf(stderr, "    (rest of the call stack unreadable)\n");
        }
#endif
    }

    //For showing a path to the user: Windows separators, no doubled ones.
    std::string tidyPath(const std::string& path)
    {
        std::string out;
        for (size_t i = 0; i < path.size(); i++) {
            const char c = (path[i] == '/') ? '\\' : path[i];
            if (c == '\\' && i > 1 && !out.empty() && out[out.size() - 1] == '\\') { continue; }
            out += c;
        }
        return out;
    }

    std::wstring widen(const std::string& text)
    {
        if (text.empty()) { return std::wstring(); }
        const int n = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, NULL, 0);
        if (n <= 1) { return std::wstring(); }
        std::wstring out(n - 1, L' ');
        MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, &out[0], n);
        return out;
    }

    LONG WINAPI onCrash(EXCEPTION_POINTERS* info)
    {
        static volatile LONG entered = 0;
        if (InterlockedExchange(&entered, 1) != 0) { return EXCEPTION_EXECUTE_HANDLER; } //a crash while reporting one

        const EXCEPTION_RECORD* record = info->ExceptionRecord;
        char where[MAX_PATH + 32];
        describeAddress(record->ExceptionAddress, where, sizeof(where));
        fprintf(stderr, "\n*** Unexpected stop: exception 0x%08lX at %s\n", (unsigned long)record->ExceptionCode, where);
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const ULONG_PTR kind = record->ExceptionInformation[0];
            fprintf(stderr, "    %s address 0x%llx\n", kind == 0 ? "reading" : (kind == 1 ? "writing" : "executing"),
                (unsigned long long)record->ExceptionInformation[1]);
        }
        fflush(stderr);

        bool dumped = false;
        typedef BOOL(WINAPI* WriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
            PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
        HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
        WriteDumpFn writeDump = dbghelp ? (WriteDumpFn)(void*)GetProcAddress(dbghelp, "MiniDumpWriteDump") : 0;
        if (writeDump && !dumpPath.empty()) {
            HANDLE file = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
                exceptionInfo.ThreadId = GetCurrentThreadId();
                exceptionInfo.ExceptionPointers = info;
                exceptionInfo.ClientPointers = FALSE;
                dumped = writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &exceptionInfo, NULL, NULL) != FALSE;
                CloseHandle(file);
            }
        }
        fprintf(stderr, "    Calls:\n");
        writeCallStack(info->ContextRecord);
        if (dumped) { fprintf(stderr, "    Minidump: %s\n", dumpPath.c_str()); }
        fflush(stderr);

        std::wstring message = L"Le simulateur s'est arr\u00EAt\u00E9 de fa\u00E7on inattendue.";
        if (!logPath.empty() || dumped) {
            message += L"\n\nUn rapport a \u00E9t\u00E9 enregistr\u00E9 :\n";
            if (!logPath.empty()) { message += widen(tidyPath(logPath)) + L"\n"; }
            if (dumped) { message += widen(tidyPath(dumpPath)) + L"\n"; }
            message += L"\nMerci de transmettre ces fichiers au support NAUTITECH.";
        }
        MessageBoxW(NULL, message.c_str(), L"NAUTITECH - Simulateur", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
        return EXCEPTION_EXECUTE_HANDLER;
    }
}
#endif // _WIN32


//Global definition for ini logger
namespace IniFile {
    irr::ILogger* irrlichtLogger = 0;
}


// Irrlicht Namespaces
using namespace irr;

/*irr::core::stringw getCredits() {

    irr::core::stringw creditsString(L"NO DATA SUPPLIED WITH THIS PROGRAM, OR DERIVED FROM IT IS TO BE USED FOR NAVIGATION.\n\n");
    creditsString.append(L"Bridge Command is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.\n\n");
    creditsString.append(L"Bridge Command  is distributed  in the  hope that  it will  be useful, but WITHOUT ANY WARRANTY; without even the implied  warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\n\n");
    creditsString.append(L"In memory of Sergio Fuentes, who provided many useful suggestions for the program's development.\n\n");
    creditsString.append(L"Many thanks to those who have made their models available for use in Bridge Command:\n");
    creditsString.append(L"> Juergen Klemp\n");
    creditsString.append(L"> Simon D Richardson\n");
    creditsString.append(L"> Jason Simpson\n");
    creditsString.append(L"> Ragnar\n");
    creditsString.append(L"> Thierry Videlaine\n");
    creditsString.append(L"> NETC (Naval Education and Training Command)\n");
    creditsString.append(L"> Sky image from 0ptikz\n\n");
    creditsString.append(L"Many thanks to Ken Trethewey for making his images of the Eddystone lighthouse available.\n\n");

    creditsString.append(L"Many thanks to contributors including David Elir Evans, Antoine Saillard, Konrad Wolsing, Jan Bauer, AndreySSH, Manfred, ceeac.\n\n");

    creditsString.append(L"Bridge Command uses the Irrlicht Engine, the ENet networking library, ASIO, PortAudio, water based on Keith Lantz FFT water implementation, RealisticWaterSceneNode by elvman, AIS Parser by Brian C. Lane, and the Serial library by William Woodall. Bridge Command depends on libsndfile, which is released under the GNU Lesser General Public License version 2.1 or 3.\n\n");

    creditsString.append(L"The Irrlicht Engine is based in part on the work of the Independent JPEG Group, the zlib, and libpng.");

    return creditsString;
}*/

/*void displayLoadingScreen(irr::IrrlichtDevice* device)
{
    /*kyara frame limiter 01
    irr::u32 targetFPS = 144; // Your desired FPS
    irr::u32 frameDelay = 1000 / targetFPS;
    auto lastFrameTime = std::chrono::steady_clock::now();*/


    /*Use the -> operator when accessing members of the device pointer.
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIEnvironment* env = device->getGUIEnvironment();

    // Load the background image.
    irr::video::ITexture* backgroundImage = driver->getTexture("media/background.png");


    // Determine the screen size.
    irr::core::dimension2d<irr::u32> screenSize = driver->getScreenSize();

    /* Set the duration(in milliseconds) of the loading screen.
    irr::u32 loadingDuration = 3000;
    irr::u32 startTime = device->getTimer()->getTime();

    // Display the loading screen for the specified time duration.
    while (device->run() && (device->getTimer()->getTime() - startTime) < loadingDuration)
    {*/

    /*kyara frame limiter 02
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameTime).count();

        if (elapsed < frameDelay) {
            std::this_thread::sleep_for(std::chrono::milliseconds(frameDelay - elapsed));
        }
        lastFrameTime = std::chrono::steady_clock::now();*/



        /*driver->beginScene(true, true, irr::video::SColor(255, 0, 0, 0)); // Clear screen (using black)

        if (backgroundImage)
        {
            driver->draw2DImage(backgroundImage,
                irr::core::position2d<irr::s32>(0, 0),
                irr::core::rect<irr::s32>(0, 0, screenSize.Width, screenSize.Height),
                0,
                irr::video::SColor(255, 255, 255, 255),
                true);
        }



        // Draw any additional GUI elements if present.
        env->drawAll();

        driver->endScene();
    }*/



JoystickSetup getJoystickSetup(std::string iniFilename, bool isAzimuthDrive) {
    //Load joystick settings, subtract 1 as first axis is 0 internally (not 1)
    JoystickSetup joystickSetup;
    if (!(isAzimuthDrive)) {
        joystickSetup.portJoystickAxis = IniFile::iniFileTou32(iniFilename, "port_throttle_channel") - 1;
        joystickSetup.stbdJoystickAxis = IniFile::iniFileTou32(iniFilename, "stbd_throttle_channel") - 1;
    }
    joystickSetup.rudderJoystickAxis = IniFile::iniFileTou32(iniFilename, "rudder_channel") - 1;

    joystickSetup.bowThrusterJoystickAxis = IniFile::iniFileTou32(iniFilename, "bow_thruster_channel") - 1;
    joystickSetup.sternThrusterJoystickAxis = IniFile::iniFileTou32(iniFilename, "stern_thruster_channel") - 1;


    if (isAzimuthDrive) {
        // DEE 10JAN23 vvvv Azimuth drive specific code moved to here

    // joystick numbers used for azimuth drive controls
        joystickSetup.portThrustLever_joystickNo = IniFile::iniFileTou32(iniFilename, "portThrustLever_joystickNo");
        joystickSetup.stbdThrustLever_joystickNo = IniFile::iniFileTou32(iniFilename, "stbdhrustLever_joystickNo");
        joystickSetup.portSchottel_joystickNo = IniFile::iniFileTou32(iniFilename, "portSchottel_joystickNo");
        joystickSetup.stbdSchottel_joystickNo = IniFile::iniFileTou32(iniFilename, "stbdSchottel_joystickNo");

        // axes used for azimuth drive controls
        joystickSetup.portThrustLever_channel = IniFile::iniFileTou32(iniFilename, "portThrustLever_channel") - 1;
        joystickSetup.stbdThrustLever_channel = IniFile::iniFileTou32(iniFilename, "stbdThrustLever_channel") - 1;
        joystickSetup.portSchottel_channel = IniFile::iniFileTou32(iniFilename, "portSchottel_channel") - 1;
        joystickSetup.stbdSchottel_channel = IniFile::iniFileTou32(iniFilename, "stbdSchottel_channel") - 1;

        // inversion of joystick axes
        // NB dont use this for schottels like Shetland Traders because only one axis is inverted
        // to model that use the boat.ini file

        joystickSetup.schottelPortDirection = 1;
        if (IniFile::iniFileTou32(iniFilename, "invertPortSchottel") == 1) {
            joystickSetup.schottelPortDirection = -1;
        }

        joystickSetup.schottelStbdDirection = -1;
        if (IniFile::iniFileTou32(iniFilename, "invertStbdSchottel") == 1) {
            joystickSetup.schottelStbdDirection = -1;
        }

        joystickSetup.thrustLeverPortDirection = 1;
        if (IniFile::iniFileTou32(iniFilename, "invertPortThrustLever") == 1) {
            joystickSetup.thrustLeverPortDirection = -1;
        }

        joystickSetup.thrustLeverStbdDirection = -1;
        if (IniFile::iniFileTou32(iniFilename, "invertStbdthrustLever") == 1) {
            joystickSetup.thrustLeverStbdDirection = -1;
        }

        // offset and scaling
        joystickSetup.schottelPortScaling = IniFile::iniFileTof32(iniFilename, "scalingPortSchottelAngle");
        joystickSetup.schottelStbdScaling = IniFile::iniFileTof32(iniFilename, "scalingStbdSchottelAngle");
        joystickSetup.schottelPortOffset = IniFile::iniFileTof32(iniFilename, "offsetPortSchottelAngle");
        joystickSetup.schottelStbdOffset = IniFile::iniFileTou32(iniFilename, "offsetStbdSchottelAngle");

        joystickSetup.thrustLeverPortScaling = IniFile::iniFileTof32(iniFilename, "scalingPortThrustLever");
        joystickSetup.thrustLeverStbdScaling = IniFile::iniFileTof32(iniFilename, "scalingStbdThrustLever");
        joystickSetup.thrustLeverPortOffset = IniFile::iniFileTof32(iniFilename, "offsetPortThrustLever");
        joystickSetup.thrustLeverStbdOffset = IniFile::iniFileTof32(iniFilename, "offsetStbdThrustLever");



        // DEE 10JAN23 ^^^^
    }
    else {
        joystickSetup.portJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_port"); //TODO: Note that these have changed after 5.0b4 to be consistent with BC4.7
        joystickSetup.stbdJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_stbd");
    }
    joystickSetup.rudderJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_rudder");


    joystickSetup.bowThrusterJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_bow_thruster");
    joystickSetup.sternThrusterJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_stern_thruster");
    //Joystick button mapping
    joystickSetup.joystickNoHorn = IniFile::iniFileTou32(iniFilename, "joystick_no_horn");
    joystickSetup.joystickButtonHorn = IniFile::iniFileTou32(iniFilename, "joystick_button_horn") - 1;

    joystickSetup.joystickNoChangeView = IniFile::iniFileTou32(iniFilename, "joystick_no_change_view");
    joystickSetup.joystickButtonChangeView = IniFile::iniFileTou32(iniFilename, "joystick_button_change_view") - 1;

    joystickSetup.joystickNoChangeAndLockView = IniFile::iniFileTou32(iniFilename, "joystick_no_change_and_lock_view");
    joystickSetup.joystickButtonChangeAndLockView = IniFile::iniFileTou32(iniFilename, "joystick_button_change_and_lock_view") - 1;

    joystickSetup.joystickNoLookStepLeft = IniFile::iniFileTou32(iniFilename, "joystick_no_look_step_left");
    joystickSetup.joystickButtonLookStepLeft = IniFile::iniFileTou32(iniFilename, "joystick_button_look_step_left") - 1;

    joystickSetup.joystickNoLookStepRight = IniFile::iniFileTou32(iniFilename, "joystick_no_look_step_right");
    joystickSetup.joystickButtonLookStepRight = IniFile::iniFileTou32(iniFilename, "joystick_button_look_step_right") - 1;

    joystickSetup.joystickNoIncreaseBowThrust = IniFile::iniFileTou32(iniFilename, "joystick_no_increase_bow_thrust");
    joystickSetup.joystickButtonIncreaseBowThrust = IniFile::iniFileTou32(iniFilename, "joystick_button_increase_bow_thrust") - 1;

    joystickSetup.joystickNoDecreaseBowThrust = IniFile::iniFileTou32(iniFilename, "joystick_no_decrease_bow_thrust");
    joystickSetup.joystickButtonDecreaseBowThrust = IniFile::iniFileTou32(iniFilename, "joystick_button_decrease_bow_thrust") - 1;

    joystickSetup.joystickNoIncreaseSternThrust = IniFile::iniFileTou32(iniFilename, "joystick_no_increase_stern_thrust");
    joystickSetup.joystickButtonIncreaseSternThrust = IniFile::iniFileTou32(iniFilename, "joystick_button_increase_stern_thrust") - 1;

    joystickSetup.joystickNoDecreaseSternThrust = IniFile::iniFileTou32(iniFilename, "joystick_no_decrease_stern_thrust");
    joystickSetup.joystickButtonDecreaseSternThrust = IniFile::iniFileTou32(iniFilename, "joystick_button_decrease_stern_thrust") - 1;

    joystickSetup.joystickNoBearingOn = IniFile::iniFileTou32(iniFilename, "joystick_no_bearing_on");
    joystickSetup.joystickButtonBearingOn = IniFile::iniFileTou32(iniFilename, "joystick_button_bearing_on") - 1;

    joystickSetup.joystickNoBearingOff = IniFile::iniFileTou32(iniFilename, "joystick_no_bearing_off");
    joystickSetup.joystickButtonBearingOff = IniFile::iniFileTou32(iniFilename, "joystick_button_bearing_off") - 1;

    joystickSetup.joystickNoZoomOn = IniFile::iniFileTou32(iniFilename, "joystick_no_zoom_on");
    joystickSetup.joystickButtonZoomOn = IniFile::iniFileTou32(iniFilename, "joystick_button_zoom_on") - 1;

    joystickSetup.joystickNoZoomOff = IniFile::iniFileTou32(iniFilename, "joystick_no_zoom_off");
    joystickSetup.joystickButtonZoomOff = IniFile::iniFileTou32(iniFilename, "joystick_button_zoom_off") - 1;

    joystickSetup.joystickNoLookLeft = IniFile::iniFileTou32(iniFilename, "joystick_no_look_left");
    joystickSetup.joystickButtonLookLeft = IniFile::iniFileTou32(iniFilename, "joystick_button_look_left") - 1;

    joystickSetup.joystickNoLookRight = IniFile::iniFileTou32(iniFilename, "joystick_no_look_right");
    joystickSetup.joystickButtonLookRight = IniFile::iniFileTou32(iniFilename, "joystick_button_look_right") - 1;

    joystickSetup.joystickNoLookUp = IniFile::iniFileTou32(iniFilename, "joystick_no_look_up");
    joystickSetup.joystickButtonLookUp = IniFile::iniFileTou32(iniFilename, "joystick_button_look_up") - 1;

    joystickSetup.joystickNoLookDown = IniFile::iniFileTou32(iniFilename, "joystick_no_look_down");
    joystickSetup.joystickButtonLookDown = IniFile::iniFileTou32(iniFilename, "joystick_button_look_down") - 1;

    joystickSetup.joystickNoPump1On = IniFile::iniFileTou32(iniFilename, "joystick_no_pump1_on");
    joystickSetup.joystickButtonPump1On = IniFile::iniFileTou32(iniFilename, "joystick_button_pump1_on") - 1;

    joystickSetup.joystickNoPump1Off = IniFile::iniFileTou32(iniFilename, "joystick_no_pump1_off");
    joystickSetup.joystickButtonPump1Off = IniFile::iniFileTou32(iniFilename, "joystick_button_pump1_off") - 1;

    joystickSetup.joystickNoPump2On = IniFile::iniFileTou32(iniFilename, "joystick_no_pump2_on");
    joystickSetup.joystickButtonPump2On = IniFile::iniFileTou32(iniFilename, "joystick_button_pump2_on") - 1;

    joystickSetup.joystickNoPump2Off = IniFile::iniFileTou32(iniFilename, "joystick_no_pump2_off");
    joystickSetup.joystickButtonPump2Off = IniFile::iniFileTou32(iniFilename, "joystick_button_pump2_off") - 1;

    joystickSetup.joystickNoFollowUpOn = IniFile::iniFileTou32(iniFilename, "joystick_no_follow_up_on");
    joystickSetup.joystickButtonFollowUpOn = IniFile::iniFileTou32(iniFilename, "joystick_button_follow_up_on") - 1;

    joystickSetup.joystickNoFollowUpOff = IniFile::iniFileTou32(iniFilename, "joystick_no_follow_up_off");
    joystickSetup.joystickButtonFollowUpOff = IniFile::iniFileTou32(iniFilename, "joystick_button_follow_up_off") - 1;

    joystickSetup.joystickNoNFUPort = IniFile::iniFileTou32(iniFilename, "joystick_no_NFU_port");
    joystickSetup.joystickButtonNFUPort = IniFile::iniFileTou32(iniFilename, "joystick_button_NFU_port") - 1;

    joystickSetup.joystickNoNFUStbd = IniFile::iniFileTou32(iniFilename, "joystick_no_NFU_stbd");
    joystickSetup.joystickButtonNFUStbd = IniFile::iniFileTou32(iniFilename, "joystick_button_NFU_stbd") - 1;

    joystickSetup.joystickNoAckAlarm = IniFile::iniFileTou32(iniFilename, "joystick_no_ack_alarm");
    joystickSetup.joystickButtonAckAlarm = IniFile::iniFileTou32(iniFilename, "joystick_button_ack_alarm") - 1;

    // Radar controls by button
    joystickSetup.joystickNoIncreaseClutterSetting = IniFile::iniFileTou32(iniFilename, "joystick_no_increase_radar_clutter");
    joystickSetup.joystickNoDecreaseClutterSetting = IniFile::iniFileTou32(iniFilename, "joystick_no_decrease_radar_clutter");
    joystickSetup.joystickButtonIncreaseClutterSetting = IniFile::iniFileTou32(iniFilename, "joystick_button_increase_radar_clutter") - 1;
    joystickSetup.joystickButtonDecreaseClutterSetting = IniFile::iniFileTou32(iniFilename, "joystick_button_decrease_radar_clutter") - 1;

    joystickSetup.joystickNoIncreaseGainSetting = IniFile::iniFileTou32(iniFilename, "joystick_no_increase_radar_gain");
    joystickSetup.joystickNoDecreaseGainSetting = IniFile::iniFileTou32(iniFilename, "joystick_no_decrease_radar_gain");
    joystickSetup.joystickButtonIncreaseGainSetting = IniFile::iniFileTou32(iniFilename, "joystick_button_increase_radar_gain") - 1;
    joystickSetup.joystickButtonDecreaseGainSetting = IniFile::iniFileTou32(iniFilename, "joystick_button_decrease_radar_gain") - 1;

    joystickSetup.joystickNoIncreaseRainSetting = IniFile::iniFileTou32(iniFilename, "joystick_no_increase_radar_rain");
    joystickSetup.joystickNoDecreaseRainSetting = IniFile::iniFileTou32(iniFilename, "joystick_no_decrease_radar_rain");
    joystickSetup.joystickButtonIncreaseRainSetting = IniFile::iniFileTou32(iniFilename, "joystick_button_increase_radar_rain") - 1;
    joystickSetup.joystickButtonDecreaseRainSetting = IniFile::iniFileTou32(iniFilename, "joystick_button_decrease_radar_rain") - 1;

    joystickSetup.joystickNoDecreaseRange = IniFile::iniFileTou32(iniFilename, "joystick_no_decrease_radar_range");
    joystickSetup.joystickNoIncreaseRange = IniFile::iniFileTou32(iniFilename, "joystick_no_increase_radar_range");
    joystickSetup.joystickButtonDecreaseRange = IniFile::iniFileTou32(iniFilename, "joystick_button_decrease_radar_range") - 1;
    joystickSetup.joystickButtonIncreaseRange = IniFile::iniFileTou32(iniFilename, "joystick_button_increase_radar_range") - 1;
    // End of Radar controls by button

    joystickSetup.joystickNoAzimuth1Master = IniFile::iniFileTou32(iniFilename, "joystick_no_toggle_azimuth1_master");
    joystickSetup.joystickNoAzimuth2Master = IniFile::iniFileTou32(iniFilename, "joystick_no_toggle_azimuth2_master");
    joystickSetup.joystickButtonAzimuth1Master = IniFile::iniFileTou32(iniFilename, "joystick_button_toggle_azimuth1_master") - 1;
    joystickSetup.joystickButtonAzimuth2Master = IniFile::iniFileTou32(iniFilename, "joystick_button_toggle_azimuth2_master") - 1;

    joystickSetup.joystickNoPOV = IniFile::iniFileTou32(iniFilename, "joystick_no_POV");
    joystickSetup.joystickPOVLookLeft = IniFile::iniFileTou32(iniFilename, "joystick_POV_look_left");
    joystickSetup.joystickPOVLookRight = IniFile::iniFileTou32(iniFilename, "joystick_POV_look_right");
    joystickSetup.joystickPOVLookUp = IniFile::iniFileTou32(iniFilename, "joystick_POV_look_up");
    joystickSetup.joystickPOVLookDown = IniFile::iniFileTou32(iniFilename, "joystick_POV_look_down");

    //Joystick mapping
    irr::u32 numberOfJoystickPoints = IniFile::iniFileTou32(iniFilename, "joystick_map_points");
    if (numberOfJoystickPoints > 0) {
        for (irr::u32 i = 1; i < numberOfJoystickPoints + 1; i++) {
            joystickSetup.inputPoints.push_back(IniFile::iniFileTof32(iniFilename, IniFile::enumerate2("joystick_map", i, 1)));
            joystickSetup.outputPoints.push_back(IniFile::iniFileTof32(iniFilename, IniFile::enumerate2("joystick_map", i, 2)));
        }
    }
    //Default linear mapping if not set
    if (joystickSetup.inputPoints.size() < 2) {
        joystickSetup.inputPoints.clear();
        joystickSetup.outputPoints.clear();
        joystickSetup.inputPoints.push_back(-1.0);
        joystickSetup.inputPoints.push_back(1.0);
        joystickSetup.outputPoints.push_back(-1.0);
        joystickSetup.outputPoints.push_back(1.0);
    }
    joystickSetup.rudderDirection = 1;
    if (IniFile::iniFileTou32(iniFilename, "invert_rudder") == 1) {
        joystickSetup.rudderDirection = -1;
    }

    // DEE 10JAN23 vvvv
        // joystickSetup.azimuth1Direction = 1;
        // joystickSetup.azimuth1Direction = -1;


    //    joystickSetup.azimuth1Offset = IniFile::iniFileTof32(iniFilename, "offset_azimuth1_angle",1.0);
    //    joystickSetup.azimuth2Offset = IniFile::iniFileTof32(iniFilename, "offset_azimuth2_angle",1.0);
    //    joystickSetup.azimuth1Scaling = IniFile::iniFileTof32(iniFilename, "scaling_azimuth1_angle",0.0);
    //    joystickSetup.azimuth2Scaling = IniFile::iniFileTof32(iniFilename, "scaling_azimuth2_angle",0.0);

    // These are all wrapped up in an if isAzimuth earlier in this funciton

    // DEE 10JAN22 ^^^^

        // Check if user wants to update all joystick axes when one changes
    if (IniFile::iniFileTou32(iniFilename, "update_changed_axes_only") == 1) {
        joystickSetup.updateAllAxes = false;
    }
    else {
        joystickSetup.updateAllAxes = true;
    }


    return joystickSetup;
}

#ifdef _WIN32
static LRESULT CALLBACK CustomWndProc(HWND hWnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        HWND hwndCtl = (HWND)lParam;
        int code = HIWORD(wParam);
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }
        break;
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    }


    return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

//The first UDP port from 'first' on that nothing on this PC listens on yet (for the radar station).
static irr::u32 freeUdpPort(irr::u32 first)
{
    for (irr::u32 port = first; port < first + 20 && port <= 65535; port++) {
        asio::io_context io;
        asio::ip::udp::socket probe(io);
        asio::error_code error;
        probe.open(asio::ip::udp::v4(), error);
        if (error) { return first; }
        probe.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), (unsigned short)port), error);
        if (!error) { return port; } //closed again when probe goes
    }
    return first;
}

//Radar station: another copy of this simulator, started with -radar-station <port> on another screen,
//showing the large radar of this one. It is a secondary display of this simulator, fed over 127.0.0.1.
//It is closed with this simulator (Windows: a job object that ends it when this process ends, even
//after a crash; Linux: the parent-death signal).
static bool startRadarStation(int screen, irr::u32 port, const std::string& iniArgument)
{
#ifdef _WIN32
    wchar_t exe[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, exe, MAX_PATH) == 0) { return false; }
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\"";
    if (!iniArgument.empty()) { cmd += L" -c \"" + std::wstring(iniArgument.begin(), iniArgument.end()) + L"\""; } //-c must come first
    cmd += L" -radar-station " + std::to_wstring(port) + L" -monitor " + std::to_wstring(screen);

    static HANDLE job = 0;
    if (!job) {
        job = CreateJobObjectW(NULL, NULL);
        if (job) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
            ZeroMemory(&limits, sizeof(limits));
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        }
    }
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    std::vector<wchar_t> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back(0);
    if (!CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cerr << "Could not start the radar station (error " << GetLastError() << ")." << std::endl;
        return false;
    }
    if (job) { AssignProcessToJobObject(job, pi.hProcess); }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#elif defined(__linux__)
    const pid_t parent = getpid();
    const pid_t child = fork();
    if (child < 0) { return false; }
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent) { _exit(0); } //the simulator already ended
        const std::string portText = std::to_string(port);
        const std::string screenText = std::to_string(screen);
        if (!iniArgument.empty()) {
            execl("/proc/self/exe", "Simulator", "-c", iniArgument.c_str(), "-radar-station", portText.c_str(), "-monitor", screenText.c_str(), (char*)0);
        }
        else {
            execl("/proc/self/exe", "Simulator", "-radar-station", portText.c_str(), "-monitor", screenText.c_str(), (char*)0);
        }
        _exit(1);
    }
    return true;
#else
    (void)screen; (void)port; (void)iniArgument;
    return false;
#endif
}

int main(int argc, char** argv)
{
    // --- 1. NAUTITECH USB STARTUP CHECK ---
    std::string reason;
    if (!UsbKeyPresent(&reason))
    {
        MessageBoxA(NULL,
            ("Cle de licence NAUTITECH non trouvee.\n\n" + reason +
                "\n\nVeuillez inserer la cle USB puis relancer le simulateur.").c_str(),
            "NAUTITECH - Licence", MB_OK | MB_ICONERROR);
        return 1;   // Closes the application immediately
    }
    // --------------------------------------
#ifdef WITH_PROFILING
    IPROF_FUNC;
#endif

#ifdef FOR_DEB
    chdir("/usr/share/bridgecommand");
#endif // FOR_DEB

    //Mac OS:
#ifdef __APPLE__
//Find starting folder
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
    //change up from BridgeCommand.app/Contents/MacOS/bc.app/Contents/MacOS to BridgeCommand.app/Contents/Resources
    exeFolderPath.append("/../../../../Resources");
    //change to this path now, so ini file is read
    chdir(exeFolderPath.c_str());
    //Note, we use this again after the createDevice call
#endif

//User read/write location - look in here first and the exe folder second for files
    std::string userFolder = Utilities::getUserDir();

    //Read basic ini settings
    std::string iniFilename = "bc5.ini";
    //Use local ini file if it exists
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }

    std::string iniArgument; //-c <file>, passed on to the radar station
    if ((argc > 2) && (strcmp(argv[1], "-c") == 0)) {
        iniFilename = std::string(argv[2]); //TODO: Check this for sanity?
        iniArgument = iniFilename;
        std::cout << "Using Ini file >" << iniFilename << "<" << std::endl;
    }

    //Large radar on another screen: -radar N (from the launcher), else bc5.ini radar_screen (0 = none).
    //This copy IS that radar station when started with -radar-station <port>.
    int radarScreen = (int)IniFile::iniFileTou32(iniFilename, "radar_screen");
    bool radarStation = false;
    irr::u32 radarStationPort = 0;
    for (int arg = 1; arg + 1 < argc; arg++) {
        if (strcmp(argv[arg], "-radar") == 0) { radarScreen = atoi(argv[arg + 1]); }
        if (strcmp(argv[arg], "-radar-station") == 0) {
            radarStation = true;
            radarStationPort = (irr::u32)atoi(argv[arg + 1]);
        }
    }
    if (radarStation) { radarScreen = 0; }

    //The user folder (settings, log): made now if this is the first run on this PC.
    const std::string userFolders[2] = { Utilities::getUserDirBase(), userFolder };
    for (int i = 0; i < 2; i++) {
        if (userFolders[i].size() > 1 && !Utilities::pathExists(userFolders[i])) {
            const std::string pathToMake = userFolders[i].substr(0, userFolders[i].size() - 1); //no trailing slash
#ifdef _WIN32
            _mkdir(pathToMake.c_str());
#else
            mkdir(pathToMake.c_str(), 0755);
#endif // _WIN32
        }
    }

    //Several copies can run on one PC (one per screen, say): each has a number, 1 for the first, and
    //keeps its own log (log.txt, then log-2.txt...) and console window placement.
    irr::u32 instanceNumber = 1;
#ifdef _WIN32
    for (irr::u32 n = 1; n <= 16; n++) {
        const std::string mutexName = "NautitechSimulator-" + std::to_string(n);
        HANDLE mutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
        if (mutex && GetLastError() != ERROR_ALREADY_EXISTS) {
            instanceNumber = n; //the mutex stays held until this copy ends
            break;
        }
        if (mutex) { CloseHandle(mutex); }
    }
#endif
    const std::string logName = (instanceNumber == 1) ? std::string("log.txt") : "log-" + std::to_string(instanceNumber) + ".txt";
    bool logRedirected = false; //stdout and stderr go to the log file

#ifdef _WIN32
    //Messages: a console window on request (debug_console=1), else the log file. (A console is only
    //already there if this was built without the subsystem pragma above, and then it is kept.)
    if (GetConsoleWindow() == NULL) {
        FILE* stream = 0;
        if (IniFile::iniFileTou32(iniFilename, "debug_console") == 1 && AllocConsole()) {
            freopen_s(&stream, "CONOUT$", "w", stdout);
            freopen_s(&stream, "CONOUT$", "w", stderr);
        }
        else {
            const std::string logPath = userFolder + logName;
            std::ofstream(logPath.c_str(), std::ios::trunc).close(); //a fresh log for each run
            //freopen, not freopen_s: freopen_s locks the file, so stderr could not share it with stdout
            //(its messages were lost), nor could the messages saved at the end be added.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
            const bool outOk = freopen(logPath.c_str(), "a", stdout) != NULL;
            const bool errOk = freopen(logPath.c_str(), "a", stderr) != NULL;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
            if (outOk) { setvbuf(stdout, NULL, _IONBF, 0); } //in the file at once, even if the program stops
            if (errOk) { CrashReport::logPath = logPath; }
            logRedirected = outOk;
        }
    }
    CrashReport::dumpPath = userFolder + ((instanceNumber == 1) ? std::string("crash.dmp") : "crash-" + std::to_string(instanceNumber) + ".dmp");
    SetUnhandledExceptionFilter(CrashReport::onCrash);
    std::cout << "Simulator copy " << instanceNumber << " on this PC" << std::endl;
#endif

    std::string scriptToExe = IniFile::iniFileToString(iniFilename, "script_start_BC");
    if (!scriptToExe.empty()) {
        std::string scriptPath;
        if (Utilities::pathExists(userFolder + scriptToExe)) {
            scriptPath = userFolder + scriptToExe;
        }
        else {
#ifdef _WIN32
            scriptPath = "Scripts\\win\\" + scriptToExe;
#else
#ifdef __APPLE__
            scriptPath = "./Scripts/macOS/" + scriptToExe;
#else
            scriptPath = "./Scripts/linux/" + scriptToExe;
#endif
#endif    
        }

        std::cout << "Going to run " << scriptPath << std::endl;

#ifdef _WIN32
        ShellExecute(NULL, "open", scriptPath.c_str(), NULL, NULL, SW_MINIMIZE);
#else
        scriptPath = "\"" + scriptPath + "\"";
        system(scriptPath.c_str());
#endif
    }

#ifdef __arm__
    if (IniFile::iniFileTou32(iniFilename, "PA_ALSA_PLUGHW") == 1) {
        setenv("PA_ALSA_PLUGHW", "1", true);
    }
#endif

    irr::u32 graphicsWidth = IniFile::iniFileTou32(iniFilename, "graphics_width");
    irr::u32 graphicsHeight = IniFile::iniFileTou32(iniFilename, "graphics_height");
    irr::u32 graphicsDepth = IniFile::iniFileTou32(iniFilename, "graphics_depth");
    bool fullScreen = (IniFile::iniFileTou32(iniFilename, "graphics_mode") == 1); //1 for full screen
    bool fakeFullScreen = (IniFile::iniFileTou32(iniFilename, "graphics_mode") == 3); //3 for no border
    if (radarStation) { //the whole of its screen, without a frame
        fullScreen = false;
        fakeFullScreen = true;
    }
#ifdef __APPLE__
    if (fakeFullScreen) {
        fullScreen = true; //Fall back for mac
    }
#endif
    irr::u32 antiAlias = IniFile::iniFileTou32(iniFilename, "anti_alias"); // 0 or 1 for disabled, 2,4,6,8 etc for FSAA
    irr::u32 directX = IniFile::iniFileTou32(iniFilename, "use_directX"); // 0 for openGl, 1 for directX (if available)
    irr::u32 disableShaders = IniFile::iniFileTou32(iniFilename, "disable_shaders"); // 0 for normal, 1 for no shaders
    if (directX == 1) {
        disableShaders = 1; //FIXME: Hardcoded for no directX shaders
    }
    irr::u32 waterSegments = IniFile::iniFileTou32(iniFilename, "water_segments"); // power of 2
    if (waterSegments == 0) {
        waterSegments = 32;
    }
    // KYARA: water reflection quality tri-state.
    //   ini water_reflection: 1 = full (every frame), 2 = half (every 2nd frame), 3 = off.
    //   Missing/0 -> full, so existing installs are unchanged.
    irr::u32 reflectionIni = IniFile::iniFileTou32(iniFilename, "water_reflection");
    irr::u32 reflectionMode; // internal: 0 full, 1 half, 2 off
    if (reflectionIni == 3) { reflectionMode = 2; }
    else if (reflectionIni == 2) { reflectionMode = 1; }
    else { reflectionMode = 0; } // 0 (unset) or 1

    irr::u32 numberOfContactPointsX = IniFile::iniFileTou32(iniFilename, "contact_points_X");
    if (numberOfContactPointsX == 0) {
        numberOfContactPointsX = 10;
    }
    irr::u32 numberOfContactPointsY = IniFile::iniFileTou32(iniFilename, "contact_points_Y");
    if (numberOfContactPointsY == 0) {
        numberOfContactPointsY = 30;
    }
    irr::u32 numberOfContactPointsZ = IniFile::iniFileTou32(iniFilename, "contact_points_Z");
    if (numberOfContactPointsZ == 0) {
        numberOfContactPointsZ = 30;
    }
    irr::core::vector3di numberOfContactPoints(numberOfContactPointsX, numberOfContactPointsY, numberOfContactPointsZ);

    irr::f32 minContactPointSpacing = IniFile::iniFileTof32(iniFilename, "contact_points_minSpacing", 100); // Large default

    bool debugMode = (IniFile::iniFileTou32(iniFilename, "debug_mode") == 1);

    bool showTideHeight = (IniFile::iniFileTou32(iniFilename, "show_tide_height") == 1);

    irr::u32 limitTerrainResolution = IniFile::iniFileTou32(iniFilename, "max_terrain_resolution"); //Default of zero means unlimited


    irr::f32 contactStiffnessFactor = IniFile::iniFileTof32(iniFilename, "contactStiffness_perArea"); //Contact stiffness to use
    irr::f32 contactDampingFactor = IniFile::iniFileTof32(iniFilename, "contactDamping_factor"); //Contact damping factor (roughly proportion of critical)
    irr::f32 lineStiffnessFactor = IniFile::iniFileTof32(iniFilename, "lineStiffness_factor", 1.0); //Line stiffness scaling factor
    irr::f32 lineDampingFactor = IniFile::iniFileTof32(iniFilename, "lineDamping_factor", 1.0); //Line damping factor (roughly proportion of critical)
    irr::f32 frictionCoefficient = IniFile::iniFileTof32(iniFilename, "contactFriction_coefficient", 0.5); //Contact friction coefficient (0-1)
    irr::f32 tanhFrictionFactor = IniFile::iniFileTof32(iniFilename, "contactFriction_tanhFactor", 1); //Contact friction factor (Generally 1-100)
    if (frictionCoefficient < 0) { frictionCoefficient = 0; }
    if (frictionCoefficient > 1) { frictionCoefficient = 1; }
    if (tanhFrictionFactor < 0) { tanhFrictionFactor = 0; }


    //Initial view configuration
    irr::f32 viewAngle = IniFile::iniFileTof32(iniFilename, "view_angle"); //Horizontal field of view
    irr::f32 lookAngle = IniFile::iniFileTof32(iniFilename, "look_angle"); //Initial look angle
    if (viewAngle <= 0) {
        viewAngle = 90;
    }

    //NAUTITECH triple-screen: angled per-TV rendering for the Eyefinity wraparound.
    //When enabled, the main 3D view is drawn as three columns, each its own camera:
    //left yawed to port, centre straight ahead, right yawed to starboard.
    bool triScreen = (IniFile::iniFileTou32(iniFilename, "triple_screen") == 1);
    irr::f32 perScreenFOV = IniFile::iniFileTof32(iniFilename, "per_screen_fov"); //Horizontal FOV of ONE TV (degrees)
    if (perScreenFOV <= 0) { perScreenFOV = 45; }
    irr::f32 bezelYaw = IniFile::iniFileTof32(iniFilename, "bezel_yaw"); //Angle between adjacent TV centres (degrees)
    if (bezelYaw <= 0) { bezelYaw = perScreenFOV; }

    irr::f32 cameraMinDistance = IniFile::iniFileTof32(iniFilename, "minimum_distance");
    irr::f32 cameraMaxDistance = IniFile::iniFileTof32(iniFilename, "maximum_distance");
    if (cameraMinDistance <= 0) {
        cameraMinDistance = 1;
    }
    if (cameraMaxDistance <= 0) {
        cameraMaxDistance = 6 * M_IN_NM;
    }


    //Load NMEA settings
    std::string nmeaSerialPortName = IniFile::iniFileToString(iniFilename, "NMEA_ComPort");
    irr::u32 nmeaSerialPortBaudrate = IniFile::iniFileTou32(iniFilename, "NMEA_Baudrate", 4800);
    std::string nmeaUDPAddressName = IniFile::iniFileToString(iniFilename, "NMEA_UDPAddress");
    std::string nmeaUDPPortName = IniFile::iniFileToString(iniFilename, "NMEA_UDPPort");
    std::string nmeaUDPListenPortName = IniFile::iniFileToString(iniFilename, "NMEA_UDPListenPort");

    //Load UDP network settings
    irr::u32 udpPort = IniFile::iniFileTou32(iniFilename, "udp_send_port");
    if (radarStation && radarStationPort > 0) {
        udpPort = radarStationPort;
    }
    if (udpPort == 0) {
        udpPort = 18304;
    }

    int fontSize = 12;
    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale > 1) {
        fontSize = (int)(fontSize * fontScale + 0.5);
    }
    else {
        fontScale = 1.0;
    }

    //Sensible defaults if not set
    if (graphicsWidth == 0 || graphicsHeight == 0) {
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
            if (fullScreen || fakeFullScreen) {
                graphicsWidth = deskres.Width;
            }
            else {
                graphicsWidth = 1200 * fontScale; // deskres.Width*0.8;
                if (graphicsWidth > deskres.Width * 0.90) {
                    graphicsWidth = deskres.Width * 0.90;
                }
            }
        }
        if (graphicsHeight == 0) {
            if (fullScreen) {
                graphicsHeight = deskres.Height;
            }
            else {
                graphicsHeight = 900 * fontScale; // deskres.Height*0.8;
                if (graphicsHeight > deskres.Height * 0.90) {
                    graphicsHeight = deskres.Height * 0.90;
                }
            }
        }
    }

    if (graphicsDepth == 0) { graphicsDepth = 32; }

    // Check if collision warning should be shown
    bool showCollided;
    if (IniFile::iniFileTou32(iniFilename, "hide_collision_warning") == 1) {
        showCollided = false;
    }
    else {
        showCollided = true;
    }

    //load language
    std::string modifier = IniFile::iniFileToString(iniFilename, "lang");
    if (modifier.length() == 0) {
        modifier = "en"; //Default
    }
    std::string languageFile = "language-";
    languageFile.append(modifier);
    languageFile.append(".txt");
    if (Utilities::pathExists(userFolder + languageFile)) {
        languageFile = userFolder + languageFile;
    }
    Lang language(languageFile);

    irr::SIrrlichtCreationParameters deviceParameters;
    /*kyara
    deviceParameters.HighPrecisionFPU = true;
    deviceParameters.Stencilbuffer = false; // Reduce overhead
    deviceParameters.DriverMultithreaded = true;*/
#ifdef _WIN32

    HWND hWnd = 0;
    HINSTANCE hInstance = 0;
    // create dialog
    const char* Win32ClassName = ScreenChooser::SimulatorWindowClass; //lets the screen choosers see which screens are taken
    const DWORD style = WS_VISIBLE | WS_POPUP;
    int windowX = 0, windowY = 0; //borderless window: top left of its screen (the window is opened below)

    WNDCLASSEX wcex;

    if (fakeFullScreen) {

        int requestedMonitor = IniFile::iniFileTou32(iniFilename, "monitor") - 1; //0 indexed, -1 will indicate default
        //The launcher's screen picker passes its choice as -monitor N (1 indexed); it overrides the ini.
        for (int arg = 1; arg + 1 < argc; arg++) {
            if (strcmp(argv[arg], "-monitor") == 0) {
                requestedMonitor = atoi(argv[arg + 1]) - 1;
            }
        }

        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = (WNDPROC)CustomWndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = DLGWINDOWEXTRA;
        wcex.hInstance = hInstance;
        wcex.hIcon = NULL;
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
        wcex.lpszMenuName = 0;
        wcex.lpszClassName = Win32ClassName;
        wcex.hIconSm = 0;
        RegisterClassEx(&wcex);

        cMonitorsVec Monitors; //The constructor for this initialises it with a list of the monitors

        //Started directly (the launcher always says which screen) on a desk with several screens, and no
        //screen set in bc5.ini: ask which one.
        if (requestedMonitor < 0 && Monitors.iMonitors.size() > 1) {
            const bool french = (modifier == "fr");
            const int chosen = ScreenChooser::ask(
                french ? L"Sur quel \u00E9cran ouvrir le simulateur ?" : L"Which screen should the simulator open on?",
                french ? L"Cliquez sur un \u00E9cran puis sur Ouvrir. Les fen\u00EAtres d\u00E9j\u00E0 ouvertes sont indiqu\u00E9es."
                    : L"Click a screen, then Open. Windows already open are shown.",
                french);
            if (chosen == ScreenChooser::Cancelled) {
                std::cout << "Screen choice cancelled: not starting." << std::endl;
                return EXIT_SUCCESS;
            }
            requestedMonitor = chosen;
        }

        if (requestedMonitor > -1 && (int)Monitors.iMonitors.size() > requestedMonitor) {
            //The user has requested a specific monitor

            //Set to fill requested monitor
            windowX = Monitors.rcMonitors[requestedMonitor].left;
            windowY = Monitors.rcMonitors[requestedMonitor].top;
            graphicsWidth = Monitors.rcMonitors[requestedMonitor].right - Monitors.rcMonitors[requestedMonitor].left;
            graphicsHeight = Monitors.rcMonitors[requestedMonitor].bottom - Monitors.rcMonitors[requestedMonitor].top;
            std::cout << "Screen " << requestedMonitor + 1 << " of " << Monitors.iMonitors.size() << ": " << graphicsWidth << " x " << graphicsHeight
                << " at " << windowX << ", " << windowY << std::endl;
        }
        else {
            //One screen only, or a screen asked for that is not connected: the one under the mouse pointer.
            if (requestedMonitor > -1) {
                std::cerr << "Screen " << requestedMonitor + 1 << " requested, but " << Monitors.iMonitors.size() << " screen(s) connected: using the screen under the mouse pointer." << std::endl;
            }

            //Find location of mouse cursor
            POINT p;
            if (GetCursorPos(&p))
            {
                //Find monitor this is on
                HMONITOR monitor = MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY);
                MONITORINFO mi;
                RECT        rc;

                mi.cbSize = sizeof(mi);
                GetMonitorInfo(monitor, &mi);
                rc = mi.rcMonitor;

                //Set to fill current monitor
                windowX = rc.left;
                windowY = rc.top;
                graphicsWidth = rc.right - rc.left;
                graphicsHeight = rc.bottom - rc.top;
            }
        }

    }
#endif

    //Use an extra SIrrlichtCreationParameters parameter, added to our version of the Irrlicht source, to request a borderless X11 window if requested
#ifdef __linux__
    if (fakeFullScreen) {
        deviceParameters.X11borderless = true; //Has an effect on X11 only
    }
#endif

    //create device
    deviceParameters.DriverType = irr::video::EDT_OPENGL;
    //Allow optional directX if available
    if (directX == 1) {
        if (irr::IrrlichtDevice::isDriverSupported(irr::video::EDT_DIRECT3D9)) {
            deviceParameters.DriverType = irr::video::EDT_DIRECT3D9;
        }
        else {
            std::cerr << "DirectX 9 requested but not available.\nThis may be because Simulateur de Navigation Maritime has been compiled without DirectX support,\nor your system does not support DirectX.\nTrying OpenGL" << std::endl << std::endl;
        }
    }

    deviceParameters.Vsync = false;  //KYARA: all three deployment TVs are 60Hz. Vsync IS the frame
    //limiter, and it is a better one than any sleep loop - it paces
    //to the actual scan-out. No tearing across the three panels.

    deviceParameters.WindowSize = irr::core::dimension2d<irr::u32>(graphicsWidth, graphicsHeight);
    deviceParameters.Bits = graphicsDepth;
    deviceParameters.Fullscreen = fullScreen;
    deviceParameters.AntiAlias = antiAlias;

    //Start the 3D display. If that fails: on Windows, with the borderless window on another screen than
    //the main one, start it on the main screen and move the window there afterwards (OpenGL does not
    //always start in a window on a screen driven by another graphics output); and if the graphics
    //driver refuses the anti-aliasing asked for, the same again without it.
    irr::IrrlichtDevice* device = 0;
    for (int attempt = 0; attempt < 4 && !device; attempt++) {
        const bool startOnMainScreen = (attempt % 2) == 1;
        const bool withoutAntiAlias = attempt >= 2;
        if (withoutAntiAlias && antiAlias <= 1) { break; } //0 or 1: anti-aliasing already off
#ifdef _WIN32
        const bool movable = fakeFullScreen && (windowX != 0 || windowY != 0); //the main screen is at 0, 0
#else
        const bool movable = false;
#endif
        if (startOnMainScreen && !movable) { continue; }
        if (attempt > 0) {
            std::cerr << "The 3D display did not start. Trying again" << (startOnMainScreen ? ", on the main screen first" : "")
                << (withoutAntiAlias ? ", without anti-aliasing" : "") << "." << std::endl;
        }
        deviceParameters.AntiAlias = withoutAntiAlias ? 0 : antiAlias;
#ifdef _WIN32
        if (fakeFullScreen) {
            if (hWnd) { DestroyWindow(hWnd); } //a window keeps the pixel format of a failed start: a new one each time
            hWnd = CreateWindowA(Win32ClassName, "Simulateur de Navigation Maritime",
                style, startOnMainScreen ? 0 : windowX, startOnMainScreen ? 0 : windowY, graphicsWidth, graphicsHeight,
                NULL, NULL, hInstance, NULL);
            deviceParameters.WindowId = hWnd; //Tell irrlicht about the window to use
        }
#endif
        device = irr::createDeviceEx(deviceParameters);
#ifdef _WIN32
        if (device && startOnMainScreen) {
            SetWindowPos(hWnd, NULL, windowX, windowY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            std::cout << "3D display started on the main screen, then moved to the chosen screen." << std::endl;
        }
#endif
    }

    //On Windows, redirect console stderr to log file
    std::string userLog = userFolder + logName;
    std::cout << "User log file is " << userLog << std::endl;
    /*
    FILE * stream = 0;
    #ifdef _WIN32
    errno_t success = freopen_s(&stream, userLog.c_str(), "w", stderr);
    #endif // _WIN32
    */
    if (device == 0) {
        std::cerr << "Could not start - please check your graphics options." << std::endl;
#ifdef _WIN32
        std::wstring message = L"L'affichage 3D (OpenGL) n'a pas pu d\u00E9marrer.\n\nV\u00E9rifiez le pilote de la carte graphique, "
            L"et les options graphiques du simulateur (graphics_mode, monitor, anti_alias).";
        if (logRedirected) { message += L"\n\nD\u00E9tails : " + CrashReport::widen(CrashReport::tidyPath(userLog)); }
        MessageBoxW(NULL, message.c_str(), L"NAUTITECH - Simulateur", MB_OK | MB_ICONERROR);
#endif
        return(EXIT_FAILURE); //Could not get file system
    }
    device->getCursorControl()->setVisible(true);
    //Start paused initially
    device->getTimer()->setSpeed(0.0);

    device->setWindowCaption(irr::core::stringw(LONGNAME.c_str()).c_str()); //Note: Odd conversion from char* to wchar*!

    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::scene::ISceneManager* smgr = device->getSceneManager();

    std::vector<std::string> logMessages;
    DefaultEventReceiver defReceiver(&logMessages, device);
    device->setEventReceiver(&defReceiver);

    //Tell the Ini routine the logger address
    IniFile::irrlichtLogger = device->getLogger();

    device->getLogger()->log("User folder is:");
    device->getLogger()->log(userFolder.c_str());

    smgr->getParameters()->setAttribute(irr::scene::ALLOW_ZWRITE_ON_TRANSPARENT, true);

#ifdef __APPLE__
    //Bring window to front
    //NSWindow* window = reinterpret_cast<NSWindow>(device->getVideoDriver()->getExposedVideoData().HWnd);
    //Mac OS - cd back to original dir - seems to be changed during createDevice
    irr::io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem == 0) {
        std::cerr << "Could not get filesystem:" << std::endl;
        return(EXIT_FAILURE); //Could not get file system

    }
    fileSystem->changeWorkingDirectoryTo(exeFolderPath.c_str());
#endif
    //icon - kyara 
    device->setWindowCaption(radarStation ? L"Simulateur de Navigation Maritime - Radar" : L"Simulateur de Navigation Maritime");

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

    //set gui skin and 'flatten' this
    irr::gui::IGUISkin* newskin = device->getGUIEnvironment()->createSkin(irr::gui::EGST_WINDOWS_METALLIC);

    device->getGUIEnvironment()->setSkin(newskin);
    newskin->drop();

    irr::u32 su = driver->getScreenSize().Width;
    irr::u32 sh = driver->getScreenSize().Height;

    //set size of camera window, based on actual window
    graphicsWidth = su;
    graphicsHeight = sh;
    irr::u32 graphicsWidth3d = su;
    irr::u32 graphicsHeight3d = sh * viewProportion3D();
    irr::f32 aspect = (irr::f32)su / (irr::f32)sh;
    irr::f32 aspect3d = (irr::f32)graphicsWidth3d / (irr::f32)graphicsHeight3d;

    std::cout << "graphicsWidth: " << graphicsWidth << " graphicsHeight: " << graphicsHeight << std::endl;

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

    //Choose scenario
    std::string scenarioName = "";
    std::string hostname = "";
    //Scenario path - default to user dir if it exists
    std::string scenarioPath = "Scenarios/";
    if (Utilities::pathExists(userFolder + scenarioPath)) {
        scenarioPath = userFolder + scenarioPath;
    }

    //Find default hostname if set in user directory (hostname.txt)
    if (Utilities::pathExists(userFolder + "/hostname.txt")) {
        hostname = IniFile::iniFileToString(userFolder + "/hostname.txt", "hostname");
    }

    //Start sound
    Sound sound;

    OperatingMode::Mode mode = OperatingMode::Normal;
    if (IniFile::iniFileTou32(iniFilename, "secondary_mode") == 1 || radarStation) {
        mode = OperatingMode::Secondary;
    }

    if (mode == OperatingMode::Normal) {
        //Several screens seen as one (Surround / Eyefinity): the exercise choice on the middle one only.
        //bc5.ini menu_screens: 0 = from the window's shape, 1 = the whole window, 3 = three screens.
        centre::state().forcedScreens = (int)IniFile::iniFileTou32(iniFilename, "menu_screens");
        ScenarioChoice scenarioChoice(device, &language, fontName, modifier == "fr");
        scenarioChoice.chooseScenario(scenarioName, hostname, udpPort, mode, scenarioPath);
    }

    hostname = Utilities::trim(hostname);

    //Save hostname in user directory (hostname.txt). (The directory was made at start-up.) Not by the
    //radar station, which would replace the simulator's own with nothing.
    if (!radarStation && Utilities::pathExists(userFolder)) { //TODO: Should we make this if it doesn't exist?
        std::string hostnameFile = userFolder + "/hostname.txt";
        std::ofstream file(hostnameFile.c_str());
        if (file.is_open()) {
            file << "hostname=" << hostname << std::endl;
            file.close();
        }
    }
    //KYARA CHARGEMENT: real loading page. The old code showed media/background.png for a FIXED
    //3 seconds BEFORE anything was loaded (pure waiting), then left a frozen frame on screen during
    //the real loading. The page below is redrawn between each loading stage instead, and every
    //stage's duration is written to the log as "[CHARGEMENT] <stage> : <n> ms".
    irr::gui::IGUIFont* loadingTitleFont = 0;
    {
        //Bigger version of the same font for the scenario title, if one exists in media/fonts
        const int titleSizes[3] = { fontSize * 2, fontSize + 10, fontSize + 6 };
        for (int i = 0; i < 3 && !loadingTitleFont; i++) {
            std::string titlePath = "media/fonts/" + fontName + "/" + fontName + "-" + std::to_string(titleSizes[i]) + ".xml";
            if (device->getFileSystem()->existFile(titlePath.c_str())) {
                loadingTitleFont = device->getGUIEnvironment()->getFont(titlePath.c_str());
            }
        }
    }
    LoadingScreen loadingScreen(device, font, loadingTitleFont);
    loadingScreen.setStage(0.02f, "Connexion au réseau");


    /*Show loading message
    irr::u32 creditsStartTime = device->getTimer()->getRealTime();
    irr::core::stringw creditsText = language.translate("loadingmsg");
    creditsText.append(L"\n\n");
    //creditsText.append(getCredits());
    irr::gui::IGUIStaticText* loadingMessage = device->getGUIEnvironment()->addStaticText(creditsText.c_str(), irr::core::rect<irr::s32>(0.05*su,0.05*sh,0.95*su,0.95*sh),true);
    device->run();
    driver->beginScene(irr::video::ECBF_COLOR|irr::video::ECBF_DEPTH, irr::video::SColor(0,200,200,200));
    device->getGUIEnvironment()->drawAll();
    driver->endScene();

    //seed random number generator
    std::srand(device->getTimer()->getTime());*/

    //create GUI
    //The device is let go only after the GUI and the model below are destroyed (objects go in the
    //reverse order). Dropped earlier, as it was, their destructors removed fire, monitor and GUI
    //elements the device had already freed: a crash on quitting after a fire exercise.
    struct DeviceRelease
    {
        irr::IrrlichtDevice* device;
        ~DeviceRelease() { if (device) { device->drop(); } }
    } deviceRelease = { device };

    GUIMain guiMain;
    guiMain.setInstanceNumber(instanceNumber);
#ifdef _WIN32
    //Instrument console on a screen of its own: launcher -console N (0: in the bridge view), else bc5.ini
    //console_monitor=N. Never on the bridge view's own screen.
    {
        int consoleMonitor = (int)IniFile::iniFileTou32(iniFilename, "console_monitor") - 1;
        for (int arg = 1; arg + 1 < argc; arg++) {
            if (strcmp(argv[arg], "-console") == 0) {
                consoleMonitor = atoi(argv[arg + 1]) - 1;
            }
        }
        if (consoleMonitor >= 0) {
            cMonitorsVec monitors;
            const HWND mainWindow = (HWND)driver->getExposedVideoData().OpenGLWin32.HWnd;
            const HMONITOR bridgeMonitor = MonitorFromWindow(mainWindow, MONITOR_DEFAULTTONEAREST);
            if (consoleMonitor < (int)monitors.hMonitors.size() && monitors.hMonitors[consoleMonitor] != bridgeMonitor) {
                const RECT& r = monitors.rcMonitors[consoleMonitor];
                guiMain.setConsoleScreen(irr::core::rect<irr::s32>(r.left, r.top, r.right, r.bottom));
                std::cout << "Instrument console on screen " << consoleMonitor + 1 << std::endl;
            }
            else {
                std::cerr << "Instrument console: screen " << consoleMonitor + 1 << " is not connected, or shows the bridge view. Console kept in the bridge view." << std::endl;
            }
        }
    }
#endif

    //Set up networking (this will get a pointer to the model later)
    //Create networking, linked to model, choosing whether to use main or secondary network mode
    Network* network = Network::createNetwork(mode, udpPort, device);
    //Network network(&model);

    //The large radar on another screen: a radar station started now, so that it loads while this
    //simulator does, and added to the displays this one sends its data to. Its port follows the one
    //this simulator really listens on (another copy on this PC may have taken udp_send_port).
    std::string displayHosts = hostname;
    if (radarScreen > 0 && (mode == OperatingMode::Normal || mode == OperatingMode::Multiplayer)) {
        irr::u32 radarPort = IniFile::iniFileTou32(iniFilename, "radar_station_port");
        if (radarPort == 0) { radarPort = (network->getPort() > 0 ? (irr::u32)network->getPort() : udpPort) + 100; }
        radarPort = freeUdpPort(radarPort);
        bool screenThere = true;
#ifdef _WIN32
        cMonitorsVec screens;
        screenThere = radarScreen <= (int)screens.iMonitors.size();
#endif
        if (!screenThere) {
            std::cerr << "Radar on screen " << radarScreen << ": that screen is not connected." << std::endl;
        }
        else if (startRadarStation(radarScreen, radarPort, iniArgument)) {
            const std::string radarHost = "127.0.0.1:" + std::to_string(radarPort);
            displayHosts = displayHosts.empty() ? radarHost : displayHosts + "," + radarHost;
            std::cout << "Radar station started on screen " << radarScreen << ", port " << radarPort << "." << std::endl;
        }
    }

    network->connectToServer(mode == OperatingMode::Normal ? displayHosts : hostname);

    // If in multiplayer mode, also start 'normal' network, so we can send data to secondary displays
    // (the radar station among them)
    Network* extraNetwork = 0;
    if ((mode == OperatingMode::Multiplayer) && (displayHosts.length() > 0)) {
        extraNetwork = Network::createNetwork(OperatingMode::Normal, udpPort, device);
        extraNetwork->connectToServer(displayHosts);
        //std::cout << "Starting extra network to " << hostname << " on " << udpPort << std::endl;
    }

    //Read in scenario data (work in progress)
    ScenarioData scenarioData;
    if (mode == OperatingMode::Normal) {
        scenarioData = Utilities::getScenarioDataFromFile(scenarioPath + scenarioName, scenarioName);
    }
    else {
        //If in secondary mode, get scenario information from the server
        //Tell user what we're doing
        irr::core::stringw portMessage = language.translate("secondaryWait");
        portMessage.append(L" ");
        std::string ourHostName = asio::ip::host_name();
        portMessage.append(irr::core::stringw(ourHostName.c_str()));
        portMessage.append(L":");
        portMessage.append(irr::core::stringw(network->getPort()));
        //KYARA CHARGEMENT: the grey frame that used to be drawn here is gone - the loading page covers it.
        std::cout << irr::core::stringc(portMessage.c_str()).c_str() << std::endl;
        //Get the data
        std::string receivedSerialisedScenarioData;
        loadingScreen.setStage(0.03f, "En attente des données du poste principal");
        while (device->run() && receivedSerialisedScenarioData.empty()) {
            network->getScenarioFromNetwork(receivedSerialisedScenarioData);
            loadingScreen.refresh(); //KYARA CHARGEMENT: keep the page alive while waiting
        }
        scenarioData.deserialise(receivedSerialisedScenarioData);
    }
    std::string serialisedScenarioData = scenarioData.serialise(false);

    //KYARA CHARGEMENT: scenario card on the loading page
    {
        loadingScreen.setTitle(scenarioData.scenarioName);
        loadingScreen.addInfo("Zone", scenarioData.worldName);
        loadingScreen.addInfo("Navire", scenarioData.ownShipData.ownShipName);
        {
            irr::f32 t = scenarioData.startTime;
            int hh = (int)t;
            int mm = (int)((t - hh) * 60.0f + 0.5f);
            if (mm >= 60) { mm -= 60; hh += 1; }
            char buf[64];
            snprintf(buf, sizeof(buf), "%02d:%02d  -  %02u/%02u/%04u", hh % 24, mm,
                (unsigned)scenarioData.startDay, (unsigned)scenarioData.startMonth, (unsigned)scenarioData.startYear);
            loadingScreen.addInfo("Départ", buf);
        }
        {
            irr::f32 seaState = scenarioData.weather;
            if (seaState > SIM_MAX_WEATHER) { seaState = SIM_MAX_WEATHER; } //same cap as the model applies
            char buf[64];
            snprintf(buf, sizeof(buf), "%.1f", seaState);
            loadingScreen.addInfo("État de mer", buf);
            snprintf(buf, sizeof(buf), "%03d°  %.0f nd", (int)(scenarioData.windDirection + 0.5f) % 360, scenarioData.windSpeed);
            loadingScreen.addInfo("Vent", buf);
            if (scenarioData.visibilityRange > 0) {
                snprintf(buf, sizeof(buf), "%.1f NM", scenarioData.visibilityRange);
                loadingScreen.addInfo("Visibilité", buf);
            }
        }
        if (!scenarioData.otherShipsData.empty()) {
            loadingScreen.addInfo("Autres navires", std::to_string(scenarioData.otherShipsData.size()));
        }
        //Briefing: the scenario's description.ini, if there is one (primary only - the secondary
        //does not have the scenario folder)
        if (mode == OperatingMode::Normal) {
            std::ifstream descFile((scenarioPath + scenarioName + "/description.ini").c_str());
            if (descFile.is_open()) {
                std::stringstream descBuffer;
                descBuffer << descFile.rdbuf();
                loadingScreen.setBriefing(descBuffer.str());
            }
        }
        loadingScreen.setStage(0.05f, "Préparation de l'exercice");
    }

    //Note: We could use this serialised format as a scenario import/export format or for online distribution

    // Check VR mode
    bool vr3dMode = false;
    if (IniFile::iniFileTou32(iniFilename, "vr_mode") == 1) {
        vr3dMode = true;
    }

    // Set up the VR interface
    VRInterface vrInterface(device, device->getSceneManager(), device->getVideoDriver(), su, sh);

    bool secondaryControlWheel = false;
    bool secondaryControlPortEngine = false;
    bool secondaryControlStbdEngine = false;
    bool secondaryControlPortSchottel = false;
    bool secondaryControlStbdSchottel = false;
    bool secondaryControlPortThrustLever = false;
    bool secondaryControlStbdThrustLever = false;
    bool secondaryControlBowThruster = false;
    bool secondaryControlSternThruster = false;
    if (mode == OperatingMode::Secondary) {
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_wheel") == 1) {
            secondaryControlWheel = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_port_engine") == 1) {
            secondaryControlPortEngine = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_stbd_engine") == 1) {
            secondaryControlStbdEngine = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_port_schottel") == 1) {
            secondaryControlPortSchottel = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_stbd_schottel") == 1) {
            secondaryControlStbdSchottel = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_port_thrust") == 1) {
            secondaryControlPortThrustLever = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_stbd_thrust") == 1) {
            secondaryControlStbdThrustLever = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_bow_thruster") == 1) {
            secondaryControlBowThruster = true;
        }
        if (IniFile::iniFileTou32(iniFilename, "secondary_control_stern_thruster") == 1) {
            secondaryControlSternThruster = true;
        }

    }

    SimulationModel::ModelParameters modelParameters;
    // TODO: most of these intermediate variables can probably be removed.
    modelParameters.mode = mode;
    modelParameters.vrMode = vr3dMode;
    modelParameters.reflectionMode = reflectionMode;
    modelParameters.viewAngle = viewAngle;
    modelParameters.lookAngle = lookAngle;
    modelParameters.cameraMinDistance = cameraMinDistance;
    modelParameters.cameraMaxDistance = cameraMaxDistance;
    modelParameters.disableShaders = disableShaders;
    modelParameters.waterSegments = waterSegments;
    modelParameters.numberOfContactPoints = numberOfContactPoints;
    modelParameters.minContactPointSpacing = minContactPointSpacing;
    modelParameters.contactStiffnessFactor = contactStiffnessFactor;
    modelParameters.contactDampingFactor = contactDampingFactor;
    modelParameters.frictionCoefficient = frictionCoefficient;
    modelParameters.tanhFrictionFactor = tanhFrictionFactor;
    modelParameters.lineStiffnessFactor = lineStiffnessFactor;
    modelParameters.lineDampingFactor = lineDampingFactor;
    modelParameters.limitTerrainResolution = limitTerrainResolution;
    modelParameters.secondaryControlWheel = secondaryControlWheel;
    modelParameters.secondaryControlPortEngine = secondaryControlPortEngine;
    modelParameters.secondaryControlStbdEngine = secondaryControlStbdEngine;
    modelParameters.secondaryControlPortSchottel = secondaryControlPortSchottel;
    modelParameters.secondaryControlStbdSchottel = secondaryControlStbdSchottel;
    modelParameters.secondaryControlPortThrustLever = secondaryControlPortThrustLever;
    modelParameters.secondaryControlStbdThrustLever = secondaryControlStbdThrustLever;
    modelParameters.secondaryControlBowThruster = secondaryControlBowThruster;
    modelParameters.secondaryControlSternThruster = secondaryControlSternThruster;
    modelParameters.debugMode = debugMode;

    //Create simulation model
    SimulationModel model(device,
        smgr,
        &guiMain,
        &sound,
        scenarioData,
        modelParameters,
        &loadingScreen); //KYARA CHARGEMENT
    model.setTripleScreen(triScreen, perScreenFOV, bezelYaw);
    //check enough time has elapsed to show the credits screen (5s)
    //while (device->getTimer()->getRealTime() - creditsStartTime < 5000) {
       //device->run();
    //}

    //KYARA CHARGEMENT: the world readme used to be "shown" here by drawing a grey frame over the
    //loading page (the text itself was already disabled). Removed; the readme stays available
    //through model.getWorldReadme() if you want it on the loading page later.

    // Remove loading message, as not needed again
    //loadingMessage->remove(); loadingMessage = 0;


    // Load the VR interface, allowing link to model
    int vrSuccess = -1;
    if (vr3dMode) {
        vrSuccess = vrInterface.load(&model);
        std::cout << "vrSuccess=" << vrSuccess << std::endl;
    }

    //Load the gui
    bool hideEngineAndRudder = false;
    // Hide engine/wheel inputs if not used (todo: control individually?)
    if (mode == OperatingMode::Secondary) {
        if (secondaryControlWheel ||
            secondaryControlPortEngine ||
            secondaryControlStbdEngine ||
            secondaryControlPortSchottel ||
            secondaryControlStbdSchottel ||
            secondaryControlPortThrustLever ||
            secondaryControlStbdThrustLever ||
            secondaryControlBowThruster ||
            secondaryControlSternThruster) {
            hideEngineAndRudder = false;
        }
        else {
            hideEngineAndRudder = true;
        }
    }

    loadingScreen.setStage(0.86f, "Interface et instruments");
    //Instructor tools: 0 in bc5.ini hides them from trainees
    guiMain.setInstructorTools(IniFile::iniFileTou32(iniFilename, "show_colreg_tab", 1) == 1,
                               IniFile::iniFileTou32(iniFilename, "show_size_tool", 1) == 1,
                               IniFile::iniFileTou32(iniFilename, "show_instrument_tool", 1) == 1);
    guiMain.load(device, &language, &logMessages, &model, model.isSingleEngine(), model.isAzimuthDrive(), hideEngineAndRudder, model.hasDepthSounder(), model.getMaxSounderDepth(), model.hasGPS(), showTideHeight, model.hasBowThruster(), model.hasSternThruster(), model.hasTurnIndicator(), showCollided, vr3dMode);
    //The console layout decides how much of the screen the 3D view gets (GUIMain::load)
    graphicsHeight3d = sh * viewProportion3D();
    aspect3d = (irr::f32)graphicsWidth3d / (irr::f32)graphicsHeight3d;

    //Give the network class a pointer to the model
    network->setModel(&model);
    if (extraNetwork) {
        extraNetwork->setModel(&model);
    }

    //load realistic water
    //RealisticWaterSceneNode* realisticWater = new RealisticWaterSceneNode(smgr, 4000, 4000, "./",irr::core::dimension2du(512, 512),smgr->getRootSceneNode());

    //load joystick setup
    JoystickSetup joystickSetup = getJoystickSetup(iniFilename, model.isAzimuthDrive());

    //create event receiver, linked to model
    MyEventReceiver receiver(device, &model, &guiMain, network, &vrInterface, joystickSetup, &logMessages);
    device->setEventReceiver(&receiver);

    //create NMEA serial port and UDP, linked to model
    NMEA nmea(&model, nmeaSerialPortName, nmeaSerialPortBaudrate, nmeaUDPAddressName, nmeaUDPPortName, nmeaUDPListenPortName, device);

    //Load sound files


    //KYARA CHARGEMENT: the sound files used to be loaded TWICE (this block existed here and again
    //after the radar setup below). Only the second copy is kept.

    //Set up initial options
    if (IniFile::iniFileTou32(iniFilename, "hide_instruments") == 1) {
        guiMain.hide2dInterface();
    }
    if (IniFile::iniFileTou32(iniFilename, "full_radar") == 1 || radarStation) {
        guiMain.setLargeRadar(true);
        model.setRadarDisplayRadius(guiMain.getRadarPixelRadius());
        guiMain.hide2dInterface();
        if (mode == OperatingMode::Secondary) {
            model.setSecondaryRadarMaster(true); //kyara: this secondary is THE radar station
        }
    }
    //kyara: designate this secondary as the radar station via EITHER full_radar=1 OR the
    //dedicated key secondary_radar_master=1 in THIS instance's ini file. Only the radar
    //station sends radar sync to the primary; view-angle secondaries must not have either key.
    if (mode == OperatingMode::Secondary && (radarStation || IniFile::iniFileTou32(iniFilename, "full_radar") == 1 || IniFile::iniFileTou32(iniFilename, "secondary_radar_master") == 1)) {
        model.setSecondaryRadarMaster(true);
        std::cout << "KYARA: this secondary is the RADAR MASTER - radar sync enabled" << std::endl;
    }
    if (IniFile::iniFileTou32(iniFilename, "arpa_on") == 1) {
        guiMain.setARPAComboboxes(2); // 0: Off/Manual, 1: MARPA, 2: ARPA
        model.setArpaMode(2);
    }
    irr::u32 radarStartupMode = IniFile::iniFileTou32(iniFilename, "radar_mode");
    if (radarStartupMode == 1) {
        model.setRadarCourseUp();
    }
    if (radarStartupMode == 2) {
        model.setRadarHeadUp();
    }
    loadingScreen.setStage(0.93f, "Sons");
    if (mode != OperatingMode::Secondary) {
        sound.load(model.getOwnShipEngineSound(), model.getOwnShipWaveSound(), model.getOwnShipHornSound(),
            model.getOwnShipAlarmSound(), model.getOwnShipProxyAlarmSound(), model.getOwnShipCollisionSound(),
            model.getOwnShipInsideSound(), model.getOwnShipOutsideSound(), model.getOwnShipSeagullSound(),
            model.getOwnShipContactSound(), model.getOwnShipRadarAlarmSound(),
            model.getOwnShipRainSound(), model.getOwnShipStormSound(), model.getOwnShipThunderSound(), model.getOwnShipThunderboltSound(), model.getOwnShipFireBurningSound(), model.getOwnShipWaterSound(), model.getOwnShipFireAlarmSound(),
            model.getOwnShipAbandonAlarmSound(), model.getOwnShipExplosionSound(), model.getOwnShipGroanSound(), model.getOwnShipSteamSound(), model.getOwnShipVhfSound());
        sound.setVolumeWave(IniFile::iniFileTof32(iniFilename, "wave_volume"));
        //KYARA SLAM: hull coming down on the water. Optional file; bc5.ini slam_volume (default 1).
        sound.loadSlamSound(model.getOwnShipSlamSound());
        //KYARA SLAM: water on the wheelhouse glass. bc5.ini screen_spray: 0 off, 1 normal (default),
        //2 test - water after every landing, for checking the effect.
        {
            std::string screenSprayMode = IniFile::iniFileToString(iniFilename, "screen_spray");
            model.setScreenSprayMode(screenSprayMode.empty() ? 1 : atoi(screenSprayMode.c_str()));
        }
        {
            irr::f32 slamVol = IniFile::iniFileTof32(iniFilename, "slam_volume");
            if (slamVol <= 0.0f) { slamVol = 1.0f; }
            sound.setVolumeSlam(slamVol);
        }
    }

    //set up timing for NMEA

//    Profiling
//    Profiler networkProfile("Network");
//    Profiler nmeaProfile("NMEA");
//    Profiler modelProfile("Model");
//    Profiler renderSetupProfile("Render setup");
//    Profiler renderRadarProfile("Render radar");
//    Profiler renderProfile("3d render");
//    Profiler guiProfile("GUI render");
//    Profiler renderFinishProfile("Render finish");

    sound.StartSound();

    //KYARA CHARGEMENT: done - no "click to start" page any more (the key mapping is in the
    //launcher). The primary starts the clock itself; a secondary keeps following the primary's
    //clock over the network, so it is left alone.
    loadingScreen.finish();
    if (mode != OperatingMode::Secondary) {
        model.setAccelerator(1.0);
    }

    //main loop
    while (device->run())
    {

        {
            IPROF("Network");
            //        networkProfile.tic();
            network->update();
            if (extraNetwork) {
                extraNetwork->update();
            }
            //        networkProfile.toc();

                    // Update NMEA, check if new sensor or AIS data is ready to be sent
            //        nmeaProfile.tic();
        } {
            IPROF("NMEA");

            if (!nmeaUDPListenPortName.empty()) {
                nmea.receive();
            }

            if (!nmeaSerialPortName.empty() || (!nmeaUDPAddressName.empty() && !nmeaUDPPortName.empty())) {
                nmea.updateNMEA();

                if (!nmeaSerialPortName.empty()) {
                    nmea.sendNMEASerial();
                }

                if (!nmeaUDPAddressName.empty() && !nmeaUDPPortName.empty()) {
                    nmea.sendNMEAUDP();
                }

                nmea.clearQueue();
            }
            //        nmeaProfile.toc();

            //        modelProfile.tic();
        } {
            IPROF("Render setup");

            driver->setViewPort(irr::core::rect<irr::s32>(0, 0, graphicsWidth, graphicsHeight)); //Full screen before beginScene
            //KYARA Changes color 
            driver->setViewPort(irr::core::rect<irr::s32>(0, 0, graphicsWidth, graphicsHeight)); //Full screen before beginScene
            driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, irr::video::SColor(255, 0, 50, 20), 1.0f, 0, guiMain.getMainVideoData());//        renderSetupProfile.toc();

            //        renderRadarProfile.tic();

        }
        bool fullScreenRadar = guiMain.getLargeRadar();
        {
            IPROF("Render radar");
            if (model.isRadarOn()) {
                //radar view portion
                //KYARA: with the instrument console there is no small radar to render - that saves a full
                //smgr->drawAll() every frame. The radar calculation itself still runs in model.update().
                if (graphicsHeight > graphicsHeight3d && ((guiMain.getShowInterface() && guiMain.getSmallRadarEnabled()) || fullScreenRadar)) {
                    model.setWaterVisible(false); //Hide the reflecting water, as this updates itself on drawAll()
                    if (fullScreenRadar) {
                        driver->setViewPort(guiMain.getLargeRadarRect());
                    }
                    else {
                        driver->setViewPort(guiMain.getSmallRadarRect());
                    }
                    model.setRadarCameraActive();
                    smgr->drawAll();
                    model.setWaterVisible(true); //Re-show the water
                }
            }

            //       renderRadarProfile.toc();

            //       renderProfile.tic();
        } {
            IPROF("Render");

            //3d view portion
            model.setMainCameraActive(); //Note that the NavLights expect the main camera to be active, so they know where they're being viewed from

            // Normal rendering
            if (!fullScreenRadar) {
                if (triScreen) {
                    //NAUTITECH triple-screen: split the 3D area into three columns, one per TV.
                    irr::s32 baseW = guiMain.getCompact3dView() ? (irr::s32)graphicsWidth3d : (irr::s32)graphicsWidth;
                    irr::s32 baseH = guiMain.getCompact3dView() ? (irr::s32)graphicsHeight3d : (irr::s32)graphicsHeight;
                    irr::s32 colW = baseW / 3;
                    irr::f32 colAspect = (irr::f32)colW / (irr::f32)baseH;
                    irr::f32 yaw[3] = { -bezelYaw, 0.0f, bezelYaw }; // left(port), centre, right(stbd)
                    for (int c = 0; c < 3; ++c) {
                        //With the console on the middle screen only, the side screens show the view to the
                        //bottom: same scale, horizon raised to where the middle screen has it.
                        const bool fullSide = (c != 1) && guiMain.getCompact3dView() && guiMain.getSideScreensFree();
                        const irr::s32 colH = fullSide ? (irr::s32)graphicsHeight : baseH;
                        driver->setViewPort(irr::core::rect<irr::s32>(c * colW, 0, (c + 1) * colW, colH));
                        const irr::f32 shiftY = fullSide ? (irr::f32)(colH - baseH) / (irr::f32)colH : 0.0f;
                        model.renderMainColumn(fullSide ? (irr::f32)colW / (irr::f32)colH : colAspect, perScreenFOV, yaw[c], shiftY);
                        smgr->drawAll();
                    }
                    //Restore the full-surface viewport so the GUI/overlays draw across all screens
                    driver->setViewPort(irr::core::rect<irr::s32>(0, 0, graphicsWidth, graphicsHeight));
                }
                else {
                    if (guiMain.getCompact3dView()) {
                        driver->setViewPort(irr::core::rect<irr::s32>(0, 0, graphicsWidth3d, graphicsHeight3d));
                        model.updateViewport(aspect3d);
                    }
                    else {
                        driver->setViewPort(irr::core::rect<irr::s32>(0, 0, graphicsWidth, graphicsHeight));
                        model.updateViewport(aspect);
                    }

                    //if (guiMain.getShow3d()) {
                    smgr->drawAll();
                    //}
                }
            }

            if (vr3dMode && vrSuccess == 0) {

                // Set aspect ratio
                irr::f32 aspectRatioVR = vrInterface.getAspectRatio();
                model.updateViewport(aspectRatioVR);

                // Process events
                int runtimeEventSuccess = vrInterface.runtimeEvents(); // TODO: Use return value here, e.g. to trigger close?

                // Render and get inputs from VR
                if (runtimeEventSuccess == 0) {
                    vrInterface.update();
                }
            }

            //       renderProfile.toc();

            //       guiProfile.tic();
        } {
            IPROF("Model");
            model.update();
            //        modelProfile.toc();


                    //Set up

            //        renderSetupProfile.tic();
        } {
            IPROF("GUI");
            //gui
            driver->setViewPort(irr::core::rect<irr::s32>(0, 0, 10, 10));//Set to a dummy value first to force the next call to make the change
            driver->setViewPort(irr::core::rect<irr::s32>(0, 0, graphicsWidth, graphicsHeight)); //Full screen for gui
            guiMain.drawGUI();

            //       guiProfile.toc();
        } {
            IPROF("End scene");
            //       renderFinishProfile.tic();
            driver->endScene();
            //       renderFinishProfile.toc();
        }
        //Instrument console in its own window (if detached): its input, then its frame.
        guiMain.renderDetachedConsole();

        //KYARA FRAME PACING --------------------------------------------------------------------
      //Removing the 10ms enet block gave the frames back, but that block was also accidentally
      //acting as a frame limiter. Now the frame time swings - the reflection RTT only runs on
      //every 2nd frame, so odd frames are cheap and even frames are dear. Uneven frame TIMES are
      //what you see as jitter even when the average FPS is high, because the ship advances by
      //deltaTime each frame and so moves a long step, then a short step, then a long step.
      //Pinning the frame to a fixed budget makes the motion even.
      //16667us = 60fps. For the TVs use 1000000/refreshRate. Set to 0 to disable.
        {
            //const long long TARGET_FRAME_US = 16667;
            const long long TARGET_FRAME_US = 16667; //KYARA: vsync handles pacing on the 60Hz TVs
            if (TARGET_FRAME_US > 0) {
                static std::chrono::steady_clock::time_point nextFrame = std::chrono::steady_clock::now();
                nextFrame += std::chrono::microseconds(TARGET_FRAME_US);
                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                if (now < nextFrame) {
                    std::this_thread::sleep_until(nextFrame);
                }
                else {
                    nextFrame = now; //Behind budget - re-base rather than trying to catch up,
                    //otherwise one slow frame causes a burst of zero-sleep frames.
                }
            }
        }

    }

#ifdef WITH_PROFILING
    InternalProfiler::aggregateEntries();

    std::cout << "\nThe profiler stats so far:\n"
        "WHAT: AVG_TIME (TOTAL_TIME / TIMES_EXECUTED)"
        "\nAll times in micro seconds\n"
        << InternalProfiler::stats << std::endl;
#endif

    //Remember where the console window was, and close it while the device is still alive.
    guiMain.shutdownConsoleWindow();

    //networking should be stopped (presumably with destructor when it goes out of scope?)
    device->getLogger()->log("About to stop network");
    delete network;
    if (extraNetwork) {
        delete extraNetwork;
    }

    // Close down OpenXR and clean up
    if (vr3dMode && vrSuccess == 0) {
        vrInterface.unload();
        std::cout << "Unloaded OpenXR" << std::endl;
    }

    //(the device is dropped by deviceRelease, after guiMain and model)

    scriptToExe = IniFile::iniFileToString(iniFilename, "script_stop_BC");
    if (!scriptToExe.empty()) {
        std::string scriptPath;
        if (Utilities::pathExists(userFolder + scriptToExe)) {
            scriptPath = userFolder + scriptToExe;
        }
        else {
#ifdef _WIN32
            scriptPath = "Scripts\\win\\" + scriptToExe;
#else
#ifdef __APPLE__
            scriptPath = "./Scripts/macOS/" + scriptToExe;
#else
            scriptPath = "./Scripts/linux/" + scriptToExe;
#endif
#endif    
        }

        std::cout << "Going to run " << scriptPath << std::endl;

#ifdef _WIN32
        ShellExecute(NULL, "open", scriptPath.c_str(), NULL, NULL, SW_MINIMIZE);
#else
        scriptPath = "\"" + scriptPath + "\"";
        system(scriptPath.c_str());
#endif
    }

    //Save log messages out
    //Note that stderr has also been redirected to this file on windows, so it will contain anything from cerr, as well as these log messages
    //Save log messages to user directory, into log.txt, overwrite old file with that name
    std::ofstream logFile;
    /*
    if (stream) {
        fclose(stream);
        logFile.open(userLog, std::ofstream::app); //Append
    }
    else {
    */
    logFile.open(userLog, logRedirected ? std::ofstream::app : std::ofstream::out); //After what was printed, or overwrite
    /*
    }
    */


    for (unsigned int i = 0; i < logMessages.size(); i++) {
        if (logFile.good()) {
            //Check we're not creating an excessively long file
            if (i <= 1000 && logMessages.at(i).length() <= 1000) {
                logFile << "Log: " << logMessages.at(i) << std::endl;
            }
        }
    }

    //End
    return(0);
}