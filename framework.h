#pragma once
#define WIN32_LEAN_AND_MEAN
// STANDARD LIBRARY
#include <cstdio>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
// DETOUR
#include "detourCommon.h"
#include "detourNavMesh.h"
#include "detourNavMeshQuery.h"
#pragma comment(lib, "detour.lib")

// OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>

// WS2
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")

// VECTOR 3
//#include "vector.h"

// 
#include <chrono>
#include <windows.h>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <optional>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <d3d11.h>
#pragma comment (lib, "d3d11.lib")

#include <variant>
#include <set>
#include <mutex>
#include <thread>

#include <winsock2.h>


#include <psapi.h>
#include <tlhelp32.h>
#include <winternl.h>

#include <codecvt>
#include <cstdint>

#include <nt.h>
#include <winternl.h>
#include <ntstatus.h>
#pragma comment(lib, "ntdll.lib")

#include <vector.h>

#include <algorithm>;