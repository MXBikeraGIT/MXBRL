// ram_limiter.cpp - MX Bikes RAM limiter plugin using Windows Job Objects
#include <windows.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <sstream>

// Function to read integer from a simple key=value .ini file
int ReadConfigInt(const std::string& filename, const std::string& key, int default_value) {
    std::ifstream file(filename);
    if (!file.is_open()) return default_value;
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            // trim spaces (simple)
            k.erase(0, k.find_first_not_of(" \t"));
            k.erase(k.find_last_not_of(" \t") + 1);
            v.erase(0, v.find_first_not_of(" \t"));
            v.erase(v.find_last_not_of(" \t") + 1);
            if (k == key) {
                return std::stoi(v);
            }
        }
    }
    return default_value;
}

// Plugin entry point – called when MX Bikes loads the .dlo
extern "C" __declspec(dllexport) void InitializePlugin() {
    // Get the path of the plugin DLL
    char dllPath[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA("ram_limiter.dlo"), dllPath, MAX_PATH);
    std::string iniPath = std::string(dllPath);
    size_t lastSlash = iniPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        iniPath = iniPath.substr(0, lastSlash + 1) + "config.ini";
    } else {
        iniPath = "config.ini";
    }

    // Read configuration
    bool enabled = (ReadConfigInt(iniPath, "limit", 1) == 1);
    int maxMB = ReadConfigInt(iniPath, "max", 1024);

    if (!enabled) {
        MessageBoxA(NULL, "RAM Limiter plugin loaded but disabled in config.ini", "RAM Limiter", MB_OK);
        return;
    }

    // Create a job object and set a memory limit
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (!hJob) {
        MessageBoxA(NULL, "Failed to create job object", "RAM Limiter Error", MB_OK);
        return;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
    jeli.JobMemoryLimit = (SIZE_T)maxMB * 1024 * 1024; // convert MB to bytes

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
        MessageBoxA(NULL, "Failed to set job memory limit", "RAM Limiter Error", MB_OK);
        CloseHandle(hJob);
        return;
    }

    // Assign the current process to the job
    if (!AssignProcessToJobObject(hJob, GetCurrentProcess())) {
        MessageBoxA(NULL, "Failed to assign process to job object", "RAM Limiter Error", MB_OK);
        CloseHandle(hJob);
        return;
    }

    // Success – show a notification
    char msg[256];
    sprintf_s(msg, "RAM Limiter active: %d MB limit", maxMB);
    MessageBoxA(NULL, msg, "RAM Limiter", MB_OK);
}

// Optional: DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return TRUE;
}
