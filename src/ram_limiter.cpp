// ram_limiter.cpp - MX Bikes RAM limiter plugin
#include <windows.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>

int ReadConfigInt(const std::string& filename, const std::string& key, int default_value) {
    std::ifstream file(filename);
    if (!file.is_open()) return default_value;
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            k.erase(0, k.find_first_not_of(" \t"));
            k.erase(k.find_last_not_of(" \t") + 1);
            v.erase(0, v.find_first_not_of(" \t"));
            v.erase(v.find_last_not_of(" \t") + 1);
            if (k == key) return std::stoi(v);
        }
    }
    return default_value;
}

extern "C" __declspec(dllexport) void InitializePlugin() {
    char dllPath[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA("ram_limiter.dlo"), dllPath, MAX_PATH);
    std::string iniPath = std::string(dllPath);
    size_t lastSlash = iniPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        iniPath = iniPath.substr(0, lastSlash + 1) + "config.ini";
    } else {
        iniPath = "config.ini";
    }

    bool enabled = (ReadConfigInt(iniPath, "limit", 1) == 1);
    int maxMB = ReadConfigInt(iniPath, "max", 1024);
    bool strict = (ReadConfigInt(iniPath, "strict", 1) == 1); // new option

    if (!enabled) {
        MessageBoxA(NULL, "RAM Limiter plugin loaded but disabled in config.ini", "RAM Limiter", MB_OK);
        return;
    }

    // --- Method 1: Job Object (hard limit) ---
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (!hJob) {
        MessageBoxA(NULL, "Failed to create job object", "RAM Limiter Error", MB_OK);
        return;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
    jeli.JobMemoryLimit = (SIZE_T)maxMB * 1024 * 1024;
    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
        MessageBoxA(NULL, "Failed to set job memory limit", "RAM Limiter Error", MB_OK);
        CloseHandle(hJob);
        return;
    }

    // Try to assign the current process to the job
    BOOL assigned = AssignProcessToJobObject(hJob, GetCurrentProcess());
    if (!assigned) {
        // Process might already be in a job; we can't remove it. Show warning and use fallback.
        DWORD err = GetLastError();
        char msg[512];
        sprintf_s(msg, "Warning: Could not assign process to job object (error %d).\n"
                      "The memory limit may not be enforced.\n"
                      "Using working set fallback.", err);
        MessageBoxA(NULL, msg, "RAM Limiter", MB_OK);

        // Fallback: set working set size (soft limit)
        SIZE_T min = 0, max = (SIZE_T)maxMB * 1024 * 1024;
        if (!SetProcessWorkingSetSize(GetCurrentProcess(), min, max)) {
            MessageBoxA(NULL, "Failed to set working set size", "RAM Limiter Error", MB_OK);
        } else {
            MessageBoxA(NULL, "Working set limit applied (soft limit).", "RAM Limiter", MB_OK);
        }
        CloseHandle(hJob);
        return;
    }

    // Success
    char msg[256];
    sprintf_s(msg, "RAM Limiter active: %d MB hard limit (Job Object).", maxMB);
    MessageBoxA(NULL, msg, "RAM Limiter", MB_OK);
    // Keep the job object handle open (it will be closed when process exits)
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return TRUE;
}
