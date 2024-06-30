#pragma once
#include <framework.h>
//from https://github.com/zodiacon/WindowsInternals/blob/master/APISetMap/ApiSet.h

#define API_SET_SCHEMA_ENTRY_FLAGS_SEALED 1

typedef struct _API_SET_NAMESPACE {
    ULONG Version;
    ULONG Size;
    ULONG Flags;
    ULONG Count;
    ULONG EntryOffset;
    ULONG HashOffset;
    ULONG HashFactor;
} API_SET_NAMESPACE, * PAPI_SET_NAMESPACE;

typedef struct _API_SET_HASH_ENTRY {
    ULONG Hash;
    ULONG Index;
} API_SET_HASH_ENTRY, * PAPI_SET_HASH_ENTRY;

typedef struct _API_SET_NAMESPACE_ENTRY {
    ULONG Flags;
    ULONG NameOffset;
    ULONG NameLength;
    ULONG HashedLength;
    ULONG ValueOffset;
    ULONG ValueCount;
} API_SET_NAMESPACE_ENTRY, * PAPI_SET_NAMESPACE_ENTRY;

typedef struct _API_SET_VALUE_ENTRY {
    ULONG Flags;
    ULONG NameOffset;
    ULONG NameLength;
    ULONG ValueOffset;
    ULONG ValueLength;
} API_SET_VALUE_ENTRY, * PAPI_SET_VALUE_ENTRY;

//from https://github.com/x64dbg/x64dbg/blob/development/src/dbg/ntdll/ntdll.h
#define GDI_HANDLE_BUFFER_SIZE32    34
#define GDI_HANDLE_BUFFER_SIZE64    60

#ifndef _WIN64
#define GDI_HANDLE_BUFFER_SIZE GDI_HANDLE_BUFFER_SIZE32
#else
#define GDI_HANDLE_BUFFER_SIZE GDI_HANDLE_BUFFER_SIZE64
#endif

namespace undocumented {
    namespace ntdll {

        typedef ULONG GDI_HANDLE_BUFFER32[GDI_HANDLE_BUFFER_SIZE32];
        typedef ULONG GDI_HANDLE_BUFFER64[GDI_HANDLE_BUFFER_SIZE64];
        typedef ULONG GDI_HANDLE_BUFFER[GDI_HANDLE_BUFFER_SIZE];

        typedef struct _PEB
        {
            BOOLEAN InheritedAddressSpace;
            BOOLEAN ReadImageFileExecOptions;
            BOOLEAN BeingDebugged;
            union
            {
                BOOLEAN BitField;
                struct
                {
                    BOOLEAN ImageUsesLargePages : 1;
                    BOOLEAN IsProtectedProcess : 1;
                    BOOLEAN IsImageDynamicallyRelocated : 1;
                    BOOLEAN SkipPatchingUser32Forwarders : 1;
                    BOOLEAN IsPackagedProcess : 1;
                    BOOLEAN IsAppContainer : 1;
                    BOOLEAN IsProtectedProcessLight : 1;
                    BOOLEAN IsLongPathAwareProcess : 1;
                } s1;
            } u1;

            HANDLE Mutant;

            PVOID ImageBaseAddress;
            PPEB_LDR_DATA Ldr;
            PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
            PVOID SubSystemData;
            PVOID ProcessHeap;
            PRTL_CRITICAL_SECTION FastPebLock;
            PVOID AtlThunkSListPtr;
            PVOID IFEOKey;
            union
            {
                ULONG CrossProcessFlags;
                struct
                {
                    ULONG ProcessInJob : 1;
                    ULONG ProcessInitializing : 1;
                    ULONG ProcessUsingVEH : 1;
                    ULONG ProcessUsingVCH : 1;
                    ULONG ProcessUsingFTH : 1;
                    ULONG ProcessPreviouslyThrottled : 1;
                    ULONG ProcessCurrentlyThrottled : 1;
                    ULONG ReservedBits0 : 25;
                } s2;
            } u2;
            union
            {
                PVOID KernelCallbackTable;
                PVOID UserSharedInfoPtr;
            } u3;
            ULONG SystemReserved[1];
            ULONG AtlThunkSListPtr32;
            PVOID ApiSetMap;
            ULONG TlsExpansionCounter;
            PVOID TlsBitmap;
            ULONG TlsBitmapBits[2];

            PVOID ReadOnlySharedMemoryBase;
            PVOID SharedData; // HotpatchInformation
            PVOID* ReadOnlyStaticServerData;

            PVOID AnsiCodePageData;
            PVOID OemCodePageData;
            PVOID UnicodeCaseTableData; // PNLSTABLEINFO

            ULONG NumberOfProcessors;
            ULONG NtGlobalFlag;

            LARGE_INTEGER CriticalSectionTimeout;
            SIZE_T HeapSegmentReserve;
            SIZE_T HeapSegmentCommit;
            SIZE_T HeapDeCommitTotalFreeThreshold;
            SIZE_T HeapDeCommitFreeBlockThreshold;

            ULONG NumberOfHeaps;
            ULONG MaximumNumberOfHeaps;
            PVOID* ProcessHeaps; // PHEAP

            PVOID GdiSharedHandleTable;
            PVOID ProcessStarterHelper;
            ULONG GdiDCAttributeList;

            PRTL_CRITICAL_SECTION LoaderLock;

            ULONG OSMajorVersion;
            ULONG OSMinorVersion;
            USHORT OSBuildNumber;
            USHORT OSCSDVersion;
            ULONG OSPlatformId;
            ULONG ImageSubsystem;
            ULONG ImageSubsystemMajorVersion;
            ULONG ImageSubsystemMinorVersion;
            ULONG_PTR ActiveProcessAffinityMask;
            GDI_HANDLE_BUFFER GdiHandleBuffer;
            PVOID PostProcessInitRoutine;

            PVOID TlsExpansionBitmap;
            ULONG TlsExpansionBitmapBits[32];

            ULONG SessionId;

            ULARGE_INTEGER AppCompatFlags;
            ULARGE_INTEGER AppCompatFlagsUser;
            PVOID pShimData;
            PVOID AppCompatInfo; // APPCOMPAT_EXE_DATA

            UNICODE_STRING CSDVersion;

            PVOID ActivationContextData; // ACTIVATION_CONTEXT_DATA
            PVOID ProcessAssemblyStorageMap; // ASSEMBLY_STORAGE_MAP
            PVOID SystemDefaultActivationContextData; // ACTIVATION_CONTEXT_DATA
            PVOID SystemAssemblyStorageMap; // ASSEMBLY_STORAGE_MAP

            SIZE_T MinimumStackCommit;

            PVOID* FlsCallback;
            LIST_ENTRY FlsListHead;
            PVOID FlsBitmap;
            ULONG FlsBitmapBits[FLS_MAXIMUM_AVAILABLE / (sizeof(ULONG) * 8)];
            ULONG FlsHighIndex;

            PVOID WerRegistrationData;
            PVOID WerShipAssertPtr;
            PVOID pUnused; // pContextData
            PVOID pImageHeaderHash;
            union
            {
                ULONG TracingFlags;
                struct
                {
                    ULONG HeapTracingEnabled : 1;
                    ULONG CritSecTracingEnabled : 1;
                    ULONG LibLoaderTracingEnabled : 1;
                    ULONG SpareTracingBits : 29;
                } s3;
            } u4;
            ULONGLONG CsrServerReadOnlySharedMemoryBase;
            PVOID TppWorkerpListLock;
            LIST_ENTRY TppWorkerpList;
            PVOID WaitOnAddressHashTable[128];
            PVOID TelemetryCoverageHeader; // REDSTONE3
            ULONG CloudFileFlags;
        } PEB, * PPEB;
    }
}

#define VIRTUAL_DLL_PREFIX "api-ms"

static NTSTATUS(__stdcall* NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval) = (NTSTATUS(__stdcall*)(BOOL, PLARGE_INTEGER)) GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtDelayExecution");
static NTSTATUS(__stdcall* ZwSetTimerResolution)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG)) GetProcAddress(GetModuleHandle(L"ntdll.dll"), "ZwSetTimerResolution");

class Utils {
private:
    Utils() {};
public:
    //https://stackoverflow.com/questions/85122/how-to-make-thread-sleep-less-than-a-millisecond-on-windows/31411628#31411628
    //todo: syscall NtDelayExecution
    static void sleep(float milliseconds) {
        static bool once = true;
        if (once) {
            ULONG actualResolution;
            ZwSetTimerResolution(1, true, &actualResolution);
            once = false;
        }

        LARGE_INTEGER interval;
        interval.QuadPart = -1 * (int)(milliseconds * 10000.0f);
        NtDelayExecution(false, &interval);
    }
    //https://github.com/zodiacon/WindowsInternals/blob/master/APISetMap/APISetMap.cpp
    static std::wstring utf8_to_wstring(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
        return wstr;
    }

    static std::string wstring_to_utf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
        return str;
    }
    static std::string dll_from_map(const std::string& api_set) {
        // Convert api_set directly to wstring
        std::wstring wapi_set = utf8_to_wstring(api_set);

        undocumented::ntdll::PEB* peb = reinterpret_cast<undocumented::ntdll::PEB*>(NtCurrentTeb()->ProcessEnvironmentBlock);
        API_SET_NAMESPACE* apiSetMap = static_cast<API_SET_NAMESPACE*>(peb->ApiSetMap);
        ULONG_PTR apiSetMapAsNumber = reinterpret_cast<ULONG_PTR>(apiSetMap);
        API_SET_NAMESPACE_ENTRY* nsEntry = reinterpret_cast<API_SET_NAMESPACE_ENTRY*>((apiSetMap->EntryOffset + apiSetMapAsNumber));
        for (ULONG i = 0; i < apiSetMap->Count; i++) {
            UNICODE_STRING nameString, valueString;
            nameString.MaximumLength = static_cast<USHORT>(nsEntry->NameLength);
            nameString.Length = static_cast<USHORT>(nsEntry->NameLength);
            nameString.Buffer = reinterpret_cast<PWCHAR>(apiSetMapAsNumber + nsEntry->NameOffset);

            std::wstring name = std::wstring(nameString.Buffer, nameString.Length / sizeof(WCHAR)) + L".dll";
            // Convert name directly to string
            std::string name_utf8 = wstring_to_utf8(name);

            // Use case-insensitive comparison
            if (_wcsicmp(wapi_set.c_str(), name.c_str()) == 0) {
                API_SET_VALUE_ENTRY* valueEntry = reinterpret_cast<API_SET_VALUE_ENTRY*>(apiSetMapAsNumber + nsEntry->ValueOffset);
                if (nsEntry->ValueCount == 0)
                    return "";
                valueString.Buffer = reinterpret_cast<PWCHAR>(apiSetMapAsNumber + valueEntry->ValueOffset);
                valueString.MaximumLength = static_cast<USHORT>(valueEntry->ValueLength);
                valueString.Length = static_cast<USHORT>(valueEntry->ValueLength);
                std::wstring value = std::wstring(valueString.Buffer, valueString.Length / sizeof(WCHAR));
                // Convert value directly to string
                std::string value_utf8 = wstring_to_utf8(value);
                return value_utf8;
            }
            nsEntry++;
        }
        return "";
    }
    static void print_and_exit(const char* format, ...) {
        printf("Error: ");
        va_list arglist;
        va_start(arglist, format);
        vprintf(format, arglist);
        va_end(arglist);
        std::exit(1);
    }
};