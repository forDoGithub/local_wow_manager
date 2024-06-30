#include "framework.h"
#include <tlhelp32.h>
#include <tchar.h>
#include <psapi.h>
#pragma comment(lib, "psapi")
import pid_data_manager;
import mmap;
export module wow_start;
// CONSTANTS
const std::wstring app_path = LR"(C:\Program Files (x86)\World of Warcraft\_retail_\)";
const std::wstring app_name = L"Wow.exe";
const std::wstring dll_name = L"Discord.dll";

// WINDOW CALLBACK
BOOL CALLBACK callback(HWND hwnd, LPARAM lParam)
{
    auto& window = *reinterpret_cast<std::tuple<HWND, DWORD>*>(lParam);

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    auto& [h, p] = window;
    if (pid == p)
        h = hwnd;

    return TRUE;
}

// WINDOW HANDLE
HWND window(DWORD pid)
{
    std::tuple<HWND, DWORD> window{};
    auto& [h, p] = window;
    p = pid;

    EnumWindows(callback, reinterpret_cast<LPARAM>(&window));

    if (h)
        return h;

    return nullptr;
}

// NEW LOAD MODULE USING MM CLASS
int load_new(DWORD pid, const std::wstring& path)
{
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process)
        return -1;

    mm module(process, pid);
    module.map_dll(path.c_str());

    CloseHandle(process);

    return 0;
}

bool waitForWindow(DWORD pid, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(timeout_ms)) {
        if (window(pid)) {
            return true;
        }
        Sleep(100);  // Check every 100 milliseconds
    }
    return false;  // Timeout
}

// DETECT GUI AND INJECT DLL
int initialize(DWORD pid, int timeout_ms = 30000) {
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process) {
        return -1;
    }

    DWORD waitResult = WaitForInputIdle(process, timeout_ms);
    CloseHandle(process);

    if (waitResult == 0) {  // The process is idle, meaning it has finished initialization
        if (waitForWindow(pid, timeout_ms)) {  // Ensure the window is created
            return load_new(pid, std::filesystem::current_path().append(dll_name).c_str());
        }
    }
    return 1;  // Timeout or error
}

export namespace wow_start {

    std::vector<std::string> ReadAccountEmails(const std::string& iniFilePath) {
        std::vector<std::string> emails;
        std::ifstream inFile(iniFilePath);

        std::string line;
        while (std::getline(inFile, line)) {
            if (line.substr(0, 6) == "email=") { // Check if the line starts with "email="
                emails.push_back(line.substr(6)); // Add the part after "email=" to the vector
                std::cout << line.substr(6);
            }
        }

        return emails;
    }

    bool isProcessRunning(DWORD pid) {
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        DWORD ret = WaitForSingleObject(process, 0);
        CloseHandle(process);
        return ret == WAIT_TIMEOUT;
    }

    void stopProcess(DWORD pid) {
        // Open a handle to the process
        HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);

        if (processHandle == NULL) {
            std::cerr << "Failed to open process. Error Code: " << GetLastError() << std::endl;
            return;
        }

        // Terminate the process
        if (!TerminateProcess(processHandle, 1)) {
            std::cerr << "Failed to terminate process. Error Code: " << GetLastError() << std::endl;
        }

        // Close the handle to the process
        CloseHandle(processHandle);
    }

    void CloseBlizzardErrorProcess() {
        DWORD aProcesses[1024], cbNeeded, cProcesses;
        unsigned int i;

        if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
            // Failed to enumerate processes
            return;
        }

        cProcesses = cbNeeded / sizeof(DWORD);

        for (i = 0; i < cProcesses; i++) {
            if (aProcesses[i] != 0) {
                TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, aProcesses[i]);

                if (hProcess != NULL) {
                    HMODULE hMod;
                    DWORD cbNeeded;

                    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                        GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
                    }

                    // If a process with the name "BlizzardError" exists, terminate it
                    if (_tcscmp(szProcessName, TEXT("BlizzardError.exe")) == 0) {
                        TerminateProcess(hProcess, 0);
                    }

                    CloseHandle(hProcess);
                }
            }
        }
    }

    void LaunchAccount(int accountNumber, const std::string& email, const std::string& additionalArgs, bool relog);

    void monitorAndRelog(DWORD pid, int accountNumber, const std::string& email, const std::string& additionalArgs = "", bool relog = false) {
        while (true) {
            // Check if process is still running
            if (!isProcessRunning(pid)) {
                std::cout << "Process " << pid << " has exited. Attempting to restart." << std::endl;

                // Launch the game again for the same account
                LaunchAccount(accountNumber, email, additionalArgs, relog);

                // Exit the loop since LaunchAccount will handle monitoring for the new process
                return;
            }
            CloseBlizzardErrorProcess();
            // Sleep for a specified time before checking again
            Sleep(1000); // Example: 10 seconds
        }
    }

    void LaunchAccount(int accountNumber, const std::string& email, const std::string& additionalArgs = "", bool relog = false) {
        // Get the PIDDataManager instance
        PIDDataManager& manager = PIDDataManager::getInstance();

        // Check if there's a launching PID for this account
        const auto& allLaunchingPIDData = manager.getAllLaunchingPIDData();
        for (const auto& pidDataEntry : allLaunchingPIDData) {
            if (pidDataEntry.second.accountNumber == accountNumber) {
                auto now = std::chrono::system_clock::now();
                auto launchTime = pidDataEntry.second.startTime;
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - launchTime);

                if (duration.count() > 5) {
                    std::cout << "PID LAUNCH TIMES OUT AT 5 s!!!! starting the same agaiiiiin!!!" << std::endl;

                    // Stop the current process
                    stopProcess(pidDataEntry.first);

                    // Remove the PID from the launching map
                    manager.removeLaunchingPID(pidDataEntry.first);

                    // Start the process again
                    LaunchAccount(accountNumber, email, additionalArgs, relog);
                    return;
                }
                else {
                    // If the PID is still launching and hasn't timed out, don't start another one
                    return;
                }
            }
        }

        std::cout << "Launching account with email: " << email << std::endl;
        std::cout << "Additional arguments: " << additionalArgs << std::endl;

        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD flags = CREATE_SUSPENDED;

        auto path = app_path + app_name;
        std::wstringstream cmdLineStream;
        cmdLineStream << L"\"" << path << L"\" " << std::wstring(additionalArgs.begin(), additionalArgs.end());
        std::wstring cmdLine = cmdLineStream.str();

        std::cout << "Attempting to create process with command line: " << std::string(cmdLine.begin(), cmdLine.end()) << std::endl;

        BOOL success = CreateProcess(NULL, &cmdLine[0], NULL, NULL, FALSE, flags, NULL, app_path.c_str(), &si, &pi);

        if (success) {
            std::cout << "Process created successfully. PID: " << pi.dwProcessId << std::endl;

            DWORD pid = pi.dwProcessId;
            // Mark the PID as created but not initialized in PIDDataManager
            auto& pidData = manager.getOrCreatePIDData(pid);
            pidData.relog = relog;
            pidData.c_init = false;
            pidData.isNewPID = true;
            pidData.accountNumber = accountNumber;
            pidData.accountEmail = email;  // Set the account email
            // Add the PID to the launching map
            manager.addLaunchingPID(pid);
            ResumeThread(pi.hThread);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            // Create a new thread for initializing the process
            std::thread initThread([pid, &manager]() {
                if (initialize(pid) == 0) {
                    // Mark the PID as initialized in PIDDataManager
                    manager.getOrCreatePIDData(pid).initialized = true;
                    manager.getOrCreatePIDData(pid).isNewPID = false;
                    // Remove the PID from the launching map
                    manager.removeLaunchingPID(pid);
                    std::cout << "Initialization complete for PID: " << pid << std::endl;
                }
                else {
                    std::cerr << "Failed to initialize process PID: " << pid << std::endl;
                    manager.removeLaunchingPID(pid);
                }
                });

            // Detach the thread to allow it to run independently
            initThread.detach();

            // Start the monitoring in a separate thread
            std::thread monitorThread([pid, accountNumber, email, additionalArgs, relog, &manager]() {
                while (!manager.getOrCreatePIDData(pid).initialized) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                while (true) {
                    if (!manager.getOrCreatePIDData(pid).relog || !isProcessRunning(pid)) {
                        break;
                    }
                    monitorAndRelog(pid, accountNumber, email, additionalArgs, relog);
                }
                });

            monitorThread.detach();
        }
        else {
            std::cerr << "Failed to create process. Error Code: " << GetLastError() << std::endl;
        }

        // Close the handles even if CreateProcess fails
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

}
