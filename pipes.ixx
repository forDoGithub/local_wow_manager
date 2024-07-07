module;
#include "framework.h"
import pid_data_manager;
import remap;
export module pipes;


void handle_client(HANDLE hPipe) {
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
    }
    DWORD lastError = GetLastError();
    if (lastError == ERROR_BROKEN_PIPE) {
        std::wcout << L"Client disconnected." << std::endl;
    }
    else {
        std::wcerr << L"ReadFile failed. Error code: " << lastError << std::endl;
    }
}

export int pipes_server(DWORD pid) {
    std::wstringstream ss;
    ss << L"\\\\.\\pipe\\tetra_" << pid;
    std::wstring pipe_name = ss.str();

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
            std::wcout << L"Client connected on pipe " << pipe_name << L". Waiting for messages..." << std::endl;
            handle_client(hPipe);
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