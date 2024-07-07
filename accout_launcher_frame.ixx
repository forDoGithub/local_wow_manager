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
    std::map<DWORD, bool> pidStatusMap_;
    ConfigManager configManager_; // Add a ConfigManager member variable

    AccountLauncherFrame() : configManager_("config.ini") { // Initialize the ConfigManager with the config file name
        // Load the state of checkboxes from the config file
       /* for (int i = 0; i < 5; ++i) {
            account_checkboxes_[i] = configManager_.GetAccountCheckbox(i);
        }*/
    }

    ~AccountLauncherFrame() {
        // Save the state of checkboxes to the config file
        /*for (int i = 0; i < 5; ++i) {
            configManager_.SetAccountCheckbox(i, account_checkboxes_[i]);
        }*/
    }
  
 
    void Draw(const char* title, bool* p_open = nullptr) {
        ImGui::Begin("ACCOUNT LAUNCHER");

        // Check if the processes are still running
        auto& pidDataManager = PIDDataManager::getInstance();
        const auto& activePids = pidDataManager.getAllActivePIDData();
        for (const auto& pidDataPair : activePids) {
            DWORD exitCode;
            HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pidDataPair.first);
            if (!GetExitCodeProcess(processHandle, &exitCode) || exitCode != STILL_ACTIVE) {
                // If the process is not running, deactivate the PID
                //pidDataManager.deactivatePID(pidDataPair.first);
            }
            CloseHandle(processHandle);
        }

        DrawConfigManager();
        DrawActivePIDs();
        DrawOldPIDs();
        DrawLaunchingPids();

        ImGui::End();
    }
private:
    void LaunchSelectedAccounts() {
        // Read the account emails from your configuration file

        /*auto accountEmails = wow_start::ReadAccountEmails("\"D:\\LIVE CLASSIC 12.31.23.721.pm\\tetra_classic_era\\server\\config.ini\"");

        for (int i = 0; i < 5; ++i) {
            if (account_checkboxes_[i] && i < accountEmails.size()) {
                wow_start::LaunchAccount(accountEmails[i]);
            }
        }*/
        //wow_start::LaunchAccount("a@a.com");

    }
 
};