#include "framework.h"
#include "json_wrapper.h"
export module pid_data_manager;

export struct AccountConfig {
    int accountNumber;
    bool active;
    std::string characterName;
    std::string email;
    bool relog;
    std::string serverName;
};
export enum class AccountState {
    NotLaunched,
    Launching,
    Initializing,
    Injecting,
    Running,
    Failed,
    Closed
};
export struct PIDData {
    AccountState state;
    bool clientConnected = false;
    bool relog = false;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    int accountNumber;
    std::unordered_map<std::string, std::variant<int, std::string, bool, std::intptr_t, HWND, std::vector<std::string>, std::chrono::system_clock::time_point>> data;
    std::vector<std::string> sentMessages;
    std::vector<std::string> receivedMessages;
    HWND hwnd;
    std::chrono::system_clock::time_point timestamp;
    std::string accountEmail;
    bool imguiWindowOpen;
    int totalMessagesSent = 0;
    int totalMessagesReceived = 0;
    std::vector<std::string> messageRecord;
    bool isNewPID = false;
    bool initialized = false;
    int hp = 0;
    bool c_init = false;
    DWORD pid = 0;
    bool running = false;
    bool changeProtectionDone = false;
    std::unique_ptr<std::mutex> mtx;
    std::unique_ptr<std::condition_variable> cvInit;
    std::unique_ptr<std::condition_variable> cvMonitor;

    PIDData() :
        imguiWindowOpen(false),
        mtx(std::make_unique<std::mutex>()),
        cvInit(std::make_unique<std::condition_variable>()),
        cvMonitor(std::make_unique<std::condition_variable>()),
        state(AccountState::NotLaunched) {}


    // Delete copy constructor and assignment operator
    PIDData(const PIDData&) = delete;
    PIDData& operator=(const PIDData&) = delete;

    // Add move constructor and assignment operator
    PIDData(PIDData&&) = default;
    PIDData& operator=(PIDData&&) = default;

    void addSentMessage(const std::string& message) {
        sentMessages.push_back(message);
    }

    void addReceivedMessage(const std::string& message) {
        receivedMessages.push_back(message);

        if (receivedMessages.size() == 1) {
            data["c_init"] = true;
        }
    }

    void addInt(const std::string& key, int value) {
        data[key] = value;
    }

    void addString(const std::string& key, const std::string& value) {
        data[key] = value;
    }
};

export class PIDDataManager {
public:
    bool isAccountInState(int accountNumber, AccountState state) {
        std::lock_guard<std::mutex> lock(mapMutex);
        for (const auto& [pid, data] : pidDataMap) {
            if (data->accountNumber == accountNumber && data->state == state) {
                return true;
            }
        }
        return false;
    }

    void updateAccountState(DWORD pid, AccountState newState) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            it->second->state = newState;
        }
    }


    static PIDDataManager& getInstance() {
        static PIDDataManager instance;
        return instance;
    }

    PIDData& getOrCreatePIDData(DWORD pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto [it, inserted] = pidDataMap.emplace(pid, std::make_unique<PIDData>());
        if (inserted) {
            it->second->pid = pid;
        }
        return *(it->second);
    }

    void updatePIDData(DWORD pid, std::unique_ptr<PIDData> data) {
        std::lock_guard<std::mutex> lock(mapMutex);
        data->pid = pid; // Ensure the PID is set correctly
        pidDataMap[pid] = std::move(data);
        std::cout << "Updated PID data for PID " << pid << ", Running: " << pidDataMap[pid]->running << std::endl;
    }

    bool isAccountLaunching(int accountNumber) {
        std::lock_guard<std::mutex> lock(mapMutex);
        for (const auto& [pid, data] : launchingPIDDataMap) {
            if (data->accountNumber == accountNumber) {
                return true;
            }
        }
        return false;
    }


    void addLaunchingPID(DWORD pid, int accountNumber) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto [it, inserted] = launchingPIDDataMap.emplace(pid, std::make_unique<PIDData>());
        it->second->isNewPID = true;
        it->second->accountNumber = accountNumber;
        it->second->startTime = std::chrono::system_clock::now();
    }

    void deactivatePID(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            oldPIDDataMap[pid] = std::move(it->second);
            pidDataMap.erase(it);
        }
    }


    void removeLaunchingPID(DWORD pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        launchingPIDDataMap.erase(pid);
    }

    const std::unordered_map<int, std::unique_ptr<PIDData>>& getAllActivePIDData() const {
        return pidDataMap;
    }

    const std::unordered_map<int, std::unique_ptr<PIDData>>& getAllOldPIDData() const {
        return oldPIDDataMap;
    }

    const std::unordered_map<int, std::unique_ptr<PIDData>>& getAllLaunchingPIDData() const {
        return launchingPIDDataMap;
    }

    std::string getAccountEmail(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            return it->second->accountEmail;
        }
        return "";
    }

    PIDData* getPIDData(DWORD pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    void removePIDData(DWORD pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            pidDataMap.erase(it);
        }
    }

    PIDData* getPIDDataForAccount(int accountNumber) {
        std::lock_guard<std::mutex> lock(mapMutex);
        for (const auto& [pid, pidData] : pidDataMap) {
            if (pidData->accountNumber == accountNumber) {
                return pidData.get();
            }
        }
        return nullptr;
    }

    void updatePIDDataFromConfig(const std::vector<AccountConfig>& accounts) {
        std::lock_guard<std::mutex> lock(mapMutex);
        for (const auto& account : accounts) {
            DWORD pid = getPIDForAccountNumber_unsafe(account.accountNumber);
            if (pid != 0) {
                auto& pidData = getOrCreatePIDData_unsafe(pid);
                pidData.relog = account.relog;
                pidData.accountNumber = account.accountNumber;
                pidData.accountEmail = account.email;
                pidData.running = account.active;
            }
        }
    }

    DWORD getPIDForAccountNumber(int accountNumber) {
        std::lock_guard<std::mutex> lock(mapMutex);
        for (const auto& [pid, pidData] : pidDataMap) {
            if (pidData->accountNumber == accountNumber) {
                return pid;
            }
        }
        return 0; // Return 0 if no matching PID is found
    }
    int getAccountNumberForPID(DWORD pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            return it->second->accountNumber;
        }
        return -1; // Return -1 if no matching account number is found
    }

    void setRelogStatus(DWORD pid, bool relogStatus) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            it->second->relog = relogStatus;
        }
    }

    bool getRelogStatus(DWORD pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            return it->second->relog;
        }
        return false;
    }


private:
    PIDDataManager() {}
    std::unordered_map<int, std::unique_ptr<PIDData>> pidDataMap;
    std::unordered_map<int, std::unique_ptr<PIDData>> oldPIDDataMap;
    std::unordered_map<int, std::unique_ptr<PIDData>> launchingPIDDataMap;
    mutable std::mutex mapMutex;

    PIDDataManager(const PIDDataManager&) = delete;
    PIDDataManager& operator=(const PIDDataManager&) = delete;

    DWORD getPIDForAccountNumber_unsafe(int accountNumber) {
        // No lock here, assumed to be called from a method that already holds the lock
        for (const auto& [pid, pidData] : pidDataMap) {
            if (pidData->accountNumber == accountNumber) {
                return pid;
            }
        }
        return 0;
    }

    PIDData& getOrCreatePIDData_unsafe(DWORD pid) {
        // No lock here, assumed to be called from a method that already holds the lock
        auto [it, inserted] = pidDataMap.try_emplace(pid, std::make_unique<PIDData>());
        if (inserted) {
            it->second->pid = pid;
        }
        return *(it->second);
    }
};