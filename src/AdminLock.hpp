/*   NAUTITECH - Simulateur de Navigation
     Administrator password for the settings tools (simulator, map and multiplayer settings).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __ADMINLOCK_HPP_INCLUDED__
#define __ADMINLOCK_HPP_INCLUDED__

#include <string>

//The password is never stored: only a salted PBKDF2-HMAC-SHA256 hash of it, in admin.lock in the user
//folder (next to the user's bc5.ini). Until an administrator sets a password, or if that file is
//deleted, the built-in default password applies - so removing the file does not open the settings.
//
//This keeps trainees out of the settings tools. It does not protect the .ini files themselves, which
//anyone with access to the folder can still open in a text editor; Windows account permissions do that.
namespace AdminLock {
    bool check(const std::wstring& password);
    bool change(const std::wstring& newPassword); //false if it could not be saved
    bool usingDefault();                          //true while no password has been set here

    //PBKDF2-HMAC-SHA256, lower-case hex (exposed for tests).
    std::string pbkdf2Hex(const std::string& password, const std::string& salt, unsigned iterations, unsigned bytes);
}

#endif
