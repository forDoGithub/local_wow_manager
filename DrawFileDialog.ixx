#define NOMINMAX
#include "imgui.h"
#include "imfilebrowser.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

import wow_start;
import pid_data_manager;
import ConfigManager;
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
    if (!file.is_open()) {
        std::cerr << "Could not open config file: " << filename << std::endl;
        return;
    }

    std::string str((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    configJson = nlohmann::json::parse(str);

    // Split lines for editor
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
    if (file.is_open()) {
        file << newJson.dump(4);
    }
    else {
        std::cerr << "Could not save config file: " << filename << std::endl;
    }
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

        SaveConfigFile(currentFile);  // Use the SaveConfigFile function
        LoadConfigFile(currentFile);
    }
}

std::vector<std::pair<ConfigAccount, std::string>> wowstart_commands;

export void DrawConfigManager(ConfigManager& configManager) {
    ImGui::InputText("Config File", &currentFile[0], currentFile.size(), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse##ConfigFile")) {
        fileDialog.Open();
    }

    fileDialog.Display();
    if (fileDialog.HasSelected()) {
        currentFile = fileDialog.GetSelected().string();
        fileDialog.ClearSelected();
        configManager.LoadConfig(currentFile);
    }

    if (ImGui::Button("Reload")) {
        configManager.LoadConfig(currentFile);
    }

    ImGui::SameLine(0.0f, 10);
    if (ImGui::Button("Edit Config")) {
        showConfigEditBoxPopup = true;
    }

    static ImGui::FileBrowser scriptDialog;
    static int currentScriptAccount = -1;

    const auto& accounts = configManager.GetAccounts();
    for (const auto& account : accounts) {
        ImGui::PushID(account.accountNumber);

        bool active = account.active;
        if (ImGui::Checkbox(("Account #" + std::to_string(account.accountNumber) + ": " + account.characterName).c_str(), &active)) {
            configManager.SetAccountCheckbox(account.accountNumber, active);
        }

        if (active) {
            ImGui::SameLine(0.0f, 10);
            std::string buttonLabel = "START #" + std::to_string(account.accountNumber);
            if (ImGui::Button(buttonLabel.c_str())) {
                wow_start::LaunchAccount(account.accountNumber, account.email, "-config " + account.email + ".wtf", account.relog);
            }

            // Checkboxes
            ImGui::SameLine(0.0f, 20);
            bool relog = account.relog;
            if (ImGui::Checkbox(("Relog##" + std::to_string(account.accountNumber)).c_str(), &relog)) {
                configManager.SetAccountRelog(account.accountNumber, relog);
            }

            ImGui::SameLine(0.0f, 20);
            bool autologin = configManager.GetAccountAutoLogin(account.accountNumber);
            if (ImGui::Checkbox(("Auto Login##" + std::to_string(account.accountNumber)).c_str(), &autologin)) {
                configManager.SetAccountAutoLogin(account.accountNumber, autologin);
            }

            ImGui::SameLine(0.0f, 20);
            bool autoload = configManager.GetAccountAutoload(account.accountNumber);
            if (ImGui::Checkbox(("Autoload Script##" + std::to_string(account.accountNumber)).c_str(), &autoload)) {
                configManager.SetAccountAutoload(account.accountNumber, autoload);
            }

            // Indent for script controls
            ImGui::Indent(20);

            // Script path input, browse, load, and edit config buttons
            static char scriptPathBuffer[256];
            std::string scriptPath = configManager.GetAccountScript(account.accountNumber);

            // Safe string copy
#if defined(_MSC_VER)
            strncpy_s(scriptPathBuffer, sizeof(scriptPathBuffer), scriptPath.c_str(), _TRUNCATE);
#else
            std::size_t length = std::min(scriptPath.length(), sizeof(scriptPathBuffer) - 1);
            std::memcpy(scriptPathBuffer, scriptPath.c_str(), length);
            scriptPathBuffer[length] = '\0';
#endif

            // Calculate available width for input box
            float availableWidth = ImGui::GetContentRegionAvail().x;
            float buttonWidth = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2;
            float inputWidth = availableWidth - buttonWidth * 3 - ImGui::GetStyle().ItemSpacing.x * 3;

            ImGui::PushItemWidth(inputWidth);
            if (ImGui::InputText("##ScriptPath", scriptPathBuffer, sizeof(scriptPathBuffer))) {
                configManager.SetAccountScript(account.accountNumber, scriptPathBuffer);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Browse")) {
                currentScriptAccount = account.accountNumber;
                scriptDialog.SetTitle("Select Script for Account " + std::to_string(account.accountNumber));
                scriptDialog.SetTypeFilters({ ".lua", ".txt" });
                scriptDialog.Open();
            }

            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                DWORD pid = PIDDataManager::getInstance().getPIDForAccountNumber(account.accountNumber);
                if (pid != 0) {
                    std::string loadCommand = "LOAD_SCRIPT " + configManager.GetAccountScript(account.accountNumber);
                    // TODO: Uncomment when pipes::sendCommand is implemented
                    // pipes::sendCommand(pid, loadCommand);
                }
                else {
                    std::cerr << "Error: No active process found for account " << account.accountNumber << std::endl;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Edit Script")) {
                ImGui::OpenPopup(("Edit Script##" + std::to_string(account.accountNumber)).c_str());
            }

            // Edit Script Popup
            if (ImGui::BeginPopupModal(("Edit Script##" + std::to_string(account.accountNumber)).c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                static std::string scriptText;
                static bool initialized = false;
                static char buffer[102400] = ""; // Buffer size, adjust as needed

                if (!initialized) {
                    scriptText = configManager.GetAccountScriptText(account.accountNumber);
                    strncpy_s(buffer, scriptText.c_str(), sizeof(buffer) - 1);
                    initialized = true;
                }

                ImGui::InputTextMultiline("##script", buffer, sizeof(buffer), ImVec2(400, 300));

                if (ImGui::Button("Save")) {
                    scriptText = buffer;
                    configManager.SetAccountScriptText(account.accountNumber, scriptText);
                    ImGui::CloseCurrentPopup();
                    initialized = false;
                }

                ImGui::SameLine();
                if (ImGui::Button("Close")) {
                    ImGui::CloseCurrentPopup();
                    initialized = false;
                }

                ImGui::EndPopup();
            }

            // Unindent after script controls
            ImGui::Unindent(20);
        }

        ImGui::PopID();
    }

    

    // Display the script file dialog
    scriptDialog.Display();

    if (scriptDialog.HasSelected()) {
        if (currentScriptAccount != -1) {
            std::string scriptPath = scriptDialog.GetSelected().string();
            configManager.SetAccountScript(currentScriptAccount, scriptPath);
            currentScriptAccount = -1;
        }
        scriptDialog.ClearSelected();
    }

    if (ImGui::Button("Start All Active")) {
        auto& pidDataManager = PIDDataManager::getInstance();
        const auto& activePids = pidDataManager.getAllActivePIDData();

        for (const auto& account : accounts) {
            if (account.active) {
                bool isAlreadyRunning = std::any_of(activePids.begin(), activePids.end(),
                    [&](const auto& pidDataPair) {
                        return pidDataManager.getAccountEmail(pidDataPair.first) == account.email;
                    });

                if (!isAlreadyRunning) {
                    wow_start::LaunchAccount(account.accountNumber, account.email, "-config " + account.email + ".wtf", account.relog);
                }
            }
        }
    }

    if (showConfigEditBoxPopup) {
        ImGui::OpenPopup("Config Editor");
    }

    if (ImGui::BeginPopupModal("Config Editor", &showConfigEditBoxPopup)) {
        static nlohmann::json configJson;
        static std::string configText;
        static bool initialized = false;
        static char buffer[102400] = ""; // Buffer size, adjust as needed
        static std::string errorMessage;

        if (!initialized) {
            configText = configManager.GetConfigText();
            try {
                configJson = nlohmann::json::parse(configText);
                configText = configJson.dump(4); // Pretty print with 4 space indent
                strncpy_s(buffer, configText.c_str(), sizeof(buffer) - 1);
                errorMessage.clear();
            }
            catch (const nlohmann::json::parse_error& e) {
                errorMessage = "JSON Parse Error: " + std::string(e.what());
            }
            initialized = true;
        }

        if (!errorMessage.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), errorMessage.c_str());
        }

        if (ImGui::InputTextMultiline("##config", buffer, sizeof(buffer), ImVec2(-1, -1))) {
            configText = buffer;
            try {
                configJson = nlohmann::json::parse(configText);
                errorMessage.clear();
            }
            catch (const nlohmann::json::parse_error& e) {
                errorMessage = "JSON Parse Error: " + std::string(e.what());
            }
        }

        if (ImGui::Button("Save")) {
            if (errorMessage.empty()) {
                configManager.SetConfigFromText(configJson.dump(4));
                ImGui::CloseCurrentPopup();
                initialized = false; // Reset for next time
            }
            else {
                ImGui::OpenPopup("Error");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            initialized = false; // Reset for next time
        }

        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Cannot save invalid JSON. Please correct the errors before saving.");
            if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        ImGui::EndPopup();
    }
}