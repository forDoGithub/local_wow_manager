#define NOMINMAX
#include "imgui.h"
#include "imfilebrowser.h"
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

import wow_start;
import pid_data_manager;

export module DrawFileDialog;
struct ConfigAccount {
    int accountNumber;
    bool active;
    std::string email;
    std::string serverName;
    std::string characterName;
    bool relog;
};

export nlohmann::json configJson;
export std::vector<ConfigAccount> config_accounts;

std::string newConfigText;

ImGui::FileBrowser fileDialog;
std::string currentFile = "C:\\_\\local_wow_manager\\local_wow_manager\\x64\\Release\\config.json";
std::vector<std::string> configLines;

export void LoadConfigFile(const std::string& filename) {
    std::ifstream file(filename);
    std::string str((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    configJson = nlohmann::json::parse(str);
    std::istringstream iss(str);
    configLines.clear();
    for (std::string line; std::getline(iss, line); ) {
        configLines.push_back(line);
    }
    config_accounts.clear();
    for (const auto& accountJson : configJson["accounts"]) {
        ConfigAccount account;
        account.accountNumber = accountJson["accountNumber"];
        account.active = accountJson["active"];
        account.email = accountJson["email"];
        account.serverName = accountJson["serverName"];
        account.characterName = accountJson["characterName"];
        account.relog = accountJson["relog"];
        config_accounts.push_back(account);
    }
}

export void SaveConfigFile(const std::string& filename) {
    nlohmann::json newJson;
    newJson["accounts"] = nlohmann::json::array();
    for (const ConfigAccount& account : config_accounts) {
        nlohmann::json accountJson;
        accountJson["accountNumber"] = account.accountNumber;
        accountJson["active"] = account.active;
        accountJson["email"] = account.email;
        accountJson["serverName"] = account.serverName;
        accountJson["characterName"] = account.characterName;
        accountJson["relog"] = account.relog;
        newJson["accounts"].push_back(accountJson);
    }
    std::ofstream file(filename);
    file << newJson.dump(4);
}

bool showConfigEditBoxPopup = false;

void DrawEditorBox() {
    std::string configText;
    for (const auto& line : configLines) {
        configText += line + "\n";
    }
    std::array<char, 1024 * 16> buf{};
    std::copy_n(configText.begin(), std::min(configText.size(), buf.size() - 1), buf.begin());
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y -= ImGui::GetFrameHeightWithSpacing();
    bool textChanged = ImGui::InputTextMultiline("", buf.data(), buf.size(), size);
    if (textChanged) {
        newConfigText = std::string(buf.data());
    }
    if (ImGui::Button("Save")) {
        if (newConfigText.empty()) {
            return;
        }
        nlohmann::json json = nlohmann::json::parse(newConfigText);
        if (json.find("accounts") == json.end()) {
            json = { { "accounts", json } };
            newConfigText = json.dump();
        }
        std::ofstream file(currentFile);
        file << newConfigText;
        file.close();
        LoadConfigFile(currentFile);
    }
}

std::vector<std::pair<ConfigAccount, std::string>> wowstart_commands;


export void DrawConfigManager() {
    wowstart_commands.clear();

    ImGui::InputText("", &currentFile[0], currentFile.size(), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        fileDialog.Open();
    }
    fileDialog.Display();
    if (fileDialog.HasSelected()) {
        currentFile = fileDialog.GetSelected().string();
        fileDialog.ClearSelected();
        LoadConfigFile(currentFile);
    }
    if (ImGui::Button("Load")) {
        LoadConfigFile(currentFile);
    }
    ImGui::SameLine(0.0f, 10);
    if (ImGui::Button("Edit Config")) {
        showConfigEditBoxPopup = true;
    }

    
    for (ConfigAccount& account : config_accounts) {
        bool wasActive = account.active;
        ImGui::Checkbox(("Account #" + std::to_string(account.accountNumber)).c_str(), &account.active);
        if (account.active != wasActive) {
            SaveConfigFile(currentFile);
            LoadConfigFile(currentFile);
        }
        if (account.active) {
            ImGui::SameLine(0.0f, 10);
            std::string buttonLabel = "START # " + std::to_string(account.accountNumber) + " #";
            if (ImGui::Button(buttonLabel.c_str())) {
                wow_start::LaunchAccount(account.accountNumber, account.email, "-config " + account.email + ".wtf", account.relog);
            }
        
            // Handle changes in the "Relog" checkbox state
            bool wasRelog = account.relog;
            ImGui::SameLine(0.0f, 20); // Add a little space between the button and the checkbox
            ImGui::Checkbox("Relog", &account.relog); // Render the "Relog" checkbox
            if (account.relog != wasRelog) {
                // If the "relog" state changed, save and reload the config
                SaveConfigFile(currentFile);
                LoadConfigFile(currentFile);
            }
            wowstart_commands.push_back({ account, "-config " + account.email + ".wtf" });
        }
    }
    if (!wowstart_commands.empty()) {
        if (ImGui::Button("Start All")) {
            auto& pidDataManager = PIDDataManager::getInstance();
            auto activePids = pidDataManager.getAllActivePIDData();
            for (const auto& command : wowstart_commands) {
                bool isAlreadyRunning = std::any_of(activePids.begin(), activePids.end(), [&](const auto& pidDataPair) {
                    auto activeEmail = pidDataManager.getAccountEmail(pidDataPair.first);
                    return activeEmail == command.first.email;
                    });
                if (!isAlreadyRunning) {
                    wow_start::LaunchAccount(command.first.accountNumber, command.first.email, command.second, command.first.relog);
                }
            }
        }
    }
    if (showConfigEditBoxPopup) {
        if (ImGui::Begin("Config Editor", &showConfigEditBoxPopup)) {
            DrawEditorBox();
            ImGui::SameLine(0.0f, 10);
            if (ImGui::Button("Close")) {
                showConfigEditBoxPopup = false;
            }
            ImGui::End();
        }
    }
}
