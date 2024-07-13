module;
#include "framework.h"
import pid_data_manager;
import remap;
export module pipes;


void handle_client(HANDLE hPipe, DWORD pid) {
    PIDDataManager& manager = PIDDataManager::getInstance();
    auto& pidData = manager.getOrCreatePIDData(pid);

    // Test cout to verify access to pidData.relog
    std::wcout << L"PID " << pid << L" relog status: " << (pidData.relog ? L"true" : L"false") << std::endl;

    wchar_t buffer[512]{};
    DWORD dwRead;
    while (ReadFile(hPipe, buffer, sizeof(buffer) - sizeof(wchar_t), &dwRead, NULL) != FALSE) {
        buffer[dwRead / sizeof(wchar_t)] = L'\0';
        std::wstring received_msg(buffer);
        if (received_msg == L"_remap") {
            std::wcout << L"Removing memory protection" << std::endl;
            change_protection(PAGE_EXECUTE_READWRITE);
        }
        else if (received_msg == L"_restore") {
            change_protection(PAGE_EXECUTE_READ);
        }
        // Add any other message handling here
    }

    DWORD lastError = GetLastError();
    if (lastError == ERROR_BROKEN_PIPE) {
        std::wcout << L"Client disconnected." << std::endl;
    }
    else {
        std::wcerr << L"ReadFile failed. Error code: " << lastError << std::endl;
    }
}

export int pipes_server(DWORD pid, bool relog) {
    PIDDataManager& manager = PIDDataManager::getInstance();
    auto& pidData = manager.getOrCreatePIDData(pid);

    std::wstringstream ss;
    ss << L"\\\\.\\pipe\\tetra_" << pid;
    std::wstring pipe_name = ss.str();

    bool initial_connection = true;  // Flag to track initial connection

    while (true) {
        HANDLE hPipe = CreateNamedPipe(
            pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            1024 * 16,
            1024 * 16,
            NMPWAIT_USE_DEFAULT_WAIT,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Failed to create named pipe for PID " << pid << L". Error code: " << GetLastError() << std::endl;
            return 1;
        }

        std::wcout << L"Waiting for client connection on pipe " << pipe_name << L"..." << std::endl;
        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected) {
            std::wcout << L"Client connected on pipe " << pipe_name << L"." << std::endl;

            // Check if this is the initial connection and relog is true
            if (initial_connection && relog) {
                const wchar_t* message = L"relog";
                DWORD bytesWritten;
                if (!WriteFile(hPipe, message, wcslen(message) * sizeof(wchar_t), &bytesWritten, NULL)) {
                    std::wcerr << L"Failed to send initial relog message. Error code: " << GetLastError() << std::endl;
                }
                else {
                    std::wcout << L"Sent initial relog message to client." << std::endl;
                }
                initial_connection = false;  // Reset the flag after sending the message
            }

            handle_client(hPipe, pid);
        }
        else {
            std::wcerr << L"Failed to connect to client on pipe " << pipe_name << L". Error code: " << GetLastError() << std::endl;
            CloseHandle(hPipe);
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    return 0;
}
