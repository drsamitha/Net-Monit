#include "NetworkMonitor.hpp"

#include <thread>


// Expand a wildcard counter path into all matching instance paths.
// e.g. "\\Network Interface(*)\\Bytes Received/sec" → 
//       ["\\Network Interface(eth0)\\Bytes Received/sec", ...]
static bool ExpandWildcard(
    const std::wstring& wildcardPath,
    std::vector<std::wstring>& outPaths)
{
    DWORD required = 0;
    // first call to get buffer size
    PdhExpandWildCardPathW(
        nullptr,
        wildcardPath.c_str(),
        nullptr,
        &required,
        0
    );
    if (required == 0) return false;

    std::vector<wchar_t> buffer(required);
    if (PdhExpandWildCardPathW(
        nullptr,
        wildcardPath.c_str(),
        buffer.data(),
        &required,
        0
    ) != ERROR_SUCCESS)
    {
        return false;
    }

    // buffer is MULTI_SZ (null-separated, ending with double-null)
    wchar_t* p = buffer.data();
    while (*p) {
        outPaths.emplace_back(p);
        p += wcslen(p) + 1;
    }
    return !outPaths.empty();
}



// Convert bytes/s to appropriate unit
SpeedInfo ConvertSpeed(double bytesPerSec) {
    const double KB = 1024.0;
    const double MB = KB * 1024.0;

    if (bytesPerSec >= MB) {
        return { bytesPerSec / MB, "MB/s" };
    }
    else if (bytesPerSec >= KB) {
        return { bytesPerSec / KB, "KB/s" };
    }
    return { bytesPerSec, "B/s" };
}

bool GetNetworkSpeedsPDH(double& outDownloadBps, double& outUploadBps) {
    PDH_HQUERY query = nullptr;
    PDH_STATUS status;

    // Open PDH query
    status = PdhOpenQueryW(nullptr, 0, &query);
    if (status != ERROR_SUCCESS) return false;

    // Expand wildcard paths
    std::vector<std::wstring> recvPaths, sendPaths;
    ExpandWildcard(L"\\Network Interface(*)\\Bytes Received/sec", recvPaths);
    ExpandWildcard(L"\\Network Interface(*)\\Bytes Sent/sec", sendPaths);

    // Add counters
    std::vector<PDH_HCOUNTER> recvCounters, sendCounters;
    for (auto& path : recvPaths) {
        PDH_HCOUNTER c;
        if (PdhAddCounterW(query, path.c_str(), 0, &c) == ERROR_SUCCESS)
            recvCounters.push_back(c);
    }
    for (auto& path : sendPaths) {
        PDH_HCOUNTER c;
        if (PdhAddCounterW(query, path.c_str(), 0, &c) == ERROR_SUCCESS)
            sendCounters.push_back(c);
    }

    // Collect initial sample
    PdhCollectQueryData(query);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    PdhCollectQueryData(query);

    // Get and sum values
    double sumRecv = 0, sumSend = 0;
    PDH_FMT_COUNTERVALUE val;

    for (auto c : recvCounters) {
        if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS)
            sumRecv += val.doubleValue;
    }
    for (auto c : sendCounters) {
        if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS)
            sumSend += val.doubleValue;
    }

    PdhCloseQuery(query);

    outDownloadBps = sumRecv;  // Bytes per second
    outUploadBps = sumSend;    // Bytes per second
    return true;
}