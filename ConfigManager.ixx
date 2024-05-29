#include <fstream>
#include <string>
#include <sstream>
#include <vector>
export module ConfigManager;

export class ConfigManager {
public:
    ConfigManager(const std::string& filename) : filename_(filename) {}

    bool GetAccountCheckbox(int index) {
        std::ifstream config_file(filename_);
        std::string line;
        for (int i = 0; i < 5; ++i) {
            if (std::getline(config_file, line)) {
                std::istringstream iss(line);
                std::string key, value;
                if (std::getline(iss, key, '=') && std::getline(iss, value)) {
                    if (i == index) {
                        return (value == "true");
                    }
                }
            }
        }
        return false; // Default value if not found
    }

    void SetAccountCheckbox(int index, bool value) {
        std::vector<std::string> lines;
        std::string line;
        std::ifstream config_file_read(filename_);
        while (std::getline(config_file_read, line)) {
            lines.push_back(line);
        }
        config_file_read.close();

        // Update the specific line
        if (index < lines.size()) {
            std::ostringstream oss;
            oss << "Account" << (index + 1) << "=" << (value ? "true" : "false");
            lines[index] = oss.str();
        }

        // Write everything back
        std::ofstream config_file_write(filename_, std::ios::out | std::ios::trunc); // Overwrite mode
        for (const auto& each_line : lines) {
            config_file_write << each_line << "\n";
        }
    }

private:
    std::string filename_;
};