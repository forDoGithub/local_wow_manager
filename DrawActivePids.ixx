#include "framework.h"
#include "imgui.h"
#include "imfilebrowser.h"
#include <variant>
import pid_data_manager;
import data_helpers;


export module DrawActivePids;
export void DrawHealthBar(int health, int health_total) {
    float healthPercentage = static_cast<float>(health) / health_total;
    ImVec4 healthColor = ImVec4(1.0f - healthPercentage, healthPercentage, 0.0f, 1.0f);
    if (healthPercentage > 0.65) {
        // Blend from green to yellow
        healthColor.x = 2.0f * (1.0f - healthPercentage); // Increase red as health decreases
        healthColor.y = 1.0f; // Green is always full until halfway
    }
    else if (healthPercentage > 0.20) {
        // Blend from yellow to red
        healthColor.x = 1.0f; // Red is always full until 20%
        healthColor.y = 2.0f * healthPercentage; // Decrease green as health decreases further
    }
    else {
        // Health is below 20%, so make it red
        healthColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    ImGui::Text("Player HP:"); // Add title above the health bar
    ImGui::SameLine(); // Keep the title and the health bar on the same line

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, healthColor);
    // Display health, total health, and health percentage in the progress bar
    std::string healthText = std::to_string(health) + " / " + std::to_string(health_total) + " (" + std::to_string(static_cast<int>(healthPercentage * 100)) + "%)";

    // Calculate padding for centering text
    const float padding = (1.0f - ImGui::CalcTextSize(healthText.c_str()).x / ImGui::GetWindowWidth()) / 2.0f;
    std::string paddedHealthText = healthText + std::string(padding, ' ');

    ImGui::ProgressBar(healthPercentage, ImVec2(-FLT_MIN, 0), paddedHealthText.c_str());
    ImGui::PopStyleColor();
}
export void DrawActivePIDs() {
    if (ImGui::TreeNodeEx("ACTIVE PIDS", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("ActivePIDsChild", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10), true);
        const auto& allActivePIDData = PIDDataManager::getInstance().getAllActivePIDData();
        if (!allActivePIDData.empty()) {
            int columns_count = DetermineColumnCount(allActivePIDData);
            ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_HighlightHoveredColumn;

            if (ImGui::BeginTable("active_pid_data_table", columns_count, table_flags)) {
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
                SetupDynamicColumns(allActivePIDData);
                ImGui::TableHeadersRow();

                for (const auto& [pid, dataPtr] : allActivePIDData) {
                    const PIDData& data = *dataPtr;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable((std::string("PID: ") + std::to_string(data.pid)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (ImGui::IsItemClicked()) {
                            ImGui::SetNextItemOpen(true);
                        }
                    }

                    if (ImGui::TreeNode((std::string("Account: ") + std::to_string(data.accountNumber) + " - " + data.accountEmail).c_str())) {
                        ImGui::TextColored(data.c_init ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), "%s", data.c_init ? "|||" : "|||");

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

                    std::string windowName = "Account: " + std::to_string(data.accountNumber) + " - " + data.accountEmail;

                    if (ImGui::Begin(windowName.c_str())) {
                        if (!data.initialized) {
                            static int counter = 0;
                            static float elapsed = 0.0f;
                            elapsed += ImGui::GetIO().DeltaTime;
                            if (elapsed >= 0.5f) {
                                counter = (counter + 1) % 3;
                                elapsed = 0.0f;
                            }
                            std::string loadingText = "Waiting for connection" + std::string(counter + 1, '.');
                            ImGui::Text("%s", loadingText.c_str());
                        }
                        else {
                            ImGui::Text("Connected to server");
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(0, 1, 0, 1), "|||");

                            for (const auto& kv : data.data) {
                                if (kv.first == "last_rawJson") {
                                    ImGui::Text("RAW JSON: ");
                                }
                                else {
                                    DisplayData(kv.first, kv.second);
                                }
                            }

                            if (data.data.count("health") > 0 && data.data.count("health_total") > 0) {
                                if (std::holds_alternative<int>(data.data.at("health")) && std::holds_alternative<std::string>(data.data.at("health_total"))) {
                                    int health = std::get<int>(data.data.at("health"));
                                    std::string health_total_str = std::get<std::string>(data.data.at("health_total"));

                                    int health_total;
                                    try {
                                        health_total = std::stoi(health_total_str);
                                        DrawHealthBar(health, health_total);
                                    }
                                    catch (std::exception& e) {
                                        std::cout << "Error converting health_total: " << e.what() << std::endl;
                                    }
                                }
                            }
                        }
                        ImGui::End();
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::TreePop();
    }

}