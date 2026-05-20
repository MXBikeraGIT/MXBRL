// ram_limiter.cpp – MX Bikes RAM limiter plugin
#include <windows.h>
#include <stdio.h>
#include <string>
#include <fstream>

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
    if (lastSlash != std::string::npos)
        iniPath = iniPath.substr(0, lastSlash + 1) + "config.ini";
    else
        iniPath = "config.ini";

    bool enabled = (ReadConfigInt(iniPath, "limit", 1) == 1);
    int maxMB = ReadConfigInt(iniPath, "max", 1024);

    if (!enabled) {
        MessageBoxA(NULL, "RAM Limiter plugin loaded but disabled in config.ini", "RAM Limiter", MB_OK);
        return;
    }

    // Try to create a job object with a hard memory limit
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
    if (!AssignProcessToJobObject(hJob, GetCurrentProcess())) {
        DWORD err = GetLastError();
        if (err == 87) { // ERROR_INVALID_PARAMETER – usually means process already in a job
            char msg[512];
            sprintf_s(msg, "Cannot enforce hard memory limit because the process is already in a job.\n\n"
                           "Please use the following workaround in your Winlator container:\n"
                           "  ulimit -v %d\n\n"
                           "This will set a hard virtual memory limit at the Linux level.", maxMB * 1024);
            MessageBoxA(NULL, msg, "RAM Limiter – Use ulimit instead", MB_OK);
        } else {
            char msg[256];
            sprintf_s(msg, "Failed to assign process to job object (error %d).", err);
            MessageBoxA(NULL, msg, "RAM Limiter Error", MB_OK);
        }
        CloseHandle(hJob);
        return;
    }

    // Success – hard limit active
    char msg[256];
    sprintf_s(msg, "RAM Limiter active: %d MB hard limit.\nThe game will crash if it exceeds this limit.", maxMB);
    MessageBoxA(NULL, msg, "RAM Limiter", MB_OK);
    // Keep job object handle alive (will close when process exits)
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return TRUE;
}
