#include "framework.h"
#include <tlhelp32.h>
#include <tchar.h>
#include <psapi.h>
#include <future>
#include <memory>
#include <functional>
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
std::atomic<bool> initializationComplete(false);
std::atomic<bool> injectionComplete(false);

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
        const int CHECK_INTERVAL_MS = 1000;
        const int LAUNCH_GRACE_PERIOD_MS = 60000; // 1 minute grace period for launching
        const int MAX_INIT_TIME_MS = 30000;
        const int COOLDOWN_PERIOD_MS = 5000;
        const int MAX_RELAUNCH_ATTEMPTS = 5;
        const int RAPID_RESTART_THRESHOLD_MS = 300000;

        int relaunchAttempts = 0;
        std::chrono::steady_clock::time_point lastLaunchTime = std::chrono::steady_clock::now();
        bool launchInProgress = false;

        while (true) {
            DWORD pid = manager.getPIDForAccountNumber(accountNumber);

            if (pid == 0) {
                if (!launchInProgress) {
                    std::cout << "Account " << accountNumber << " not found in manager." << std::endl;

                    if (relog && relaunchAttempts < MAX_RELAUNCH_ATTEMPTS) {
                        auto now = std::chrono::steady_clock::now();
                        auto timeSinceLastLaunch = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLaunchTime).count();

                        if (timeSinceLastLaunch < RAPID_RESTART_THRESHOLD_MS) {
                            std::cout << "Rapid restarts detected. Waiting for cooldown period." << std::endl;
                            std::this_thread::sleep_for(std::chrono::milliseconds(COOLDOWN_PERIOD_MS));
                        }

                        std::cout << "Attempting to relaunch account " << accountNumber << " (Attempt " << relaunchAttempts + 1 << " of " << MAX_RELAUNCH_ATTEMPTS << ")" << std::endl;
                        initializationComplete.store(false);
                        injectionComplete.store(false);
                        LaunchAccount(accountNumber, email, additionalArgs, relog);
                        lastLaunchTime = std::chrono::steady_clock::now();
                        launchInProgress = true;
                        relaunchAttempts++;
                    }
                    else {
                        std::cout << "Max relaunch attempts reached or relog disabled for account " << accountNumber << ". Exiting monitor." << std::endl;
                        return;
                    }
                }
                else if (std::chrono::steady_clock::now() - lastLaunchTime > std::chrono::milliseconds(LAUNCH_GRACE_PERIOD_MS)) {
                    std::cout << "Launch grace period expired for account " << accountNumber << ". Considering as failed launch." << std::endl;
                    launchInProgress = false;
                }
            }
            else {
                launchInProgress = false;
                PIDData* pidDataPtr = manager.getPIDData(pid);
                if (!pidDataPtr) {
                    std::cout << "PID " << pid << " for account " << accountNumber << " not found in manager." << std::endl;
                    continue;
                }

                bool isRunning = isProcessRunning(pid);

                if (!isRunning) {
                    std::cout << "Process " << pid << " for account " << accountNumber << " has exited." << std::endl;

                    {
                        std::lock_guard<std::mutex> lock(*pidDataPtr->mtx);
                        pidDataPtr->running = false;
                        pidDataPtr->state = AccountState::Closed;
                    }

                    manager.removePIDData(pid);
                    continue;  // Go back to the start of the loop to handle relaunch
                }
                else {
                    std::lock_guard<std::mutex> lock(*pidDataPtr->mtx);
                    if (pidDataPtr->state == AccountState::Initializing) {
                        if (std::chrono::steady_clock::now() - lastLaunchTime > std::chrono::milliseconds(MAX_INIT_TIME_MS)) {
                            std::cout << "Initialization timed out for PID: " << pid << " (Account: " << accountNumber << ")" << std::endl;
                            manager.updateAccountState(pid, AccountState::Failed);
                        }
                    }
                    else if (pidDataPtr->state == AccountState::Running) {
                        if (std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - lastLaunchTime).count() >= 10) {
                            relaunchAttempts = 0;
                        }
                    }
                }
            }

            CloseBlizzardErrorProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
        }
    }
    PROCESS_INFORMATION LaunchProcess(int accountNumber, const std::string& email, const std::string& additionalArgs) {
        std::wstring app_path_w(app_path.begin(), app_path.end());
        std::wstring app_name_w(app_name.begin(), app_name.end());
        auto process_path = app_path_w + app_name_w;
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        DWORD flags = DETACHED_PROCESS;
        std::wstringstream cmdLineStream;
        cmdLineStream << L"\"" << process_path << L"\" " << std::wstring(additionalArgs.begin(), additionalArgs.end());
        std::wstring cmdLine = cmdLineStream.str();

        if (!CreateProcessW(process_path.c_str(), &cmdLine[0], nullptr, nullptr, FALSE, flags, nullptr, app_path_w.c_str(), &si, &pi)) {
            std::cerr << "Failed to create process. Error Code: " << GetLastError() << std::endl;
            return { 0 };
        }

        return pi;
    }

    std::unique_ptr<PIDData> CreatePIDData(DWORD pid, int accountNumber, const std::string& email, bool relog) {
        auto pidData = std::make_unique<PIDData>();
        pidData->pid = pid;
        pidData->relog = relog;
        pidData->c_init = false;
        pidData->isNewPID = true;
        pidData->accountNumber = accountNumber;
        pidData->accountEmail = email;
        pidData->running = true;
        pidData->initialized = false;
        pidData->changeProtectionDone = false;  // Ensure this is reset
        pidData->startTime = std::chrono::system_clock::now();
        return pidData;
    }

    bool InitializeProcess(DWORD pid, PIDDataManager& manager, std::atomic<bool>* initializationComplete) {
        std::cout << "Waiting for process to initialize... PID: " << pid << std::endl;
        HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (processHandle == NULL) {
            std::cerr << "Failed to open process handle for PID: " << pid << ". Error Code: " << GetLastError() << std::endl;
            return false;
        }

        DWORD waitResult = WaitForInputIdle(processHandle, 30000);
        if (waitResult == WAIT_FAILED) {
            std::cerr << "WaitForInputIdle failed for PID: " << pid << ". Error Code: " << GetLastError() << std::endl;
        }
        else if (waitResult == WAIT_TIMEOUT) {
            std::cerr << "WaitForInputIdle timed out for PID: " << pid << std::endl;
        }

        CloseHandle(processHandle);

        auto& pidData = manager.getOrCreatePIDData(pid);
        {
            std::lock_guard<std::mutex> lock(*pidData.mtx);
            pidData.initialized = true;
        }
        std::cout << "Process initialized for PID: " << pid << std::endl;

        initializationComplete->store(true);
        pidData.cvInit->notify_all();

        return true;
    }
    bool InjectDLL(DWORD pid, PIDDataManager& manager, std::atomic<bool>* initializationComplete, std::atomic<bool>* injectionComplete) {
        auto& pidData = manager.getOrCreatePIDData(pid);

        {
            std::unique_lock<std::mutex> lock(*pidData.mtx);
            if (!pidData.cvInit->wait_for(lock, std::chrono::seconds(60), [&] { return initializationComplete->load() || !pidData.running; })) {
                std::cerr << "Timed out waiting for process to initialize. PID: " << pid << std::endl;
                return false;
            }
        }

        if (pidData.running && !pidData.changeProtectionDone) {
            try {
                std::cout << "Starting DLL injection for PID: " << pid << std::endl;
                std::wstring dll_path = program_path(dll_name);

                HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
                if (processHandle == NULL) {
                    throw std::runtime_error("Failed to open process. Error code: " + std::to_string(GetLastError()));
                }

                mm module(processHandle, pid);
                auto [base, error] = module.map_dll(dll_path.c_str());

                if (!error.empty()) {
                    throw std::runtime_error("Failed to inject DLL: " + error);
                }

                if (!change_protection(pid, PAGE_EXECUTE_READWRITE)) {
                    throw std::runtime_error("Failed to change memory protection");
                }

                pidData.changeProtectionDone = true;
                pidData.isNewPID = false;
                manager.removeLaunchingPID(pid);
                std::cout << "DLL injection complete for PID: " << pid << std::endl;
                injectionComplete->store(true);

                return true;
            }
            catch (const std::exception& e) {
                std::cerr << "Error during injection for PID " << pid << ": " << e.what() << std::endl;
                injectionComplete->store(false);
                return false;
            }
        }

        pidData.cvMonitor->notify_all();
        return true;
    }
    void WaitForCompletion(DWORD pid, std::atomic<bool>* initializationComplete, std::atomic<bool>* injectionComplete) {
        auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(120)) {
            if (initializationComplete->load() && injectionComplete->load()) {
                std::cout << "Initialization and injection completed successfully for PID: " << pid << std::endl;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cerr << "Timeout: Initialization or injection did not complete within 2 minutes for PID: " << pid << std::endl;
    }
    // Helper function to check if a process is still running
    bool IsProcessRunning(DWORD pid) {
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (process == NULL) {
            return false;
        }
        DWORD result = WaitForSingleObject(process, 0);
        CloseHandle(process);
        return result == WAIT_TIMEOUT;
    }

    // Helper function to terminate a process
    void TerminateProcessIfRunning(DWORD pid) {
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (process != NULL) {
            TerminateProcess(process, 1);
            CloseHandle(process);
        }
    }

    void LaunchAccount(int accountNumber, const std::string& email, const std::string& additionalArgs, bool relog) {
        const int MAX_RETRIES = 3;
        const int RETRY_DELAY_MS = 5000;

        PIDDataManager& manager = PIDDataManager::getInstance();
        static std::mutex launchMutex;

        for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
            std::lock_guard<std::mutex> lock(launchMutex);

            // Check if the account is already in a non-launchable state
            if (manager.isAccountInState(accountNumber, AccountState::Launching) ||
                manager.isAccountInState(accountNumber, AccountState::Initializing) ||
                manager.isAccountInState(accountNumber, AccountState::Injecting) ||
                manager.isAccountInState(accountNumber, AccountState::Running)) {
                std::cout << "Account " << accountNumber << " is already active. Skipping launch..." << std::endl;
                return;
            }

            std::cout << "Launching account " << accountNumber << " (Attempt " << attempt + 1 << " of " << MAX_RETRIES << ")" << std::endl;
            std::cout << "Email: " << email << ", Additional arguments: " << additionalArgs << std::endl;

            PROCESS_INFORMATION pi = LaunchProcess(accountNumber, email, additionalArgs);
            if (pi.hProcess == NULL) {
                std::cerr << "Failed to create process. Retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                continue;
            }

            DWORD pid = pi.dwProcessId;
            std::cout << "Process created successfully. PID: " << pid << std::endl;

            auto pidData = CreatePIDData(pid, accountNumber, email, relog);
            pidData->state = AccountState::Launching;
            manager.updatePIDData(pid, std::move(pidData));

            auto threads = std::make_shared<std::vector<std::thread>>();

            // Initialize and inject
            threads->emplace_back([pid, &manager]() {
                manager.updateAccountState(pid, AccountState::Initializing);
                if (!InitializeProcess(pid, manager, &initializationComplete)) {
                    std::cerr << "Failed to initialize process " << pid << std::endl;
                    manager.updateAccountState(pid, AccountState::Failed);
                    return;
                }

                manager.updateAccountState(pid, AccountState::Injecting);
                if (initializationComplete.load() && !InjectDLL(pid, manager, &initializationComplete, &injectionComplete)) {
                    std::cerr << "Failed to inject DLL into process " << pid << std::endl;
                    manager.updateAccountState(pid, AccountState::Failed);
                    return;
                }

                if (injectionComplete.load()) {
                    manager.updateAccountState(pid, AccountState::Running);
                }
                });

            // Wait for completion
            threads->emplace_back([pid, &manager]() {
                WaitForCompletion(pid, &initializationComplete, &injectionComplete);
                if (initializationComplete.load() && injectionComplete.load()) {
                    manager.updateAccountState(pid, AccountState::Running);
                }
                else {
                    manager.updateAccountState(pid, AccountState::Failed);
                }
                });

            // Start pipes_server with timeout
            threads->emplace_back([pid, relog]() {
                if (!pipes_server(pid, relog)) {
                    std::cerr << "Pipes server failed for process " << pid << std::endl;
                }
                });

            // Start monitorAndRelog
            threads->emplace_back(monitorAndRelog, accountNumber, email, additionalArgs, relog);

            // Create a thread to join all other threads and update global variables
            std::thread([threads, pid, &manager]() {
                const int JOIN_TIMEOUT_MS = 60000; // 1 minute timeout
                auto start = std::chrono::steady_clock::now();

                for (auto& t : *threads) {
                    if (t.joinable()) {
                        t.join();
                    }
                }

                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                if (duration >= JOIN_TIMEOUT_MS || !IsProcessRunning(pid)) {
                    std::cerr << "Process " << pid << " failed to initialize or inject properly." << std::endl;
                    manager.updateAccountState(pid, AccountState::Failed);
                    TerminateProcessIfRunning(pid);
                    return;
                }

                // Ensure the final state is set to Running if everything succeeded
                if (initializationComplete.load() && injectionComplete.load()) {
                    manager.updateAccountState(pid, AccountState::Running);
                }
                }).detach();

                // If we've reached this point, we've successfully started the launch process
                return;
        }

        std::cerr << "Failed to launch account " << accountNumber << " after " << MAX_RETRIES << " attempts." << std::endl;
    }
}