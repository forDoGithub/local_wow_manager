#include "framework.h"

import dx11_module;
export module imgui_module;
export namespace imgui
{
	bool show_demo_window;
	bool show_another_window;
	void InitializeImGui(HWND hwnd, ImGuiIO& io, bool& show_demo_window, bool& show_another_window, ImVec4& clear_color)
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init((ID3D11Device*)dx11::g_pd3dDevice, (ID3D11DeviceContext*)dx11::g_pd3dDeviceContext);
		// Our state
		show_demo_window = true;
		clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	}

	
};
