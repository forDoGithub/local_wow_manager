#include "framework.h"
#include <tlhelp32.h>s
#include <tchar.h>
#include <psapi.h>
#pragma comment(lib, "psapi")
import pid_data_manager;
import mmap;
import remap;
import pipes;

// wow_starter.ixx
export module wow_start;

const std::wstring app_path = LR"(C:\Program Files (x86)\World of Warcraft\_retail_\)";
const std::wstring app_name = L"Wow.exe";
const std::wstring dll_name = L"Discord.dll";

std::wstring program_path(const std::wstring& append = L"") {
    wchar_t buffer[MAX_PATH];
    DWORD size = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (size != 0) {
        std::wstring full_path(buffer);
        size_t pos = full_path.find_last_of(L"\\");
        if (pos != std::wstring::npos) {
            return full_path.substr(0, pos) + L"\\" + append;
        }
    }
    return L"";
}

BOOL CALLBACK callback(HWND hwnd, LPARAM lParam) {
    auto& window = *reinterpret_cast<std::tuple<HWND, DWORD>*>(lParam);
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    auto& [h, p] = window;
    if (pid == p) h = hwnd;
    return TRUE;
}

HWND window(DWORD pid) {
    std::tuple<HWND, DWORD> window{};
    auto& [h, p] = window;
    p = pid;
    EnumWindows(callback, reinterpret_cast<LPARAM>(&window));
    return h ? h : nullptr;
}

bool waitForWindow(DWORD pid, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(timeout_ms)) {
        if (window(pid)) return true;
        Sleep(100);
    }
    return false;
}

export namespace wow_start {
    std::vector<std::string> ReadAccountEmails(const std::string& iniFilePath) {
        std::vector<std::string> emails;
        std::ifstream inFile(iniFilePath);
        std::string line;
        while (std::getline(inFile, line)) {
            if (line.substr(0, 6) == "email=") {
                emails.push_back(line.substr(6));
                std::cout << line.substr(6);
            }
        }
        return emails;
    }

    bool isProcessRunning(DWORD pid) {
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (process == NULL) {
            DWORD error = GetLastError();
            std::cout << "OpenProcess failed for PID " << pid << ". Error: " << error << std::endl;
            return false;
        }

        DWORD exitCode;
        if (GetExitCodeProcess(process, &exitCode)) {
            CloseHandle(process);
            if (exitCode == STILL_ACTIVE) {
                //std::cout << "Process " << pid << " is still active." << std::endl;
                return true;
            }
            else {
                std::cout << "Process " << pid << " has exited with code: " << exitCode << std::endl;
                return false;
            }
        }
        else {
            DWORD error = GetLastError();
            std::cout << "GetExitCodeProcess failed for PID " << pid << ". Error: " << error << std::endl;
            CloseHandle(process);
            return false;
        }
    }

    void stopProcess(DWORD pid) {
        HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (processHandle == NULL) {
            std::cerr << "Failed to open process. Error Code: " << GetLastError() << std::endl;
            return;
        }
        if (!TerminateProcess(processHandle, 1)) {
            std::cerr << "Failed to terminate process. Error Code: " << GetLastError() << std::endl;
        }
        CloseHandle(processHandle);
    }

    void CloseBlizzardErrorProcess() {
        DWORD aProcesses[1024], cbNeeded, cProcesses;
        unsigned int i;
        if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
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
                    if (_tcscmp(szProcessName, TEXT("BlizzardError.exe")) == 0) {
                        TerminateProcess(hProcess, 0);
                    }
                    CloseHandle(hProcess);
                }
            }
        }
    }

    void LaunchAccount(int accountNumber, const std::string& email, const std::string& additionalArgs, bool relog);

    void monitorAndRelog(int accountNumber, const std::string& email, const std::string& additionalArgs, bool relog) {
        PIDDataManager& manager = PIDDataManager::getInstance();
        const int CHECK_INTERVAL_MS = 1000; // Check every second
        const int MAX_INIT_TIME_MS = 30000; // Maximum initialization time (30 seconds)
        int initTime = 0;

        while (true) {
            DWORD pid = manager.getPIDForAccountNumber(accountNumber);
            if (pid == 0) {
                std::cout << "Account " << accountNumber << " not found in manager. Attempting to relaunch." << std::endl;
                if (relog) {
                    LaunchAccount(accountNumber, email, additionalArgs, relog);
                }
                else {
                    std::cout << "Relog is disabled for account " << accountNumber << ". Exiting monitor." << std::endl;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait before next check
                continue;
            }

            PIDData* pidDataPtr = manager.getPIDData(pid);
            if (!pidDataPtr) {
                std::cout << "PID " << pid << " for account " << accountNumber << " not found in manager. Attempting to relaunch." << std::endl;
                if (relog) {
                    LaunchAccount(accountNumber, email, additionalArgs, relog);
                }
                else {
                    std::cout << "Relog is disabled for account " << accountNumber << ". Exiting monitor." << std::endl;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait before next check
                continue;
            }

            bool isRunning = isProcessRunning(pid);

            if (!isRunning) {
                std::cout << "Process " << pid << " for account " << accountNumber << " has exited." << std::endl;

                {
                    std::lock_guard<std::mutex> lock(*pidDataPtr->mtx);
                    pidDataPtr->running = false;
                }

                manager.removePIDData(pid);

                if (relog) {
                    std::cout << "Attempting to restart account " << accountNumber << " after 5 seconds." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    LaunchAccount(accountNumber, email, additionalArgs, relog);
                }
                else {
                    std::cout << "Relog is disabled for account " << accountNumber << ". Exiting monitor." << std::endl;
                    return;
                }
            }
            else {
                std::lock_guard<std::mutex> lock(*pidDataPtr->mtx);
                if (!pidDataPtr->initialized) {
                    initTime += CHECK_INTERVAL_MS;
                    if (initTime >= MAX_INIT_TIME_MS) {
                        std::cout << "Initialization timed out for PID: " << pid << " (Account: " << accountNumber << ")" << std::endl;
                        manager.removeLaunchingPID(pid);
                        pidDataPtr->initialized = true;  // Consider it initialized to avoid getting stuck
                    }
                }
            }

            CloseBlizzardErrorProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
        }
    }
   
    void LaunchAccount(int accountNumber, const std::string& email, const std::string& additionalArgs, bool relog) {
        PIDDataManager& manager = PIDDataManager::getInstance();
        const auto& allActivePIDData = manager.getAllActivePIDData();
        const auto& allLaunchingPIDData = manager.getAllLaunchingPIDData();

        static std::mutex launchMutex;
        std::lock_guard<std::mutex> lock(launchMutex);

        std::cout << "Launching account with email: " << email << std::endl;
        std::cout << "Additional arguments: " << additionalArgs << std::endl;
        std::wstring app_path_w(app_path.begin(), app_path.end());
        std::wstring app_name_w(app_name.begin(), app_name.end());
        auto process_path = app_path_w + app_name_w;
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        DWORD flags = DETACHED_PROCESS;
        std::wstringstream cmdLineStream;
        cmdLineStream << L"\"" << process_path << L"\" " << std::wstring(additionalArgs.begin(), additionalArgs.end());
        std::wstring cmdLine = cmdLineStream.str();
        std::cout << "Attempting to create process with command line: " << std::string(cmdLine.begin(), cmdLine.end()) << std::endl;

        if (!CreateProcessW(process_path.c_str(), &cmdLine[0], nullptr, nullptr, FALSE, flags, nullptr, app_path_w.c_str(), &si, &pi)) {
            std::cerr << "Failed to create process. Error Code: " << GetLastError() << std::endl;
            return;
        }

        DWORD pid = pi.dwProcessId;
        std::cout << "Process created successfully. PID: " << pid << std::endl;

        auto pidData = std::make_unique<PIDData>();
        pidData->pid = pid;
        pidData->relog = relog;
        pidData->c_init = false;
        pidData->isNewPID = true;
        pidData->accountNumber = accountNumber;
        pidData->accountEmail = email;
        pidData->running = true;
        pidData->initialized = false;
        pidData->startTime = std::chrono::system_clock::now();
        manager.updatePIDData(pid, std::move(pidData));
        manager.addLaunchingPID(pid);

        std::thread initThread([&manager, pid, &pi]() {
            std::cout << "Waiting for process to initialize... PID: " << pid << std::endl;
            HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
            if (processHandle == NULL) {
                std::cerr << "Failed to open process handle for PID: " << pid << ". Error Code: " << GetLastError() << std::endl;
                return;
            }

            DWORD waitResult = WaitForInputIdle(processHandle, 15000);
            CloseHandle(processHandle);

            auto& pidData = manager.getOrCreatePIDData(pid);
            std::lock_guard<std::mutex> lock(*pidData.mtx);
            pidData.initialized = true;
            std::cout << "Process initialized successfully for PID: " << pid << std::endl;

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            pidData.cvInit->notify_one();
            });
        initThread.detach();

        std::thread injectThread([&manager, pid]() {
            
            auto& pidData = manager.getOrCreatePIDData(pid);
            {
                std::unique_lock<std::mutex> lock(*pidData.mtx);
                bool initialized = pidData.cvInit->wait_for(lock, std::chrono::seconds(60), [&pidData] { return pidData.initialized || !pidData.running; });

                if (!initialized) {
                    std::cerr << "Timed out waiting for process to initialize. PID: " << pid << std::endl;
                    return;
                }
            }

            if (pidData.running && !pidData.changeProtectionDone) {
                std::cout << "Starting DLL injection for PID: " << pid << std::endl;
                std::wstring dll_path = program_path(dll_name);
                mm module(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pidData.pid), pidData.pid);
                module.map_dll(dll_path.c_str());
                change_protection(PAGE_EXECUTE_READWRITE);
                pidData.changeProtectionDone = true;
                pidData.isNewPID = false;
                manager.removeLaunchingPID(pidData.pid);
                std::cout << "DLL injection complete for PID: " << pidData.pid << std::endl;
            }

            pidData.cvMonitor->notify_one();
            });
        injectThread.detach();

        std::thread pipesThread([pid, relog]() {
            pipes_server(pid, relog);
            });
        pipesThread.detach();

        std::thread monitorThread([accountNumber, email, additionalArgs, relog]() {
            monitorAndRelog(accountNumber, email, additionalArgs, relog);
            });
        monitorThread.detach();
    }
}