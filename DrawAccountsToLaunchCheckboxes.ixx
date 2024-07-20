#include "framework.h"
#include "imgui.h"
#include "imfilebrowser.h"
import ConfigManager;

export module DrawAccountsToLaunchCheckboxes;

export bool account_checkboxes_[5] = { false, false, false, false, false };

//export void DrawOptionsMenu(ConfigManager& configManager) {
//    if (ImGui::BeginPopup("Options")) {
//        const auto& accounts = configManager.GetAccounts();
//        static ImGui::FileBrowser fileDialog;
//        static int currentScriptAccount = -1;
//
//        for (const auto& account : accounts) {
//            bool active = account.active;
//            if (ImGui::Checkbox(("Account " + std::to_string(account.accountNumber) + ": " + account.characterName).c_str(), &active)) {
//                configManager.SetAccountCheckbox(account.accountNumber, active);
//            }
//
//            ImGui::SameLine();
//            bool relog = account.relog;
//            if (ImGui::Checkbox(("Relog##" + std::to_string(account.accountNumber)).c_str(), &relog)) {
//                configManager.SetAccountRelog(account.accountNumber, relog);
//            }
//
//            ImGui::SameLine();
//            bool autologin = account.autologin;
//            if (ImGui::Checkbox(("Auto Login##" + std::to_string(account.accountNumber)).c_str(), &autologin)) {
//                configManager.SetAccountAutoLogin(account.accountNumber, autologin);
//            }
//
//            ImGui::SameLine();
//            bool initWorldScript = configManager.GetAccountInitWorldScript(account.accountNumber);
//            if (ImGui::Checkbox(("Init World Script##" + std::to_string(account.accountNumber)).c_str(), &initWorldScript)) {
//                configManager.SetAccountInitWorldScript(account.accountNumber, initWorldScript);
//            }
//
//            ImGui::SameLine();
//            bool autoload = configManager.GetAccountAutoload(account.accountNumber);
//            if (ImGui::Checkbox(("Autoload##" + std::to_string(account.accountNumber)).c_str(), &autoload)) {
//                configManager.SetAccountAutoload(account.accountNumber, autoload);
//            }
//
//            ImGui::SameLine();
//            if (ImGui::Button(("Load Script##" + std::to_string(account.accountNumber)).c_str())) {
//                currentScriptAccount = account.accountNumber;
//                fileDialog.SetTitle("Select Script for Account " + std::to_string(account.accountNumber));
//                fileDialog.SetTypeFilters({ ".lua", ".txt" });
//                fileDialog.Open();
//            }
//
//            ImGui::SameLine();
//            ImGui::Text("Current Script: %s", configManager.GetAccountScript(account.accountNumber).c_str());
//        }
//
//        // Display the file dialog
//        fileDialog.Display();
//
//        if (fileDialog.HasSelected()) {
//            if (currentScriptAccount != -1) {
//                std::string scriptPath = fileDialog.GetSelected().string();
//                configManager.SetAccountScript(currentScriptAccount, scriptPath);
//                currentScriptAccount = -1;
//            }
//            fileDialog.ClearSelected();
//        }
//
//        ImGui::EndPopup();
//    }
//
//    if (ImGui::Button("Options")) {
//        ImGui::OpenPopup("Options");
//    }
//}