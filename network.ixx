#include "framework.h"

//import pathfinding;
import server_log_frame;
import pid_data_manager;
import time_helper;
import data_helpers;

export module network_module;

export namespace network {
   
    export ServerLogFrame server_log;
    

    // Constants
    constexpr int NAVIGATION_SOCKET = 54000;
    constexpr int MESSAGE_SOCKET = 54001;

    constexpr float MIN_DISTANCE = 1.05f;



    // ACCOUNT and PIDData structures
    struct ACCOUNT {
        int pid;
        std::optional<std::string> accountEmail;
        std::optional<int> health;

        ACCOUNT(const nlohmann::json& j) {
            pid = j.at("pid").get<int>();
            if (j.contains("accountEmail")) {
                accountEmail = j.at("accountEmail").get<std::string>();
            }
            if (j.contains("health")) {
                health = j.at("health").get<int>();
            }
        }

        nlohmann::json to_json() const {
            nlohmann::json j;
            j["pid"] = pid;
            if (accountEmail.has_value()) {
                j["accountEmail"] = accountEmail.value();
            }
            if (health.has_value()) {
                j["health"] = health.value();
            }
            return j;
        }
    };



    // Actual implementation of networking functions
    /*void navigation_transmission(SSL*& ssl) {
        float data[6]{};
        int received = SSL_read(ssl, reinterpret_cast<char*>(data), sizeof(data));

        if (received == sizeof(data)) {
            auto results = calculate_path(data);
            float result[3] = { data[0], data[1], data[2] };
            printf("Result: %f %f %f\n", result[0], result[1], result[2]);

            vector3 current(result);

            if (results.size() > 1) {
                for (auto& waypoint : results) {
                    if (current.distance(waypoint) >= MIN_DISTANCE) {
                        result[0] = waypoint.x;
                        result[1] = waypoint.y;
                        result[2] = waypoint.z;

                        break;
                    }
                }
            }

            int send = SSL_write(ssl, reinterpret_cast<const char*>(result), sizeof(result));

            if (send == -1) {
                ERR_print_errors_fp(stderr);
            }
        }
        else {
            std::cerr << "Failed to receive data from client! Quitting" << std::endl;
        }
    }*/

    


    std::chrono::system_clock::time_point readDataFromSSL(SSL*& ssl, char* dataBuffer, size_t bufferSize) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(SSL_get_fd(ssl), &readfds);
        struct timeval timeout;
        timeout.tv_sec = 5; // Timeout after 5 seconds
        timeout.tv_usec = 0;

        if (select(SSL_get_fd(ssl) + 1, &readfds, NULL, NULL, &timeout) > 0) {
            int bytesRead = SSL_read(ssl, dataBuffer, bufferSize - 1);
            if (bytesRead > 0) {
                dataBuffer[bytesRead] = '\0'; // Null-terminate the buffer
                return std::chrono::system_clock::now();
            }
            else {
                std::cerr << "Error reading from SSL socket" << std::endl;
            }
        }
        else {
            std::cerr << "Timeout or error on SSL_read" << std::endl;
        }
        return std::chrono::system_clock::time_point();
    }

    HWND getWindowHandleFromPID(DWORD pid) {
        HWND hwnd = NULL;
        do {
            hwnd = FindWindowEx(NULL, hwnd, NULL, NULL);
            DWORD windowPID;
            GetWindowThreadProcessId(hwnd, &windowPID);
            if (windowPID == pid) {
                printf("getWindowHandleFromPID: %d\n", pid);
                server_log.AddLog(std::to_string(pid), "getWindowHandleFromPID: ");
                return hwnd;
            }
        } while (hwnd != NULL);
        return NULL;
    }

    //std::unordered_map<DWORD, PIDData*> pidMap; // Changed to store pointers to PIDData
    //std::unordered_multimap<int, PIDData*> pidMultiMap;
    void sendTempObstaclesUpdateRequest(SSL*& ssl, const ACCOUNT& account, PIDData& pidData) {
        std::string updateRequest = "TEMP_OBSTACLES_UPDATE_REQUEST";
        nlohmann::json updateRequestJson;
        updateRequestJson["pid"] = account.pid;
        updateRequestJson["request"] = updateRequest;

        // Add timestamps to the JSON object
        updateRequestJson["time_server_sent_readable"] = time_helper::readable();
        updateRequestJson["time_server_sent_exact"] = time_helper::exact();

        std::string combinedMessage = updateRequestJson.dump();
        int send = SSL_write(ssl, combinedMessage.c_str(), combinedMessage.length());
        if (send <= 0) {
            ERR_print_errors_fp(stderr);
        }
        else {
            std::cout << "Combined message sent successfully." << std::endl;
            pidData.addSentMessage(combinedMessage);
            // Increment the total messages sent for this PID and log the count
            if (pidData.data.find("totalMessagesSent") != pidData.data.end()) {
                pidData.data["totalMessagesSent"] = std::get<int>(pidData.data["totalMessagesSent"]) + 1;
            }
            else {
                pidData.data["totalMessagesSent"] = 1;
            }
        }
    }
    void handleMessage(SSL*& ssl, const char* dataBuffer, std::chrono::system_clock::time_point timestamp) {
        std::cout << "Received message: " << dataBuffer << std::endl;
        server_log.AddLog("Received message: " + std::string(dataBuffer), "WORD!");

        // Parse JSON from buffer
        nlohmann::json parsedJson = nlohmann::json::parse(dataBuffer);
        ACCOUNT account(parsedJson);

        // Add timestamps to the ACCOUNT object
        parsedJson["time_server_received_readable"] = time_helper::readable();
        parsedJson["time_server_received_exact"] = time_helper::exact();

        // Convert the JSON back to a string
        std::string messageWithTimestamps = parsedJson.dump();

        std::cout << "Received from message from PID: " << account.pid << std::endl;
        server_log.AddLog("PID#: ");

        // Get window handle from PID
        HWND windowHandle = getWindowHandleFromPID(account.pid);
        std::cout << "Window handle obtained from PID: " << windowHandle << std::endl;

        // Manage PID data using PIDDataManager
        PIDDataManager& manager = PIDDataManager::getInstance();
        PIDData& pidData = manager.getOrCreatePIDData(account.pid);

        // Add the received message with timestamps to the PIDData
        pidData.addReceivedMessage(messageWithTimestamps);


        pidData.data["last_rawJson"] = std::string(dataBuffer);
        for (auto& element : parsedJson.items()) {
            std::string key = element.key();
            nlohmann::json value = element.value();

            if (key == "health" && value.is_number()) {
                // Store health as an int
                pidData.data[key] = value.get<int>();
            }
            
            else {
                // Convert other values to a string and store them
                pidData.data[key] = value.dump();
            }
        }

        // Update using the new key-value map in PIDData
        // Cast HWND to std::intptr_t and ensure std::variant supports this type
        pidData.data["hwnd"] = static_cast<std::intptr_t>(reinterpret_cast<std::intptr_t>(windowHandle));

        // Cast the timestamp count to an existing type in the variant, such as int
        pidData.data["timestamp"] = time_helper::readable();
        pidData.data["accountEmail"] = account.accountEmail.value_or("");
        pidData.data["imguiWindowOpen"] = false;

        // For total messages sent and received, handle incrementing if the key already exists
        if (pidData.data.find("totalMessagesReceived") != pidData.data.end()) {
            pidData.data["totalMessagesReceived"] = std::get<int>(pidData.data["totalMessagesReceived"]) + 1;
        }
        else {
            pidData.data["totalMessagesReceived"] = 1;
        }

        // Add the received message to the record
        // Assuming messageRecord is still a vector<string> and not part of the map
        pidData.messageRecord.push_back(std::string(dataBuffer));

        // Check if accountEmail is empty, if so, request for update
        auto email = std::get_if<std::string>(&pidData.data["accountEmail"]);
        if (email && email->empty()) {
            std::string updateRequest = "ACCOUNT_EMAIL_UPDATE_REQUEST";
            nlohmann::json updateRequestJson;
            updateRequestJson["pid"] = account.pid;
            updateRequestJson["request"] = updateRequest;

            // Add timestamps to the JSON object
            updateRequestJson["time_server_sent_readable"] = time_helper::readable();
            updateRequestJson["time_server_sent_exact"] = time_helper::exact();

            std::string combinedMessage = updateRequestJson.dump();
            int send = SSL_write(ssl, combinedMessage.c_str(), combinedMessage.length());
            if (send <= 0) {
                ERR_print_errors_fp(stderr);
            }
            else {
                std::cout << "Combined message sent successfully." << std::endl;
                pidData.addSentMessage(combinedMessage);
                // Increment the total messages sent for this PID and log the count
                if (pidData.data.find("totalMessagesSent") != pidData.data.end()) {
                    pidData.data["totalMessagesSent"] = std::get<int>(pidData.data["totalMessagesSent"]) + 1;
                }
                else {
                    pidData.data["totalMessagesSent"] = 1;
                }
            }
        }

        if (pidData.data.find("temp_obstacles") == pidData.data.end()) {
            // "temp_obstacles" is not in the map, send the message
            sendTempObstaclesUpdateRequest(ssl, account, pidData);
        }
        else {
            auto temp_obstacles = std::get_if<std::vector<std::string>>(&pidData.data["temp_obstacles"]);
            if (temp_obstacles && temp_obstacles->empty()) {
                // "temp_obstacles" is in the map but it's empty, send the message
                sendTempObstaclesUpdateRequest(ssl, account, pidData);
            }
        }
        
        
    }

    std::chrono::system_clock::time_point message_transmission(SSL*& ssl) {
        char dataBuffer[1024] = { 0 };
        std::chrono::system_clock::time_point timestamp = readDataFromSSL(ssl, dataBuffer, sizeof(dataBuffer));
        if (timestamp != std::chrono::system_clock::time_point()) {
            handleMessage(ssl, dataBuffer, timestamp);
        }
        return timestamp;
    }


    // Template function implementations
    template <typename TransmissionFunction>
    void handle_client(SOCKET client_socket, SSL_CTX* ssl_ctx, TransmissionFunction transmission) {
        SSL* ssl = SSL_new(ssl_ctx);
        SSL_set_fd(ssl, (int)client_socket);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            closesocket(client_socket);
            return;
        }

        transmission(ssl);

        SSL_free(ssl);
        closesocket(client_socket);
    }


    // Template function to handle connections
    template <typename TransmissionFunction>
    void handle_connections(SOCKET socket, SSL_CTX* ssl_ctx, TransmissionFunction transmission) {
        while (true) {
            sockaddr_in client{};
            int client_size = sizeof(client);
            SOCKET client_socket = accept(socket, (sockaddr*)&client, &client_size);
            if (client_socket != INVALID_SOCKET) {
                std::thread(handle_client<TransmissionFunction>, client_socket, ssl_ctx, transmission).detach();
            }
        }
    }

    // Implementations for any other necessary types, functions, constants
    // ...

    
}

