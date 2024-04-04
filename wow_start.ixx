#include "framework.h"

import pid_data_manager;
// wow_starter.ixx
export module wow_start;

// CONSTANTS
//const std::wstring app_path = LR"(C:\Users\pc\Desktop\wow1\_classic_era_\)";
//const std::wstring app_path = LR"(C:\Program Files (x86)\World of Warcraft\_classic_era_\)"; 
const std::wstring app_path = LR"(C:\Program Files (x86)\World of Warcraft\_classic_era_\)";
const std::wstring common = L"common.dll";
const std::wstring app_name = L"Wowclassic.exe";
const std::wstring dll_name = L"tetra.dll";

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

// LOAD MODULE
int load(DWORD pid, const wchar_t* path)
{
	auto sz_dll = (wcslen(path) + 1) * sizeof(wchar_t);
	auto process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	auto dll = VirtualAllocEx(process, nullptr, sz_dll, MEM_COMMIT, PAGE_READWRITE);

	if (!dll)
		return -1;

	WriteProcessMemory(process, dll, path, sz_dll, nullptr);

	auto kernel32 = GetModuleHandle(L"Kernel32.dll");

	if (!kernel32)
		return -1;

	auto load_library = (LPTHREAD_START_ROUTINE)(GetProcAddress(kernel32, "LoadLibraryW"));

	if (!load_library)
		return -1;

	HANDLE thread = CreateRemoteThread(process, nullptr, 0, load_library, dll, 0, nullptr);

	if (!thread)
		return -1;

	WaitForSingleObject(thread, INFINITE);
	VirtualFreeEx(process, dll, sz_dll, MEM_FREE);

	return 0;
}

// DETECT GUI
int initialize(DWORD pid)
{
	if (window(pid))
	{
		Sleep(3000); 
		load(pid, std::filesystem::current_path().append(common).c_str());
		load(pid, std::filesystem::current_path().append(dll_name).c_str());

		return 0;
	}

	return 1;
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

    void LaunchAccount(int accountNumber, const std::string& email, const std::string& additionalArgs = "") {
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
                    LaunchAccount(accountNumber, email, additionalArgs);
                    return;
                }
                else {
                    // If the PID is still launching and hasn't timed out, don't start another one
                    return;
                }
            }
        }

        // If there's no launching PID for this account, proceed with the existing code...
        // ...

        std::cout << "Launching account with email: " << email << std::endl;
        std::cout << "Additional arguments: " << additionalArgs << std::endl;

        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD flags = CREATE_SUSPENDED;

        // Here you should define app_path and app_name according to your context
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
                while (initialize(pid)) {
                    std::cout << "Initializing... PID: " << pid << std::endl;
                    Sleep(100);
                }

                // Mark the PID as initialized in PIDDataManager
                manager.getOrCreatePIDData(pid).initialized = true;
                manager.getOrCreatePIDData(pid).isNewPID = false;
                // Remove the PID from the launching map
                manager.removeLaunchingPID(pid);
                std::cout << "Initialization complete for PID: " << pid << std::endl;
                });

            // Detach the thread to allow it to run independently
            initThread.detach();
        }
        else {
            std::cout << "Failed to create process. Error Code: " << GetLastError() << std::endl;
        }
    }

    // Additional utility functions if needed
}