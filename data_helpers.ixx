#include "framework.h";

import pid_data_manager;

export module data_helpers;

export int DetermineColumnCount(const std::unordered_map<int, PIDData>& allPIDData) {
    int maxColumns = 1; // Start with 1 for the PID column

    for (const auto& pidDataEntry : allPIDData) {
        const PIDData& data = pidDataEntry.second;

        int currentColumns = data.data.size(); // Number of keys in the current PID's data map
        if (currentColumns > maxColumns - 1) { // Subtract 1 as we already counted the PID column
            maxColumns = currentColumns + 1; // Update maxColumns if current PID has more data fields
        }
    }

    return maxColumns;
}

export void SetupDynamicColumns(const std::unordered_map<int, PIDData>& allPIDData) {
    // Determine unique keys across all PIDData entries
    std::set<std::string> uniqueKeys;
    for (const auto& pidDataEntry : allPIDData) {
        for (const auto& kv : pidDataEntry.second.data) {
            uniqueKeys.insert(kv.first);
        }
    }

    // Setup columns for each unique key
    for (const auto& key : uniqueKeys) {
        ImGui::TableSetupColumn(key.c_str(), ImGuiTableColumnFlags_AngledHeader | ImGuiTableColumnFlags_WidthFixed);
    }
}

export void DisplayData(const std::string& key, const std::variant<int, std::string, bool, std::intptr_t, HWND, std::vector<std::string>, std::chrono::system_clock::time_point>& value) {
    std::visit([&key](const auto& arg) {
        using ArgType = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<ArgType, int>) {
            ImGui::Text("%s: %d", key.c_str(), arg);
        }
        else if constexpr (std::is_same_v<ArgType, std::string>) {
            ImGui::Text("%s: %s", key.c_str(), arg.c_str());
        }
        else if constexpr (std::is_same_v<ArgType, bool>) {
            ImGui::Text("%s: %s", key.c_str(), arg ? "True" : "False");
        }
        else if constexpr (std::is_same_v<ArgType, HWND>) {
            ImGui::Text("%s: %p", key.c_str(), static_cast<void*>(arg));
        }
        else if constexpr (std::is_same_v<ArgType, std::intptr_t>) {
            ImGui::Text("%s: %p", key.c_str(), reinterpret_cast<void*>(arg));
        }
        else if constexpr (std::is_same_v<ArgType, std::chrono::system_clock::time_point>) {
            auto time_t = std::chrono::system_clock::to_time_t(arg);
            char buffer[26];
            ctime_s(buffer, sizeof(buffer), &time_t);
            ImGui::Text("%s: %s", key.c_str(), buffer);
        }
        else if constexpr (std::is_same_v<ArgType, std::vector<std::string>>) {
            if (ImGui::TreeNode(key.c_str())) {
                for (const auto& item : arg) {
                    ImGui::BulletText("%s", item.c_str());
                }
                ImGui::TreePop();
            }
        }
        // Ensure all types in the variant are covered
        }, value);
}