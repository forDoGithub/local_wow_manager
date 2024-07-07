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
    // Query the memory region
    MEMORY_BASIC_INFORMATION mem_info{};
    SIZE_T bytes_returned{};

    if (_process && VirtualQueryEx(_process, base, &mem_info, sizeof(mem_info)) == 0) {
        std::wcerr << L"VirtualQueryEx failed. Error code: " << GetLastError() << std::endl;

        CloseHandle(_process);
        return 1;
    }

    // Print the protection of the memory region
    DWORD protection = mem_info.Protect;

    return protection;
}

void restore_memory(HANDLE _process, LPVOID base, SIZE_T sz_region) {
    DWORD old_protection;

    if (!VirtualProtectEx(_process, base, sz_region, PAGE_EXECUTE_READ, &old_protection)) {
        native_error("VirtualProtectEx", GetLastError());
    }
}

void remap_memory(HANDLE _process, LPVOID base, int sz_region) {
    std::cout << "Allocating memory for copy buffer..." << std::endl;

    LPVOID allocation = VirtualAllocEx(_process, nullptr, sz_region, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (allocation == nullptr)
        native_error("VirtualAllocEx", GetLastError());

    LPVOID buffer = VirtualAlloc(nullptr, sz_region, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    LPVOID buffer_ex = VirtualAllocEx(_process, nullptr, sz_region, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    std::cout << "Reading process memory..." << std::endl;

    if (!read_memory(_process, base, buffer, sz_region))
        native_error("ReadProcessMemory", GetLastError());

    HANDLE _section = nullptr;
    LARGE_INTEGER max_section{};
    max_section.QuadPart = sz_region;

    std::cout << "Creating section..." << std::endl;

    NTSTATUS status = NtCreateSection(&_section, SECTION_ALL_ACCESS, nullptr, &max_section, PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
    if (status != STATUS_SUCCESS)
        native_error("NtCreateSection", status);

    std::cout << "Unmapping view of section..." << std::endl;

    status = NtUnmapViewOfSection(_process, base);

    if (status != STATUS_SUCCESS)
        native_error("NtUnmapViewOfSection", status);

    std::cout << "Mapping view of section..." << std::endl;

    LPVOID view_base = base;
    LARGE_INTEGER section_offset = { 0 };
    SIZE_T sz_view = 0;

    status = NtMapViewOfSection(_section, _process, &view_base, 0, sz_region, &section_offset, &sz_view, ViewUnmap, 0, PAGE_EXECUTE_READWRITE);

    if (status != STATUS_SUCCESS)
        native_error("NtMapViewOfSection", status);

    std::cout << "Writing memory back to the updated region..." << std::endl;

    SIZE_T bytes_written;

    if (!WriteProcessMemory(_process, view_base, buffer, (SIZE_T)sz_view, &bytes_written))
        native_error("WriteProcessMemory", GetLastError());

    if (!WriteProcessMemory(_process, buffer_ex, buffer, (SIZE_T)sz_view, &bytes_written))
        native_error("WriteProcessMemory", GetLastError());

    DWORD old_protection;

    if (!VirtualProtectEx(_process, buffer_ex, (SIZE_T)sz_view, PAGE_EXECUTE_READWRITE, &old_protection))
        native_error("VirtualProtectEx", GetLastError());

    if (!VirtualFree(buffer, 0, MEM_RELEASE))
        native_error("VirtualFree", GetLastError());

    CloseHandle(_section);
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

export int change_protection(DWORD protection) {
    std::wcout << L"Creating process snapshot..." << std::endl;

    PROCESSENTRY32W pe32{};
    HANDLE _snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (_snapshot == INVALID_HANDLE_VALUE)
        native_error("CreateToolhelp32Snapshot", GetLastError());

    Process32FirstW(_snapshot, &pe32);

    HANDLE _process = nullptr;

    do {
        if (wcscmp(pe32.szExeFile, L"Wow.exe") == 0) {
            _process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
            break;
        }
    } while (Process32NextW(_snapshot, &pe32));

    if (_snapshot)
        CloseHandle(_snapshot);

    if (_process == nullptr)
        native_error("OpenProcess", GetLastError());

    MEMORY_BASIC_INFORMATION basicInformation{};
    LPVOID base;
    SIZE_T sz_region;

    std::wcout << L"Querying process memory..." << std::endl;

    if (_process && VirtualQueryEx(_process, base_address(_process), &basicInformation, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
        native_error("VirtualQueryEx", GetLastError());

    base = basicInformation.BaseAddress;
    sz_region = basicInformation.RegionSize;

    std::wcout << L"Suspending process..." << std::endl;

    NTSTATUS status = NtSuspendProcess(_process);

    if (status != STATUS_SUCCESS)
        native_error("NtSuspendProcess", status);

    if (protection == PAGE_EXECUTE_READWRITE)
        remap_memory(_process, base, (int)sz_region);
    else if (protection == PAGE_EXECUTE_READ)
        restore_memory(_process, base, (int)sz_region);

    std::wcout << L"Resuming process..." << std::endl;

    status = NtResumeProcess(_process);

    if (status != STATUS_SUCCESS)
        native_error("NtResumeProcess", status);

    std::wcout << L"Protection of the memory region: " << query_protection(_process, base) << std::endl;

    // Close the process handle
    CloseHandle(_process);

    std::wcout << L"Operation completed successfully." << std::endl;

    return 0;
}