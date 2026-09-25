#include "simple_license.h"

#include <windows.h>
#include <winioctl.h>

#include <cctype>
#include <fstream>
#include <vector>

// ---------------------------------------------------------------------------
// Read the controller serial burned into the USB device on `driveLetter`.
// This is the hardware serial (survives reformatting), not the volume serial.
// No admin rights needed: handles are opened with 0 (query-only) access.
// ---------------------------------------------------------------------------
std::string GetUsbSerialForDrive(char driveLetter)
{
    std::string volPath = std::string("\\\\.\\") + driveLetter + ":";
    HANDLE hVol = CreateFileA(volPath.c_str(), 0,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) return "";

    VOLUME_DISK_EXTENTS extents = {};
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                              NULL, 0, &extents, sizeof(extents), &bytes, NULL);
    CloseHandle(hVol);
    if (!ok || extents.NumberOfDiskExtents == 0) return "";

    std::string physPath =
        "\\\\.\\PhysicalDrive" + std::to_string(extents.Extents[0].DiskNumber);
    HANDLE hDisk = CreateFileA(physPath.c_str(), 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) return "";

    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType  = PropertyStandardQuery;

    BYTE buffer[1024] = {};
    ok = DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                         &query, sizeof(query),
                         buffer, sizeof(buffer), &bytes, NULL);
    CloseHandle(hDisk);
    if (!ok) return "";

    auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);
    if (desc->BusType != BusTypeUsb) return "";       // real USB only
    if (desc->SerialNumberOffset == 0) return "";

    std::string serial(reinterpret_cast<char*>(buffer) + desc->SerialNumberOffset);
    size_t a = serial.find_first_not_of(" \t");
    size_t b = serial.find_last_not_of(" \t");
    if (a == std::string::npos) return "";
    return serial.substr(a, b - a + 1);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string ExeDir()
{
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf, n);
    size_t slash = p.find_last_of("\\/");
    return (slash == std::string::npos) ? "." : p.substr(0, slash);
}

static bool IEquals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i]))
            return false;
    return true;
}

// Reads authorized_keys.txt (next to the .exe): one serial per line,
// blank lines and lines starting with '#' are ignored.
static std::vector<std::string> LoadAuthorizedSerials()
{
    std::vector<std::string> out;
    std::ifstream f(ExeDir() + "\\authorized_keys.txt");
    std::string line;
    while (std::getline(f, line))
    {
        size_t a = line.find_first_not_of(" \t\r\n");
        size_t b = line.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        std::string s = line.substr(a, b - a + 1);
        if (s.empty() || s[0] == '#') continue;
        out.push_back(s);
    }
    return out;
}

// ---------------------------------------------------------------------------
// The check the simulator calls at startup.
// ---------------------------------------------------------------------------
bool UsbKeyPresent(std::string* outReason)
{
    std::vector<std::string> allowed = LoadAuthorizedSerials();
    if (allowed.empty())
    {
        if (outReason) *outReason = "liste des cles introuvable (authorized_keys.txt)";
        return false;
    }

    DWORD mask = GetLogicalDrives();
    for (char c = 'A'; c <= 'Z'; ++c)
    {
        if (!(mask & (1u << (c - 'A')))) continue;

        std::string root = std::string(1, c) + ":\\";
        if (GetDriveTypeA(root.c_str()) != DRIVE_REMOVABLE) continue;

        std::string serial = GetUsbSerialForDrive(c);
        if (serial.empty()) continue;

        for (const auto& a : allowed)
            if (IEquals(a, serial)) return true;   // authorised stick found
    }

    if (outReason) *outReason = "cle USB non trouvee";
    return false;
}
