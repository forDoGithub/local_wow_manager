#include <fstream>
#include <string>
#include <vector>
#include "framework.h"
#include "json_wrapper.h"

import pid_data_manager;

export module ConfigManager;


using json = json_wrapper::json;

struct AccountConfig {
    int accountNumber;
    bool active;
    std::string characterName;
    std::string email;
    bool relog;
    std::string serverName;
};

export class ConfigManager {
public:
    void ReadConfig() {
        std::ifstream config_file(filename_);
        json j;
        config_file >> j;
        configJson = j;  // Store the entire JSON

        accounts_.clear();
        for (const auto& acc : j["accounts"]) {
            AccountConfig config{
                acc.value("accountNumber", 0),
                acc.value("active", false),
                acc.value("characterName", ""),
                acc.value("email", ""),
                acc.value("relog", false),
                acc.value("serverName", "")
            };
            accounts_.push_back(config);
            UpdatePIDData(config.accountNumber, config.relog);
        }
    }

    ConfigManager(const std::string& filename) : filename_(filename) {
        ReadConfig();
    }
    std::string GetConfigText() const {
        return configJson.dump(4);
    }

    void SetConfigFromText(const std::string& text) {
        try {
            auto newJson = nlohmann::json::parse(text);

            if (!newJson.contains("accounts") || !newJson["accounts"].is_array()) {
                throw std::runtime_error("Invalid config format: 'accounts' array not found");
            }

            std::vector<AccountConfig> newAccounts;
            newAccounts.reserve(newJson["accounts"].size());

            for (const auto& acc : newJson["accounts"]) {
                newAccounts.emplace_back(AccountConfig{
                    acc.value("accountNumber", 0),
                    acc.value("active", false),
                    acc.value("characterName", ""),
                    acc.value("email", ""),
                    acc.value("relog", false),
                    acc.value("serverName", "")
                    });
            }

            // If everything is parsed successfully, update the member variables
            configJson = std::move(newJson);
            accounts_ = std::move(newAccounts);

            WriteConfig();
            UpdatePIDDataManager();
        }
        catch (const nlohmann::json::parse_error& e) {
            // Handle JSON parsing error
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            // You might want to throw a custom exception or handle this error in a way that fits your application
        }
        catch (const std::exception& e) {
            // Handle other exceptions
            std::cerr << "Error in SetConfigFromText: " << e.what() << std::endl;
            // Handle or rethrow as appropriate
        }
    }

    const std::vector<AccountConfig>& GetAccounts() const {
        return accounts_;
    }
    void SetAccountCheckbox(int accountNumber, bool value) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            it->active = value;
            UpdatePIDData(accountNumber, it->relog);  // We still update with relog status
            WriteConfig();
        }
    }

    bool GetAccountCheckbox(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            return it->active;
        }
        return false; // Default value if not found
    }
    bool GetAccountRelog(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            return it->relog;
        }
        return false; // Default value if not found
    }

    void SetAccountRelog(int accountNumber, bool value) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            it->relog = value;
            UpdatePIDData(accountNumber, value);
            WriteConfig();
        }
    }


    
    std::string GetFilename() const {
        return filename_;
    }

    void LoadConfig(const std::string & filename) {
        filename_ = filename;
        ReadConfig();
    }

    // Modified ReadConfig to be public
        
  

private:
    std::string filename_;
    std::vector<AccountConfig> accounts_;
    json configJson;

    void WriteConfig() {
        configJson["accounts"] = json::array();
        for (const auto& acc : accounts_) {
            configJson["accounts"].push_back({
                {"accountNumber", acc.accountNumber},
                {"active", acc.active},
                {"characterName", acc.characterName},
                {"email", acc.email},
                {"relog", acc.relog},
                {"serverName", acc.serverName}
                });
        }

        std::ofstream config_file(filename_, std::ios::out | std::ios::trunc);
        config_file << std::setw(4) << configJson << std::endl;
    }

    void UpdatePIDDataManager() {
        PIDDataManager::getInstance().updatePIDDataFromConfig(accounts_);
    }
    void UpdatePIDData(int accountNumber, bool relogValue) {
        DWORD pid = GetPIDForAccountNumber(accountNumber);
        if (pid != 0) {
            PIDDataManager::getInstance().setRelogStatus(pid, relogValue);
            // Update other PID data as needed
        }
    }
    DWORD GetPIDForAccountNumber(int accountNumber) {
        // This is a placeholder function
        // Implement your logic to map account number to PID here
        // For example, you might have a separate mapping file or data structure
        // that associates account numbers with PIDs
        
        // For now, we'll just return a dummy PID
        return 1000 + accountNumber;
    }
};