#pragma once

#include "framework.h"
#include <unordered_map>
#include <chrono>
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include <winsock2.h>
#include <openssl/ssl.h>
#include <d3d11.h>
#include <thread>

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Define a struct to hold the data for each PID
struct PIDData {
	HWND hwnd;
	std::chrono::system_clock::time_point timestamp;
	std::string accountEmail;
	bool imguiWindowOpen;
	int totalMessagesSent = 0;
	int totalMessagesReceived = 0;
	std::vector<std::string> messageRecord;
};

// Function to get window handle from PID
HWND getWindowHandleFromPID(DWORD pid);

//NAV REQUEST FROM GAME HANDLING
void navigation_transmission(SSL*& ssl);

struct ACCOUNT {
	int pid;
	std::optional<std::string> accountEmail;
	std::optional<int> health;

	ACCOUNT(const nlohmann::json& j);
	nlohmann::json to_json() const;
};

//MESSAGE TRANSMISSION
std::chrono::system_clock::time_point readDataFromSSL(SSL*& ssl, char* dataBuffer, size_t bufferSize);
void handleMessage(SSL*& ssl, const char* dataBuffer, std::chrono::system_clock::time_point timestamp);
std::chrono::system_clock::time_point message_transmission(SSL*& ssl);

// Template function to handle connections
template <typename TransmissionFunction>
void handle_client(SOCKET client_socket, SSL_CTX* ssl_ctx, TransmissionFunction transmission);

template <typename TransmissionFunction>
void handle_connections(SOCKET socket, SSL_CTX* ssl_ctx, TransmissionFunction transmission);

void cleanup();

// Function to initialize ImGui
void InitializeImGui(HWND hwnd, ImGuiIO& io, bool& show_demo_window, bool& show_another_window, ImVec4& clear_color);

class Window {
public:
	Window();
	~Window();
	HWND getHwnd() const;

private:
	WNDCLASSEXW wc;
	HWND hwnd;
}; #pragma once
