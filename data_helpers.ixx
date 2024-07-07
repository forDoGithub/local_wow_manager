#include "framework.h"
#include <algorithm>  // Add this line for std::max
#include <set>        // Add this line for std::set
#include <chrono>     // Add this line for std::chrono
#include "imgui.h"    // Add this line for ImGui functions

import pid_data_manager;

export module data_helpers;

export int DetermineColumnCount(const std::unordered_map<int, std::unique_ptr<PIDData>>& data) {
    int maxColumns = 1;  // Start with 1 for the PID column
    for (const auto& [pid, dataPtr] : data) {
        int columnCount = static_cast<int>(dataPtr->data.size()) + 1;
        if (columnCount > maxColumns) {
            maxColumns = columnCount;
        }
    }
    return maxColumns;
}

export void SetupDynamicColumns(const std::unordered_map<int, std::unique_ptr<PIDData>>& data) {
    std::set<std::string> columnNames;
    for (const auto& [pid, dataPtr] : data) {
        for (const auto& [key, value] : dataPtr->data) {
            columnNames.insert(key);
        }
    }
    for (const auto& columnName : columnNames) {
        ImGui::TableSetupColumn(columnName.c_str());
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
        }, value);
}