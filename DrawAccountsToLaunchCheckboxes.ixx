#include "framework.h"
#include "imgui.h"
#include "imfilebrowser.h"
import ConfigManager;

export bool account_checkboxes_[5] = { false, false, false, false, false };

export module DrawAccountsToLaunchCheckboxes;


export void DrawOptionsMenu(ConfigManager& configManager) {
    if (ImGui::BeginPopup("Options")) {
        const auto& accounts = configManager.GetAccounts();
        for (const auto& account : accounts) {
            bool active = account.active;
            if (ImGui::Checkbox(("Account " + std::to_string(account.accountNumber) + ": " + account.characterName).c_str(), &active)) {
                configManager.SetAccountCheckbox(account.accountNumber, active);
            }

            ImGui::SameLine();
            bool relog = account.relog;
            if (ImGui::Checkbox(("Relog##" + std::to_string(account.accountNumber)).c_str(), &relog)) {
                configManager.SetAccountRelog(account.accountNumber, relog);
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::Button("Options")) {
        ImGui::OpenPopup("Options");
    }
}