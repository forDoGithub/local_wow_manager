#include "framework.h"

export module pid_data_manager;


export struct PIDData {
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
        cvMonitor(std::make_unique<std::condition_variable>()) {}

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
    static PIDDataManager& getInstance() {
        static PIDDataManager instance;
        return instance;
    }

    PIDData& getOrCreatePIDData(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto [it, inserted] = pidDataMap.emplace(pid, std::make_unique<PIDData>());
        return *(it->second);
    }

    void updatePIDData(DWORD pid, std::unique_ptr<PIDData> data) {
        std::lock_guard<std::mutex> lock(mapMutex);
        pidDataMap[pid] = std::move(data);
        std::cout << "Updated PID data for PID " << pid << ", Running: " << pidDataMap[pid]->running << std::endl;
    }

    void deactivatePID(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto it = pidDataMap.find(pid);
        if (it != pidDataMap.end()) {
            oldPIDDataMap[pid] = std::move(it->second);
            pidDataMap.erase(it);
        }
    }

    void addLaunchingPID(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        auto [it, inserted] = launchingPIDDataMap.emplace(pid, std::make_unique<PIDData>());
        it->second->isNewPID = true;
        it->second->startTime = std::chrono::system_clock::now();
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

private:
    PIDDataManager() {}
    std::unordered_map<int, std::unique_ptr<PIDData>> pidDataMap;
    std::unordered_map<int, std::unique_ptr<PIDData>> oldPIDDataMap;
    std::unordered_map<int, std::unique_ptr<PIDData>> launchingPIDDataMap;
    mutable std::mutex mapMutex;

    PIDDataManager(const PIDDataManager&) = delete;
    PIDDataManager& operator=(const PIDDataManager&) = delete;
};