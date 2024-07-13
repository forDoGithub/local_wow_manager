#include "framework.h"
#include "imgui.h"
#include "imfilebrowser.h"
import DrawFileDialog;
import DrawActivePids;
import DrawOldPids;
import DrawLaunchingPids;
import ConfigManager;
import pid_data_manager;
import wow_start;

export module account_launcher_frame;

export class AccountLauncherFrame {
public:
    AccountLauncherFrame() : configManager_("config.json") {
        // Initialize with JSON config file
    }

    void Draw(const char* title, bool* p_open = nullptr) {
        ImGui::Begin(title, p_open);

        CheckRunningProcesses();
        DrawConfigManager(configManager_);
        DrawActivePIDs();
        DrawOldPIDs();
        DrawLaunchingPids();

        ImGui::End();
    }

private:
    ConfigManager configManager_;

    void CheckRunningProcesses() {
        auto& pidDataManager = PIDDataManager::getInstance();
        const auto& activePids = pidDataManager.getAllActivePIDData();
        for (const auto& pidDataPair : activePids) {
            DWORD exitCode;
            HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pidDataPair.first);
            if (processHandle != NULL) {
                if (!GetExitCodeProcess(processHandle, &exitCode) || exitCode != STILL_ACTIVE) {
                    pidDataManager.deactivatePID(pidDataPair.first);
                }
                CloseHandle(processHandle);
            }
        }
    }
};