module;
#include "framework.h"
export module remap;

typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)

typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT;

extern "C" {
    NTSTATUS WINAPI NtSuspendProcess(HANDLE ProcessHandle);
    NTSTATUS WINAPI NtResumeProcess(HANDLE ProcessHandle);
    NTSTATUS WINAPI NtCreateSection(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle);
    NTSTATUS WINAPI NtUnmapViewOfSection(HANDLE ProcessHandle, PVOID BaseAddress);
    NTSTATUS WINAPI NtMapViewOfSection(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID* BaseAddress, ULONG ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, SECTION_INHERIT InheritDisposition, ULONG AllocationType, ULONG Protect);
}

void native_error(const char* nativeMethod, NTSTATUS status) {
    std::cout << nativeMethod << " failed. NTSTATUS: 0x" << std::hex << status << std::endl;
    ExitProcess(status);
}

bool read_memory(HANDLE processHandle, LPVOID baseAddress, LPVOID buffer, SIZE_T size) {
    SIZE_T bytes_read = 0;
    BYTE* current_address = static_cast<BYTE*>(baseAddress);
    BYTE* current_buffer = static_cast<BYTE*>(buffer);
    SIZE_T total_read = 0;

    while (total_read < size) {
        SIZE_T bytesToRead = min(size - total_read, 4096); // Read in chunks of 4096 bytes
        if (!ReadProcessMemory(processHandle, current_address, current_buffer, bytesToRead, &bytes_read)) {
            DWORD error = GetLastError();
            if (error == ERROR_PARTIAL_COPY) {
                total_read += bytes_read;
                current_address += bytes_read;
                current_buffer += bytes_read;
                continue;
            }
            std::cout << "ReadProcessMemory failed. Error: " << error << std::endl;
            return false;
        }
        total_read += bytes_read;
        current_address += bytes_read;
        current_buffer += bytes_read;
    }
    return true;
}

DWORD query_protection(HANDLE _process, LPVOID base) {
    MEMORY_BASIC_INFORMATION mem_info{};
    if (VirtualQueryEx(_process, base, &mem_info, sizeof(mem_info)) == 0) {
        std::wcerr << L"VirtualQueryEx failed. Error code: " << GetLastError() << std::endl;
        return 0;
    }
    return mem_info.Protect;
}

bool restore_memory(HANDLE _process, LPVOID base, SIZE_T sz_region) {
    DWORD old_protection;
    DWORD pid = GetProcessId(_process);
    std::wcout << L"[DEBUG][PID:" << pid << "] Restoring memory protection..." << std::endl;
    if (!VirtualProtectEx(_process, base, sz_region, PAGE_EXECUTE_READ, &old_protection)) {
        std::wcerr << L"VirtualProtectEx failed in restore_memory. Error code: " << GetLastError() << std::endl;
        return false;
    }
    return true;
}

bool remap_memory(HANDLE _process, LPVOID base, SIZE_T sz_region) {
    DWORD pid = GetProcessId(_process);
    std::wcout << L"[DEBUG][PID:" << pid << "] Allocating memory for copy buffer..." << std::endl;
    LPVOID allocation = VirtualAllocEx(_process, nullptr, sz_region, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (allocation == nullptr) {
        std::wcerr << L"VirtualAllocEx failed. Error code: " << GetLastError() << std::endl;
        return false;
    }

    LPVOID buffer = VirtualAlloc(nullptr, sz_region, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (buffer == nullptr) {
        std::wcerr << L"VirtualAlloc failed. Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        return false;
    }

    LPVOID buffer_ex = VirtualAllocEx(_process, nullptr, sz_region, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (buffer_ex == nullptr) {
        std::wcerr << L"VirtualAllocEx failed for buffer_ex. Error code: " << GetLastError() << std::endl;
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        return false;
    }

    std::wcout << L"Reading process memory..." << std::endl;
    if (!ReadProcessMemory(_process, base, buffer, sz_region, nullptr)) {
        std::wcerr << L"ReadProcessMemory failed. Error code: " << GetLastError() << std::endl;
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        VirtualFreeEx(_process, buffer_ex, 0, MEM_RELEASE);
        return false;
    }

    HANDLE _section = nullptr;
    LARGE_INTEGER max_section{};
    max_section.QuadPart = sz_region;

    std::wcout << L"Creating section..." << std::endl;
    NTSTATUS status = NtCreateSection(&_section, SECTION_ALL_ACCESS, nullptr, &max_section, PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
    if (status != STATUS_SUCCESS) {
        std::wcerr << L"NtCreateSection failed. NTSTATUS: 0x" << std::hex << status << std::endl;
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        VirtualFreeEx(_process, buffer_ex, 0, MEM_RELEASE);
        return false;
    }

    std::wcout << L"Unmapping view of section..." << std::endl;
    status = NtUnmapViewOfSection(_process, base);
    if (status != STATUS_SUCCESS) {
        std::wcerr << L"NtUnmapViewOfSection failed. NTSTATUS: 0x" << std::hex << status << std::endl;
        CloseHandle(_section);
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        VirtualFreeEx(_process, buffer_ex, 0, MEM_RELEASE);
        return false;
    }

    std::wcout << L"Mapping view of section..." << std::endl;
    LPVOID view_base = base;
    LARGE_INTEGER section_offset = { 0 };
    SIZE_T sz_view = 0;
    status = NtMapViewOfSection(_section, _process, &view_base, 0, sz_region, &section_offset, &sz_view, ViewUnmap, 0, PAGE_EXECUTE_READWRITE);
    if (status != STATUS_SUCCESS) {
        std::wcerr << L"NtMapViewOfSection failed. NTSTATUS: 0x" << std::hex << status << std::endl;
        CloseHandle(_section);
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        VirtualFreeEx(_process, buffer_ex, 0, MEM_RELEASE);
        return false;
    }

    std::wcout << L"Writing memory back to the updated region..." << std::endl;
    SIZE_T bytes_written;
    if (!WriteProcessMemory(_process, view_base, buffer, sz_view, &bytes_written)) {
        std::wcerr << L"WriteProcessMemory failed for view_base. Error code: " << GetLastError() << std::endl;
        NtUnmapViewOfSection(_process, view_base);
        CloseHandle(_section);
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        VirtualFreeEx(_process, buffer_ex, 0, MEM_RELEASE);
        return false;
    }

    if (!WriteProcessMemory(_process, buffer_ex, buffer, sz_view, &bytes_written)) {
        std::wcerr << L"WriteProcessMemory failed for buffer_ex. Error code: " << GetLastError() << std::endl;
        NtUnmapViewOfSection(_process, view_base);
        CloseHandle(_section);
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        VirtualFreeEx(_process, buffer_ex, 0, MEM_RELEASE);
        return false;
    }

    DWORD old_protection;
    if (!VirtualProtectEx(_process, buffer_ex, sz_view, PAGE_EXECUTE_READWRITE, &old_protection)) {
        std::wcerr << L"VirtualProtectEx failed. Error code: " << GetLastError() << std::endl;
        NtUnmapViewOfSection(_process, view_base);
        CloseHandle(_section);
        VirtualFree(buffer, 0, MEM_RELEASE);
        VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
        VirtualFreeEx(_process, buffer_ex, 0, MEM_RELEASE);
        return false;
    }

    VirtualFree(buffer, 0, MEM_RELEASE);
    VirtualFreeEx(_process, allocation, 0, MEM_RELEASE);
    CloseHandle(_section);

    return true;
}

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

export int change_protection(DWORD pid, DWORD protection) {
    std::wcout << L"[DEBUG][PID:" << pid << "] Entering change_protection function. Requested protection: " << protection << std::endl;

    // Open the specific process using the PID
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (process == nullptr) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] OpenProcess failed. Error code: " << GetLastError() << std::endl;
        return 1;
    }

    // Verify that the opened process is actually Wow.exe
    WCHAR processName[MAX_PATH];
    if (GetModuleBaseNameW(process, NULL, processName, MAX_PATH) == 0) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] GetModuleBaseNameW failed. Error code: " << GetLastError() << std::endl;
        CloseHandle(process);
        return 1;
    }

    if (wcscmp(processName, L"Wow.exe") != 0) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] Process is not Wow.exe" << std::endl;
        CloseHandle(process);
        return 1;
    }

    LPVOID base = base_address(process);
    if (base == nullptr) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] Failed to get base address of the process." << std::endl;
        CloseHandle(process);
        return 1;
    }

    MEMORY_BASIC_INFORMATION basicInformation{};
    std::wcout << L"[DEBUG][PID:" << pid << "] Querying process memory..." << std::endl;
    if (VirtualQueryEx(process, base, &basicInformation, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] VirtualQueryEx failed. Error code: " << GetLastError() << std::endl;
        CloseHandle(process);
        return 1;
    }

    SIZE_T sz_region = basicInformation.RegionSize;
    std::wcout << L"[DEBUG][PID:" << pid << "] Current protection: " << basicInformation.Protect << std::endl;

    std::wcout << L"[DEBUG][PID:" << pid << "] Suspending process..." << std::endl;
    NTSTATUS status = NtSuspendProcess(process);
    if (status != STATUS_SUCCESS) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] NtSuspendProcess failed. NTSTATUS: 0x" << std::hex << status << std::endl;
        CloseHandle(process);
        return 1;
    }

    bool operation_success = false;
    if (protection == PAGE_EXECUTE_READWRITE) {
        operation_success = remap_memory(process, base, sz_region);
    }
    else if (protection == PAGE_EXECUTE_READ) {
        operation_success = restore_memory(process, base, sz_region);
    }

    if (!operation_success) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] Failed to change memory protection." << std::endl;
        NtResumeProcess(process);
        CloseHandle(process);
        return 1;
    }

    std::wcout << L"[DEBUG][PID:" << pid << "] Resuming process..." << std::endl;
    status = NtResumeProcess(process);
    if (status != STATUS_SUCCESS) {
        std::wcerr << L"[DEBUG][PID:" << pid << "] NtResumeProcess failed. NTSTATUS: 0x" << std::hex << status << std::endl;
        CloseHandle(process);
        return 1;
    }

    DWORD new_protection = query_protection(process, base);
    std::wcout << L"[DEBUG][PID:" << pid << "] New protection of the memory region: " << new_protection << std::endl;

    CloseHandle(process);

    if ((protection == PAGE_EXECUTE_READWRITE && new_protection == PAGE_EXECUTE_READWRITE) ||
        (protection == PAGE_EXECUTE_READ && new_protection == PAGE_EXECUTE_READ)) {
        std::wcout << L"[DEBUG][PID:" << pid << "] Operation completed successfully. New protection: " << new_protection
            << L", Desired protection: " << protection << std::endl;
        return 0;  // Success
    }
    else {
        std::wcerr << L"[DEBUG][PID:" << pid << "] Failed to change memory protection. Current protection: " << new_protection
            << L", Desired protection: " << protection << std::endl;
        return 1;  // Failure
    }
}