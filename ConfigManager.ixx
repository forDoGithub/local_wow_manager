#include <fstream>
#include <string>
#include <sstream>

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
        // This is a simplified example. In a real application, you would likely want to read all the values into memory, update the one you're interested in, and then write all the values back out.
        std::ofstream config_file(filename_, std::ios_base::app); // Append mode
        config_file << "Account" << (index + 1) << "=" << (value ? "true" : "false") << "\n";
    }

private:
    std::string filename_;
};