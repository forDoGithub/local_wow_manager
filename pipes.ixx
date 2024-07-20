module;
#include "framework.h"
import pid_data_manager;
import remap;
#include <future>
export module pipes;

LPVOID base_address(HANDLE process)
{
    HMODULE _mods[1024];
    DWORD cb_needed;

    if (EnumProcessModules(process, _mods, sizeof(_mods), &cb_needed))
    {
        return _mods[0]; // The first module is typically the main executable module
    }

    return nullptr;
}
DWORD query_protection(HANDLE _process, LPVOID base) {
    MEMORY_BASIC_INFORMATION mem_info{};
    if (VirtualQueryEx(_process, base, &mem_info, sizeof(mem_info)) == 0) {
        std::wcerr << L"VirtualQueryEx failed. Error code: " << GetLastError() << std::endl;
        return 0;
    }
    return mem_info.Protect;
}
bool PerformHandshake(HANDLE hPipe) {
    wchar_t buffer[1024] = { 0 };
    DWORD bytesRead, bytesWritten;
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        std::wcerr << L"[DEBUG] Failed to create event for handshake. Error: " << GetLastError() << std::endl;
        return false;
    }

    std::wcout << L"[DEBUG] Waiting for handshake message..." << std::endl;

    // Read handshake message
    if (!ReadFile(hPipe, buffer, sizeof(buffer) - sizeof(wchar_t), NULL, &overlapped) &&
        GetLastError() != ERROR_IO_PENDING) {
        std::wcerr << L"[DEBUG] Handshake ReadFile failed. Error: " << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        return false;
    }

    DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 5000);
    if (waitResult != WAIT_OBJECT_0) {
        std::wcerr << L"[DEBUG] Handshake read " << (waitResult == WAIT_TIMEOUT ? L"timed out." : L"failed.") << std::endl;
        CloseHandle(overlapped.hEvent);
        return false;
    }

    if (!GetOverlappedResult(hPipe, &overlapped, &bytesRead, FALSE)) {
        std::wcerr << L"[DEBUG] Handshake GetOverlappedResult failed. Error: " << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        return false;
    }

    std::wstring message(buffer, bytesRead / sizeof(wchar_t));
    std::wcout << L"[DEBUG] Received handshake: '" << message << L"'" << std::endl;

    if (message != L"HELLO_SERVER") {
        std::wcerr << L"[DEBUG] Invalid handshake message: '" << message << L"'" << std::endl;
        CloseHandle(overlapped.hEvent);
        return false;
    }

    // Send handshake response
    const std::wstring response = L"HELLO_CLIENT";
    std::wcout << L"[DEBUG] Sending handshake response: '" << response << L"'" << std::endl;

    ResetEvent(overlapped.hEvent);
    if (!WriteFile(hPipe, response.c_str(), static_cast<DWORD>(response.length() * sizeof(wchar_t)), NULL, &overlapped) &&
        GetLastError() != ERROR_IO_PENDING) {
        std::wcerr << L"[DEBUG] Handshake WriteFile failed. Error: " << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        return false;
    }

    waitResult = WaitForSingleObject(overlapped.hEvent, 5000);
    if (waitResult != WAIT_OBJECT_0) {
        std::wcerr << L"[DEBUG] Handshake write " << (waitResult == WAIT_TIMEOUT ? L"timed out." : L"failed.") << std::endl;
        CloseHandle(overlapped.hEvent);
        return false;
    }

    if (!GetOverlappedResult(hPipe, &overlapped, &bytesWritten, FALSE)) {
        std::wcerr << L"[DEBUG] Handshake write GetOverlappedResult failed. Error: " << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        return false;
    }

    std::wcout << L"[DEBUG] Handshake completed successfully." << std::endl;
    CloseHandle(overlapped.hEvent);
    return true;
}



bool reconnect_pipe(HANDLE& hPipe, const std::wstring& pipe_name) {
    CloseHandle(hPipe);
    hPipe = CreateFile(
        pipe_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to reconnect to pipe. Error code: " << error << std::endl;
        return false;
    }

    DWORD pipeMode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(hPipe, &pipeMode, NULL, NULL)) {
        DWORD error = GetLastError();
        std::wcerr << L"SetNamedPipeHandleState failed. Error code: " << error << std::endl;
        CloseHandle(hPipe);
        return false;
    }

    return true;
}
void handle_client(HANDLE hPipe, DWORD pid, const std::wstring& pipe_name) {
    std::wcout << L"[DEBUG] Starting handle_client for PID: " << pid << std::endl;

    if (!PerformHandshake(hPipe)) {
        std::wcerr << L"[DEBUG] Initial handshake failed." << std::endl;
        CloseHandle(hPipe);
        return;
    }

    std::wcout << L"[DEBUG] Handshake completed successfully. Entering main message loop." << std::endl;

    wchar_t buffer[1024];
    DWORD bytesRead, bytesWritten;
    OVERLAPPED readOverlap = { 0 };
    OVERLAPPED writeOverlap = { 0 };
    readOverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    writeOverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (readOverlap.hEvent == NULL || writeOverlap.hEvent == NULL) {
        std::wcerr << L"[DEBUG] Failed to create event for overlapped I/O. Error code: " << GetLastError() << std::endl;
        CloseHandle(hPipe);
        return;
    }

    while (true) {
        std::wcout << L"[DEBUG] Start of loop in handle_client for PID: " << pid << std::endl;
        std::wcout << L"[DEBUG] Waiting for next message..." << std::endl;

        ZeroMemory(buffer, sizeof(buffer));
        ResetEvent(readOverlap.hEvent);

        BOOL readResult = ReadFile(hPipe, buffer, sizeof(buffer) - sizeof(wchar_t), NULL, &readOverlap);
        DWORD lastError = GetLastError();

        std::wcout << L"[DEBUG] ReadFile called. Result: " << (readResult ? L"TRUE" : L"FALSE") << L", LastError: " << lastError << std::endl;

        if (!readResult && lastError != ERROR_IO_PENDING) {
            if (lastError == ERROR_BROKEN_PIPE || lastError == ERROR_NO_DATA) {
                std::wcout << L"[DEBUG] Pipe closed by client. Exiting handle_client." << std::endl;
                break;
            }
            std::wcerr << L"[DEBUG] ReadFile failed. Error code: " << lastError << std::endl;
            break;
        }

        std::wcout << L"[DEBUG] Waiting for read operation to complete..." << std::endl;
        DWORD waitResult = WaitForSingleObject(readOverlap.hEvent, 5000);  // 5 second timeout
        std::wcout << L"[DEBUG] WaitForSingleObject result: " << waitResult << std::endl;

        if (waitResult == WAIT_OBJECT_0) {
            if (!GetOverlappedResult(hPipe, &readOverlap, &bytesRead, FALSE)) {
                lastError = GetLastError();
                std::wcerr << L"[DEBUG] GetOverlappedResult failed. Error code: " << lastError << std::endl;
                if (lastError == ERROR_BROKEN_PIPE || lastError == ERROR_NO_DATA) {
                    std::wcout << L"[DEBUG] Pipe closed by client. Exiting handle_client." << std::endl;
                    break;
                }
                break;
            }

            std::wcout << L"[DEBUG] Bytes read: " << bytesRead << std::endl;

            if (bytesRead == 0) {
                std::wcout << L"[DEBUG] Client disconnected. Exiting handle_client." << std::endl;
                break;
            }

            // Process the received data
            std::wstring received_message(buffer, bytesRead / sizeof(wchar_t));
            std::wcout << L"[DEBUG] Received message: \"" << received_message << L"\"" << std::endl;
            std::wcout << L"[DEBUG] Received message length: " << received_message.length() << L" characters" << std::endl;

            // Handle the message
            std::wstring response;
            if (received_message == L"_remap") {
                change_protection(pid, PAGE_EXECUTE_READWRITE);
                response = L"Remap successful";
            }
            else if (received_message == L"_restore") {
                change_protection(pid, PAGE_EXECUTE_READ);
                response = L"Restore successful";
            }
            else {
                std::wcout << L"[DEBUG] Received unknown command: " << received_message << std::endl;
                response = L"Unknown command received";
            }

            // Send the response using overlapped I/O
            std::wcout << L"[DEBUG] Sending response: '" << response << L"'" << std::endl;
            ResetEvent(writeOverlap.hEvent);
            if (!WriteFile(hPipe, response.c_str(), static_cast<DWORD>(response.length() * sizeof(wchar_t)), NULL, &writeOverlap) &&
                GetLastError() != ERROR_IO_PENDING) {
                std::wcerr << L"[DEBUG] WriteFile failed. Error code: " << GetLastError() << std::endl;
                break;
            }

            waitResult = WaitForSingleObject(writeOverlap.hEvent, 5000);  // 5 second timeout
            if (waitResult == WAIT_OBJECT_0) {
                if (!GetOverlappedResult(hPipe, &writeOverlap, &bytesWritten, FALSE)) {
                    std::wcerr << L"[DEBUG] Write GetOverlappedResult failed. Error code: " << GetLastError() << std::endl;
                    break;
                }
                std::wcout << L"[DEBUG] Response sent successfully. Bytes written: " << bytesWritten << std::endl;
            }
            else if (waitResult == WAIT_TIMEOUT) {
                std::wcerr << L"[DEBUG] Write operation timed out." << std::endl;
                break;
            }
            else {
                std::wcerr << L"[DEBUG] Wait for write operation failed. Error code: " << GetLastError() << std::endl;
                break;
            }
        }
        else if (waitResult == WAIT_TIMEOUT) {
            std::wcerr << L"[DEBUG] Read operation timed out. Continuing to wait..." << std::endl;
            continue;
        }
        else {
            std::wcerr << L"[DEBUG] Wait for read operation failed. Error code: " << GetLastError() << std::endl;
            break;
        }
    }

    std::wcout << L"[DEBUG] Exiting handle_client for PID: " << pid << std::endl;
    CloseHandle(readOverlap.hEvent);
    CloseHandle(writeOverlap.hEvent);
    CloseHandle(hPipe);
}
export int pipes_server(DWORD pid, bool relog) {
    std::wcout << L"[DEBUG] Entering pipes_server function for PID: " << pid << std::endl;
    PIDDataManager& manager = PIDDataManager::getInstance();
    auto& pidData = manager.getOrCreatePIDData(pid);
    std::wstring pipe_name = L"\\\\.\\pipe\\tetra_" + std::to_wstring(pid);
    std::wcout << L"[DEBUG] Pipe name: " << pipe_name << std::endl;

    while (true) {
        std::wcout << L"[DEBUG] Creating new pipe instance for PID: " << pid << std::endl;
        HANDLE hPipe = CreateNamedPipe(
            pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            1024 * 16,
            1024 * 16,
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            std::wcerr << L"[DEBUG] Failed to create named pipe for PID " << pid << L". Error code: " << error << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));  // Add a delay before retrying
            continue;  // Try to create the pipe again instead of returning
        }

        std::wcout << L"[DEBUG] Waiting for client connection on pipe " << pipe_name << L"..." << std::endl;
        OVERLAPPED connectOverlap = { 0 };
        connectOverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (connectOverlap.hEvent == NULL) {
            std::wcerr << L"[DEBUG] Failed to create event for ConnectNamedPipe. Error: " << GetLastError() << std::endl;
            CloseHandle(hPipe);
            continue;
        }

        BOOL connectResult = ConnectNamedPipe(hPipe, &connectOverlap);
        DWORD lastError = GetLastError();
        std::wcout << L"[DEBUG] ConnectNamedPipe result: " << connectResult << L", LastError: " << lastError << std::endl;

        bool clientConnected = false;

        if (!connectResult) {
            switch (lastError) {
            case ERROR_IO_PENDING:
            {
                std::wcout << L"[DEBUG] Connection is pending. Waiting for completion..." << std::endl;
                DWORD waitResult = WaitForSingleObject(connectOverlap.hEvent, 30000);  // Increased timeout to 30 seconds
                std::wcout << L"[DEBUG] Wait result: " << waitResult << std::endl;
                if (waitResult == WAIT_OBJECT_0) {
                    DWORD bytesTransferred = 0;
                    if (GetOverlappedResult(hPipe, &connectOverlap, &bytesTransferred, FALSE)) {
                        clientConnected = true;
                    }
                    else {
                        std::wcerr << L"[DEBUG] GetOverlappedResult failed. Error: " << GetLastError() << std::endl;
                    }
                }
                else if (waitResult == WAIT_TIMEOUT) {
                    std::wcerr << L"[DEBUG] Wait for ConnectNamedPipe timed out." << std::endl;
                }
                else {
                    std::wcerr << L"[DEBUG] Wait for ConnectNamedPipe failed. Error: " << GetLastError() << std::endl;
                }
            }
            break;
            case ERROR_PIPE_CONNECTED:
                std::wcout << L"[DEBUG] Client already connected." << std::endl;
                clientConnected = true;
                break;
            default:
                std::wcerr << L"[DEBUG] ConnectNamedPipe failed. Error code: " << lastError << std::endl;
                break;
            }
        }
        else {
            clientConnected = true;
        }

        CloseHandle(connectOverlap.hEvent);

        if (!clientConnected) {
            CloseHandle(hPipe);
            continue;
        }

        std::wcout << L"[DEBUG] Client connected on pipe " << pipe_name << L"." << std::endl;

        // Handle client in the current thread
        std::wcout << L"[DEBUG] Calling handle_client function" << std::endl;
        handle_client(hPipe, pid, pipe_name);

        // After handle_client returns, create a new pipe instance
        std::wcout << L"[DEBUG] handle_client function returned. Client disconnected. Creating new pipe instance." << std::endl;
    }

    std::wcout << L"[DEBUG] Exiting pipes_server function" << std::endl;
    return 0;
}