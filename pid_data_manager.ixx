#include "framework.h"

export module pid_data_manager;

export struct PIDData {
    bool relog = false;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    int accountNumber;
    std::unordered_map<std::string, std::variant<int, std::string, bool, std::intptr_t, HWND, std::vector<std::string>, std::chrono::system_clock::time_point>> data;
    std::vector<std::string> sentMessages;
    std::vector<std::string> receivedMessages;

    void addSentMessage(const std::string& message) {
        sentMessages.push_back(message);

    }

    void addReceivedMessage(const std::string& message) {
        receivedMessages.push_back(message);

        // If this is the first message, set c_init to true
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
};

export class PIDDataManager {
public:
    static PIDDataManager& getInstance() {
        static PIDDataManager instance;
        return instance;
    }

    PIDData& getOrCreatePIDData(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        if (pidDataMap.find(pid) == pidDataMap.end()) {
            pidDataMap[pid].isNewPID = true;

            pidDataMap[pid].c_init = false;
        }
        return pidDataMap[pid];
    }

    void updatePIDData(int pid, const PIDData& data) {
        std::lock_guard<std::mutex> lock(mapMutex);
        pidDataMap[pid] = data;
    }

    void deactivatePID(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        if (pidDataMap.find(pid) != pidDataMap.end()) {
            oldPIDDataMap[pid] = std::move(pidDataMap[pid]);
            pidDataMap.erase(pid);
        }
    }

    void addLaunchingPID(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        launchingPIDDataMap[pid].isNewPID = true;
    }

    void removeLaunchingPID(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        launchingPIDDataMap.erase(pid);
    }

    const std::unordered_map<int, PIDData>& getAllActivePIDData() const {
        return pidDataMap;
    }

    const std::unordered_map<int, PIDData>& getAllOldPIDData() const {
        return oldPIDDataMap;
    }

    const std::unordered_map<int, PIDData>& getAllLaunchingPIDData() const {
        return launchingPIDDataMap;
    }

    std::string getAccountEmail(int pid) {
        std::lock_guard<std::mutex> lock(mapMutex);
        if (pidDataMap.find(pid) != pidDataMap.end()) {
            return pidDataMap[pid].accountEmail;
        }
        return "";
    }

private:
    PIDDataManager() {}
    std::unordered_map<int, PIDData> pidDataMap;
    std::unordered_map<int, PIDData> oldPIDDataMap;
    std::unordered_map<int, PIDData> launchingPIDDataMap;
    mutable std::mutex mapMutex;

    PIDDataManager(const PIDDataManager&) = delete;
    PIDDataManager& operator=(const PIDDataManager&) = delete;
};
