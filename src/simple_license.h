#pragma once
#include <string>

// Returns the USB hardware serial for a drive letter (e.g. 'E'),
// or "" if that drive isn't a readable USB device.
std::string GetUsbSerialForDrive(char driveLetter);

// Returns true if one of the authorised USB sticks is currently plugged in.
// Authorised serials are listed in authorized_keys.txt next to the .exe.
// On failure, outReason (optional) holds a short message for the user.
bool UsbKeyPresent(std::string* outReason = nullptr);
