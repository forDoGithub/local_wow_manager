#include "framework.h"

import account_launcher_frame;
import network_module;
import dx11_module;
//import pathfinding;
import imgui_module;
import socket_utils;
import pid_data_manager;

constexpr float MIN_DISTANCE = 1.05f;
const int NAVIGATION_SOCKET = 54000;
const int MESSAGE_SOCKET = 54001;
//const int REQUEST_SOCKET = 54002; // Added request socket

//SOCKET navigation_socket;
SOCKET message_socket;

SSL_CTX* ssl_ctx;

static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

std::chrono::time_point<std::chrono::system_clock> start_time;

void cleanup_imgui()
{
	// New code to write PID data to log file
	auto end_time = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end_time - start_time;
	std::time_t end_time_t = std::chrono::system_clock::to_time_t(end_time);

	std::cout << "Start time: " << start_time.time_since_epoch().count() << std::endl;
	std::cout << "End time: " << end_time.time_since_epoch().count() << std::endl;
	std::cout << "Elapsed seconds: " << elapsed_seconds.count() << std::endl;

	std::stringstream ss;
	std::tm tm;
	localtime_s(&tm, &end_time_t);
	ss << std::put_time(&tm, "%m.%d.%y # %I.%M%p # ");  // Use "." as separator and "%p" for AM/PM
	ss << std::chrono::duration_cast<std::chrono::hours>(elapsed_seconds).count() << "h ";
	ss << std::chrono::duration_cast<std::chrono::minutes>(elapsed_seconds).count() % 60 << "m ";
	ss << std::chrono::duration_cast<std::chrono::seconds>(elapsed_seconds).count() % 60 << "s";

	// Convert AM/PM to lowercase
	std::string filename = ss.str();
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

	std::cout << "Filename string: " << filename << std::endl;

	filename = "D:/logs/" + filename;



	std::ofstream log_file(filename);
	if (log_file.is_open()) {
		const auto& allActivePIDData = PIDDataManager::getInstance().getAllActivePIDData();
		log_file << "ACTIVE PIDS:\n";
		for (const auto& [pid, dataPtr] : allActivePIDData) {
			const PIDData& data = *dataPtr;
			log_file << "PID: " << pid << "\n";
			log_file << "Start time: " << data.startTime.time_since_epoch().count() << "\n";
			log_file << "End time: " << data.endTime.time_since_epoch().count() << "\n";
			std::chrono::duration<double> elapsed_seconds = data.endTime - data.startTime;
			log_file << "Running time: " << elapsed_seconds.count() << " seconds\n";

			log_file << "Last State:\n";

			for (const auto& kv : data.data) {
				try {
					std::visit([&log_file, &kv](const auto& arg) {
						using ArgType = std::decay_t<decltype(arg)>;
						if constexpr (std::is_same_v<ArgType, int>) {
							log_file << kv.first << ": " << arg << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::string>) {
							log_file << kv.first << ": " << arg << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, bool>) {
							log_file << kv.first << ": " << (arg ? "True" : "False") << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, HWND>) {
							log_file << kv.first << ": " << static_cast<void*>(arg) << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::intptr_t>) {
							log_file << kv.first << ": " << reinterpret_cast<void*>(arg) << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::chrono::system_clock::time_point>) {
							auto time_t = std::chrono::system_clock::to_time_t(arg);
							char buffer[26];
							ctime_s(buffer, sizeof(buffer), &time_t);
							log_file << kv.first << ": " << buffer << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::vector<std::string>>) {
							log_file << kv.first << ": ";
							for (const auto& item : arg) {
								log_file << item << ", ";
							}
							log_file << "\n";
						}
						}, kv.second);
				}
				catch (const std::exception& e) {
					log_file << "Could not write line: " << e.what() << "\n";
				}
				catch (...) {
					log_file << "Could not write line: unknown reason\n";
				}
			}

			log_file << "Sent Messages:\n";
			for (const auto& message : data.sentMessages) {
				log_file << message << "\n";
			}
			log_file << "Received Messages:\n";
			for (const auto& message : data.receivedMessages) {
				log_file << message << "\n";
			}


			log_file << "\n";

		}

		const auto& allOldPIDData = PIDDataManager::getInstance().getAllOldPIDData();
		log_file << "OLD PIDS:\n";
		for (const auto& [pid, dataPtr] : allOldPIDData) {
			const PIDData& data = *dataPtr;
			log_file << "PID: " << pid << "\n";
			log_file << "Start time: " << data.startTime.time_since_epoch().count() << "\n";
			log_file << "End time: " << data.endTime.time_since_epoch().count() << "\n";
			std::chrono::duration<double> elapsed_seconds = data.endTime - data.startTime;
			log_file << "Running time: " << elapsed_seconds.count() << " seconds\n";

			log_file << "Last State:\n";

			for (const auto& kv : data.data) {
				try {
					std::visit([&log_file, &kv](const auto& arg) {
						using ArgType = std::decay_t<decltype(arg)>;
						if constexpr (std::is_same_v<ArgType, int>) {
							log_file << kv.first << ": " << arg << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::string>) {
							log_file << kv.first << ": " << arg << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, bool>) {
							log_file << kv.first << ": " << (arg ? "True" : "False") << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, HWND>) {
							log_file << kv.first << ": " << static_cast<void*>(arg) << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::intptr_t>) {
							log_file << kv.first << ": " << reinterpret_cast<void*>(arg) << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::chrono::system_clock::time_point>) {
							auto time_t = std::chrono::system_clock::to_time_t(arg);
							char buffer[26];
							ctime_s(buffer, sizeof(buffer), &time_t);
							log_file << kv.first << ": " << buffer << "\n";
						}
						else if constexpr (std::is_same_v<ArgType, std::vector<std::string>>) {
							log_file << kv.first << ": ";
							for (const auto& item : arg) {
								log_file << item << ", ";
							}
							log_file << "\n";
						}
						}, kv.second);
				}
				catch (const std::exception& e) {
					log_file << "Could not write line: " << e.what() << "\n";
				}
				catch (...) {
					log_file << "Could not write line: unknown reason\n";
				}
			}

			log_file << "Sent Messages:\n";
			for (const auto& message : data.sentMessages) {
				log_file << message << "\n";
			}
			log_file << "Received Messages:\n";
			for (const auto& message : data.receivedMessages) {
				log_file << message << "\n";
			}

			log_file << "\n";

		}

		log_file.close();
	}
	else {
		char error_msg[256];
		strerror_s(error_msg, sizeof(error_msg), errno);
		std::cerr << "Error opening file: " << error_msg << std::endl;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	dx11::CleanupDeviceD3D();
}


void cleanup_server() {
	SSL_CTX_free(ssl_ctx);
	//closesocket(navigation_socket);
	closesocket(message_socket);
	WSACleanup();
	//dtFreeNavMeshQuery(query);
	//dtFreeNavMesh(mesh);
	
	
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
		g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_CLOSE:
		cleanup_server(); // Free console when main window is being closed
		cleanup_imgui();  // Call cleanup_imgui function when main window is being closed
		FreeConsole();
		::PostQuitMessage(0);
		return 0;
	case WM_DESTROY:
		cleanup_server();
		cleanup_imgui();
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
class Window {
public:
	Window() {
		wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
		::RegisterClassExW(&wc);
		hwnd = ::CreateWindowW(wc.lpszClassName, L"!!!SERVER!!!", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);
	}

	~Window() {
		::DestroyWindow(hwnd);
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
	}

	HWND getHwnd() const {
		return hwnd;
	}

private:
	WNDCLASSEXW wc;
	HWND hwnd;
};

void cleanup() {
	cleanup_imgui();
	cleanup_server();
	FreeConsole();
}

int main()
{
	WSADATA ws_data{};
	ssl_ctx = init_server_ctx();
	load_certificates(ssl_ctx, "cert.crt", "private.key");
	if (!ws_data.wVersion) {
		WORD ver = MAKEWORD(2, 2);
		int ws_ok = WSAStartup(ver, &ws_data);
		if (ws_ok != 0) {
			std::cerr << "Can't initialize winsock! Quitting" << std::endl;
			return -1;
		}
	}
	//navigation_socket = create_and_bind_socket(54000);
	message_socket = create_and_bind_socket(54001);
	//listen(navigation_socket, SOMAXCONN);
	listen(message_socket, SOMAXCONN);
	//std::thread navigation_thread([=]() { network::handle_connections(navigation_socket, ssl_ctx, network::navigation_transmission); });
	std::thread message_thread([=]() { network::handle_connections(message_socket, ssl_ctx, network::message_transmission); });
	atexit(cleanup);

	Window window;
	if (!dx11::CreateDeviceD3D(window.getHwnd()))
	{
		dx11::CleanupDeviceD3D();
		return 1;
	}

	::ShowWindow(window.getHwnd(), SW_SHOWDEFAULT);
	::UpdateWindow(window.getHwnd());

	ImGuiIO io;
	ImVec4 clear_color;
	imgui::InitializeImGui(window.getHwnd(), io, imgui::show_demo_window, imgui::show_another_window, clear_color);

	// Set start_time to the current time
	start_time = std::chrono::system_clock::now();

	// Main loop
	bool done = false;
	
	while (!done)
	{
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;
		if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
		{
			dx11::CleanupRenderTarget();
			dx11::g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
			g_ResizeWidth = g_ResizeHeight = 0;
			dx11::CreateRenderTarget();
		}
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		
		if (imgui::show_demo_window)
			ImGui::ShowDemoWindow(&imgui::show_demo_window);

		AccountLauncherFrame account_launcher_frame;
		account_launcher_frame.Draw("ACCOUNT LAUNCHER");
		network::server_log.Draw("SERVER : ");
		ImGui::Render();
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		dx11::g_pd3dDeviceContext->OMSetRenderTargets(1, &dx11::g_mainRenderTargetView, nullptr);
		dx11::g_pd3dDeviceContext->ClearRenderTargetView(dx11::g_mainRenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		dx11::g_pSwapChain->Present(1, 0); // Present with vsync
		//g_pSwapChain->Present(0, 0); // Present without vsync
	}

	::DestroyWindow(window.getHwnd());
	//navigation_thread.join();
	//message_thread.join();

	return 0;
}