#pragma once

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <string>
#include <vector>
#include <cstdint>

#pragma comment(lib, "pdh.lib")


struct SpeedInfo {
    double value;
    std::string unit;
};

// Returns true on success, false on any PDH error.
bool GetNetworkSpeedsPDH(double& outDownloadMBps,
    double& outUploadMBps);

SpeedInfo ConvertSpeed(double bytesPerSec);