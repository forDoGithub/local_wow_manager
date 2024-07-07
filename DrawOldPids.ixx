#include "framework.h"
#include "imgui.h"
#include "imfilebrowser.h"

import pid_data_manager;
import data_helpers;

export module DrawOldPids;

export void DrawOldPIDs() {
    // Old PIDs
    if (ImGui::TreeNode("OLD PIDS")) {
        ImGui::BeginChild("OldPIDsChild", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10), true);
        const auto& allOldPIDData = PIDDataManager::getInstance().getAllOldPIDData();
        if (!allOldPIDData.empty()) {
            int columns_count = DetermineColumnCount(allOldPIDData);
            ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_HighlightHoveredColumn;
            if (ImGui::BeginTable("old_pid_data_table", columns_count, table_flags)) {
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
                SetupDynamicColumns(allOldPIDData);
                ImGui::TableHeadersRow();
                for (const auto& [pid, dataPtr] : allOldPIDData) {
                    const PIDData& data = *dataPtr;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable((std::string("PID: ") + std::to_string(pid)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (ImGui::IsItemClicked()) {
                            ImGui::SetNextItemOpen(true);
                        }
                    }
                    if (ImGui::TreeNode((std::string("Account: ") + std::to_string(data.accountNumber) + " - " + data.accountEmail).c_str())) {
                        // Display final connection status
                        ImGui::TextColored(data.c_init ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), "Final Connection Status: %s", data.c_init ? "Connected" : "Disconnected");

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
        ImGui::EndChild();
        ImGui::TreePop();
    }

}