#include "framework.h"
#include "imgui.h"
#include "imfilebrowser.h"

export bool account_checkboxes_[5] = { false, false, false, false, false };

export module DrawAccountsToLaunchCheckboxes;

export void DrawOptionsMenu() {
    // Options menu
    if (ImGui::BeginPopup("Options")) {
        for (int i = 0; i < 5; ++i) {
            ImGui::Checkbox(("Account " + std::to_string(i + 1)).c_str(), &account_checkboxes_[i]);
        }
        ImGui::EndPopup();
    }

    if (ImGui::Button("Options")) {
        ImGui::OpenPopup("Options");
    }
}