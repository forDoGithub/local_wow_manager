#include <fstream>
#include <string>
#include <vector>
#include <algorithm> // Include this for std::find_if
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
    bool autologin;
    std::string serverName;
    std::string scriptPath;
    bool initWorldScript;
    bool autoload;
};

export class ConfigManager {
public:
    ConfigManager(const std::string& filename) : filename_(filename) {
        ReadConfig();
    }
    void SetAccountScript(int accountNumber, const std::string& scriptPath) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            it->scriptPath = scriptPath;
            WriteConfig();
        }
    }
    std::string GetAccountScriptText(int accountNumber) const {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            // Instead of returning JSON, we'll return the actual script content
            // You may need to implement file reading logic here if scripts are stored in separate files
            return it->scriptPath;  // For now, we're just returning the path as a placeholder
        }
        return "";  // Return empty string if account not found
    }
    void SetAccountScriptText(int accountNumber, const std::string& scriptText) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            // Instead of parsing JSON, we're directly setting the script content
            // You may need to implement file writing logic here if scripts are stored in separate files
            it->scriptPath = scriptText;  // For now, we're just storing the text in scriptPath as a placeholder
            WriteConfig();
        }
    }
    std::string GetAccountScript(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        return it != accounts_.end() ? it->scriptPath : "";
    }

    void SetAccountInitWorldScript(int accountNumber, bool value) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            it->initWorldScript = value;
            WriteConfig();
        }
    }

    bool GetAccountInitWorldScript(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        return it != accounts_.end() ? it->initWorldScript : false;
    }

    void SetAccountAutoload(int accountNumber, bool value) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            it->autoload = value;
            WriteConfig();
        }
    }

    bool GetAccountAutoload(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        return it != accounts_.end() ? it->autoload : false;
    }

    void ReadConfig() {
        std::ifstream config_file(filename_);
        json j;
        config_file >> j;
        configJson = j;

        accounts_.clear();
        for (const auto& acc : j["accounts"]) {
            AccountConfig config{
                acc.value("accountNumber", 0),
                acc.value("active", false),
                acc.value("characterName", ""),
                acc.value("email", ""),
                acc.value("relog", false),
                acc.value("autologin", false),
                acc.value("serverName", ""),
                acc.value("scriptPath", ""),
                acc.value("initWorldScript", false),
                acc.value("autoload", false)
            };
            accounts_.push_back(config);
            UpdatePIDData(config.accountNumber, config.relog);
        }
    }

    std::string GetConfigText() const {
        nlohmann::json j;
        j["accounts"] = nlohmann::json::array();
        for (const auto& acc : accounts_) {
            j["accounts"].push_back({
                {"accountNumber", acc.accountNumber},
                {"active", acc.active},
                {"characterName", acc.characterName},
                {"email", acc.email},
                {"relog", acc.relog},
                {"autologin", acc.autologin},
                {"serverName", acc.serverName},
                {"scriptPath", acc.scriptPath},
                {"initWorldScript", acc.initWorldScript},
                {"autoload", acc.autoload}
                });
        }
        return j.dump(4);  // Use 4 spaces for indentation
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
                    acc.value("autologin", false),
                    acc.value("serverName", ""),
                    acc.value("scriptPath", ""),
                    acc.value("initWorldScript", false),
                    acc.value("autoload", false)
                    });
            }

            configJson = std::move(newJson);
            accounts_ = std::move(newAccounts);

            WriteConfig();
            UpdatePIDDataManager();
        }
        catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error in SetConfigFromText: " << e.what() << std::endl;
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
            UpdatePIDData(accountNumber, it->relog);
            WriteConfig();
        }
    }

    bool GetAccountCheckbox(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        return it != accounts_.end() ? it->active : false;
    }

    bool GetAccountRelog(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        return it != accounts_.end() ? it->relog : false;
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

    void SetAccountAutoLogin(int accountNumber, bool value) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        if (it != accounts_.end()) {
            it->autologin = value;
            WriteConfig();
        }
    }

    bool GetAccountAutoLogin(int accountNumber) {
        auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const AccountConfig& acc) { return acc.accountNumber == accountNumber; });
        return it != accounts_.end() ? it->autologin : false;
    }



 

    std::string GetFilename() const {
        return filename_;
    }

    void LoadConfig(const std::string& filename) {
        filename_ = filename;
        ReadConfig();
    }

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
                {"autologin", acc.autologin}, // Write autologin
                {"serverName", acc.serverName},
                {"scriptPath", acc.scriptPath}  // Write script path
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
        }
    }

    DWORD GetPIDForAccountNumber(int accountNumber) {
        return 1000 + accountNumber; // Implement your logic to map account number to PID
    }
};