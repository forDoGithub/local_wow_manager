#include "framework.h"
#include "imgui.h"
#include "imfilebrowser.h"

import pid_data_manager;
import data_helpers;

export module DrawLaunchingPids;

export void DrawLaunchingPids() {
    // Launching PIDs
    if (ImGui::TreeNode("LAUNCHING PIDS")) {
        ImGui::BeginChild("LaunchingPIDsChild", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10), true); // Create a child window with a height of 10 lines
        const auto& allLaunchingPIDData = PIDDataManager::getInstance().getAllLaunchingPIDData();
        if (!allLaunchingPIDData.empty()) {
            int columns_count = DetermineColumnCount(allLaunchingPIDData);
            ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_HighlightHoveredColumn;

            if (ImGui::BeginTable("launching_pid_data_table", columns_count, table_flags)) {
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
                SetupDynamicColumns(allLaunchingPIDData);
                ImGui::TableHeadersRow();

                for (const auto& pidDataEntry : allLaunchingPIDData) {
                    const PIDData& data = pidDataEntry.second;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable((std::string("PID: ") + std::to_string(pidDataEntry.first)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (ImGui::IsItemClicked()) {
                            ImGui::SetNextItemOpen(true);
                        }
                    }

                    if (ImGui::TreeNode((std::string("Account: ") + std::to_string(data.accountNumber)).c_str())) {
                        if (ImGui::TreeNode("Sent Messages")) {
                            for (const auto& message : data.sentMessages) {
                                ImGui::Selectable(message.c_str(), false);
                            }
                            ImGui::TreePop();
                        }

                        if (ImGui::TreeNode("Received Messages")) {
                            for (const auto& message : data.receivedMessages) {
                                ImGui::Selectable(message.c_str(), false);
                            }
                            ImGui::TreePop();
                        }

                        ImGui::TreePop();
                    }

                    int columnIndex = 1;
                    for (const auto& kv : data.data) {
                        if (ImGui::TableSetColumnIndex(columnIndex)) {
                            DisplayData(kv.first, kv.second);
                        }
                        columnIndex++;
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild(); // End of child window
        ImGui::TreePop();
    }
}