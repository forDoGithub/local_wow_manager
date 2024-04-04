#include "framework.h"

import pid_data_manager;
import wow_start;
import time_helper;
export module server_log_frame;

export class ServerLogFrame {

public:

    int total_messages_ = 0;
    int total_received_ = 0;
    int total_sent_ = 0;
    std::string first_time_ = "N/A";
    std::string time_since_first_ = "N/A";
    std::string last_time_ = "N/A";
    std::string time_since_last_ = "N/A";

    void AddLog(const std::string& message, const std::optional<std::string>& prefix = std::nullopt) {
        std::ostringstream oss;
        oss << "[" << time_helper::readable() << "] ";
        if (prefix) {
            oss << *prefix << " ";
        }
        oss << message;

        std::lock_guard<std::mutex> lock(mutex_);
        logs_.push_back(oss.str());
    }
    void Draw(const char* title, bool* p_open = nullptr) {
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open)) {
            ImGui::End();
            return;
        }

        // Options menu
        if (ImGui::BeginPopup("Options")) {
            ImGui::Checkbox("Auto-scroll", &auto_scroll_);
            ImGui::EndPopup();
        }

        // Main window
        if (ImGui::Button("Options")) {
            ImGui::OpenPopup("Options");
        }
        ImGui::SameLine();
        bool clear = ImGui::Button("Clear");
        ImGui::SameLine();
        bool copy = ImGui::Button("Copy");

        ImGui::Separator();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (clear) {
            Clear();
        }
        if (copy) {
            CopyToClipboard();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.0f));
        for (const auto& item : logs_) {
            if (filter_.PassFilter(item.c_str())) {
                ImGui::Selectable(item.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowItemOverlap);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    ImGui::SetClipboardText(item.c_str());
                }
            }
        }

        
        ImGui::End();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.clear();
    }

    void CopyToClipboard() const {
        std::ostringstream oss;
        for (const auto& item : logs_) {
            oss << item << "\n";
        }
        ImGui::SetClipboardText(oss.str().c_str());
    }

private:
    std::vector<std::string> logs_;
    mutable std::mutex mutex_;
    ImGuiTextFilter filter_;
    bool auto_scroll_ = true;
};
