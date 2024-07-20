#include "framework.h"
#include <tlhelp32.h>
#include <tchar.h>
#include <psapi.h>
#include <future>
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
        const int MAX_INIT_TIME_MS = 30000;
        const int COOLDOWN_PERIOD_MS = 5000;
        const int MAX_RELAUNCH_ATTEMPTS = 5;
        const int RAPID_RESTART_THRESHOLD_MS = 300000;

        int initTime = 0;
        int relaunchAttempts = 0;
        std::chrono::steady_clock::time_point lastLaunchTime = std::chrono::steady_clock::now();

        while (true) {
            DWORD pid = manager.getPIDForAccountNumber(accountNumber);
            if (pid == 0) {
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
                    relaunchAttempts++;
                }
                else {
                    std::cout << "Max relaunch attempts reached or relog disabled for account " << accountNumber << ". Exiting monitor." << std::endl;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }

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
                }

                manager.removePIDData(pid);

                if (relog && relaunchAttempts < MAX_RELAUNCH_ATTEMPTS) {
                    auto now = std::chrono::steady_clock::now();
                    auto timeSinceLastLaunch = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLaunchTime).count();

                    if (timeSinceLastLaunch < RAPID_RESTART_THRESHOLD_MS) {
                        std::cout << "Rapid restarts detected. Waiting for cooldown period." << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(COOLDOWN_PERIOD_MS));
                    }

                    std::cout << "Attempting to restart account " << accountNumber << " (Attempt " << relaunchAttempts + 1 << " of " << MAX_RELAUNCH_ATTEMPTS << ")" << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    initializationComplete.store(false);
                    injectionComplete.store(false);
                    LaunchAccount(accountNumber, email, additionalArgs, relog);
                    lastLaunchTime = std::chrono::steady_clock::now();
                    relaunchAttempts++;
                }
                else {
                    std::cout << "Max relaunch attempts reached or relog disabled for account " << accountNumber << ". Exiting monitor." << std::endl;
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
                        pidDataPtr->initialized = true;
                    }
                }
                else {
                    if (std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - lastLaunchTime).count() >= 10) {
                        relaunchAttempts = 0;
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

    void InitializeProcess(DWORD pid, PIDDataManager& manager, std::atomic<bool>& initializationComplete) {
        std::cout << "Waiting for process to initialize... PID: " << pid << std::endl;
        HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (processHandle == NULL) {
            std::cerr << "Failed to open process handle for PID: " << pid << ". Error Code: " << GetLastError() << std::endl;
            return;
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

        initializationComplete.store(true);
        pidData.cvInit->notify_all();
    }

    void InjectDLL(DWORD pid, PIDDataManager& manager, std::atomic<bool>& initializationComplete, std::atomic<bool>& injectionComplete) {
        static std::atomic<bool> g_injectionComplete = false;
        auto& pidData = manager.getOrCreatePIDData(pid);

        {
            std::unique_lock<std::mutex> lock(*pidData.mtx);
            if (!pidData.cvInit->wait_for(lock, std::chrono::seconds(60), [&] { return initializationComplete.load() || !pidData.running; })) {
                std::cerr << "Timed out waiting for process to initialize. PID: " << pid << std::endl;
                return;
            }
        }

        if (pidData.running && !pidData.changeProtectionDone && !g_injectionComplete.exchange(true)) {
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

                // Implement retry mechanism for changing memory protection
                const int MAX_RETRIES = 5;
                const int RETRY_INTERVAL_MS = 100;
                DWORD result = 0;
                for (int i = 0; i < MAX_RETRIES; i++) {
                    result = change_protection(PAGE_EXECUTE_READWRITE);
                    if (result == 0) {
                        std::cout << "Memory protection changed successfully on attempt " << (i + 1) << std::endl;
                        break;
                    }
                    std::cerr << "Failed to change memory protection on attempt " << (i + 1) << ". Error code: " << result << std::endl;
                    if (i < MAX_RETRIES - 1) {
                        std::this_thread::yield();  // Yield to other threads instead of sleeping
                    }
                }

                if (result != 0) {
                    throw std::runtime_error("Failed to change memory protection after " + std::to_string(MAX_RETRIES) + " attempts");
                }

                pidData.changeProtectionDone = true;
                pidData.isNewPID = false;
                manager.removeLaunchingPID(pid);
                std::cout << "DLL injection complete for PID: " << pid << std::endl;
                injectionComplete.store(true);
            }
            catch (const std::exception& e) {
                std::cerr << "Error during injection for PID " << pid << ": " << e.what() << std::endl;
                injectionComplete.store(false);
            }
        }

        pidData.cvMonitor->notify_all();
    }
    void WaitForCompletion(DWORD pid, std::atomic<bool>& initializationComplete, std::atomic<bool>& injectionComplete) {
        auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(120)) {
            if (initializationComplete.load() && injectionComplete.load()) {
                std::cout << "Initialization and injection completed successfully for PID: " << pid << std::endl;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cerr << "Timeout: Initialization or injection did not complete within 2 minutes for PID: " << pid << std::endl;
    }
    void LaunchAccount(int accountNumber, const std::string& email, const std::string& additionalArgs, bool relog) {
        initializationComplete.store(false);
        injectionComplete.store(false);

        PIDDataManager& manager = PIDDataManager::getInstance();
        static std::mutex launchMutex;
        std::lock_guard<std::mutex> lock(launchMutex);

        std::cout << "Launching account " << accountNumber << " with email: " << email << std::endl;
        std::cout << "Additional arguments: " << additionalArgs << std::endl;

        PROCESS_INFORMATION pi = LaunchProcess(accountNumber, email, additionalArgs);
        if (pi.hProcess == NULL) {
            std::cerr << "Failed to create process." << std::endl;
            return;
        }

        DWORD pid = pi.dwProcessId;
        std::cout << "Process created successfully. PID: " << pid << std::endl;

        auto pidData = CreatePIDData(pid, accountNumber, email, relog);
        manager.updatePIDData(pid, std::move(pidData));
        manager.addLaunchingPID(pid);

        
        // Create threads instead of using std::async
        std::thread initThread(InitializeProcess, pid, std::ref(manager), std::ref(initializationComplete));
        std::thread injectThread(InjectDLL, pid, std::ref(manager), std::ref(initializationComplete), std::ref(injectionComplete));
        std::thread waitThread(WaitForCompletion, pid, std::ref(initializationComplete), std::ref(injectionComplete));
        std::thread pipesThread(pipes_server, pid, relog);
        std::thread monitorThread(monitorAndRelog, accountNumber, email, additionalArgs, relog);

        // Detach threads to let them run independently
        initThread.detach();
        injectThread.detach();
        waitThread.detach();
        pipesThread.detach();
        monitorThread.detach();
    }
    

}