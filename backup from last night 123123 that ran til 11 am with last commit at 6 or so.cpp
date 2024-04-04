#include "framework.h"
#include <chrono>
#include <windows.h>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <optional>



const int NAVIGATION_SOCKET = 54000;
const int MESSAGE_SOCKET = 54001;
const int REQUEST_SOCKET = 54002; // Added request socket

SOCKET navigation_socket;
SOCKET message_socket;
SOCKET request_socket; // Added request socket

SSL_CTX* ssl_ctx;

// Define a struct to hold the data for each PID
struct PIDData {
	HWND hwnd;
	std::chrono::system_clock::time_point timestamp;
	std::string accountEmail; // Added accountEmail field
	// Add other fields as needed
	// std::string password;
	// std::string characterName;
	// int level;
	// int health;
	// std::tuple<int, int, int> position;
};

std::unordered_map<DWORD, PIDData*> pidMap; // Changed to store pointers to PIDData



constexpr int MAX_POLYS = 1024;
constexpr int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T'; // 'MSET'
constexpr int NAVMESHSET_VERSION = 1;

constexpr float MIN_DISTANCE = 1.05f;

dtNavMesh* mesh{};
dtNavMeshQuery* query{};
dtPolyRef poly_path[MAX_POLYS]{};

enum PolyFlags
{
	POLYFLAGS_WALK = 0x01,
	POLYFLAGS_SWIM = 0x02,
	POLYFLAGS_DOOR = 0x04,
	POLYFLAGS_JUMP = 0x08,
	POLYFLAGS_DISABLED = 0x10,
	POLYFLAGS_ALL = 0xffff
};

struct NavMeshSetHeader
{
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
};

struct NavMeshTileHeader
{
	dtTileRef tileRef;
	int dataSize;
};

dtNavMesh* load_tiles(const char* path)
{
	FILE* fp = nullptr;

	if (fopen_s(&fp, path, "rb") != 0) {
		printf("Error opening file\n");

		return 0;
	}

	NavMeshSetHeader header;
	size_t readLen = fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	if (readLen != 1)
	{
		fclose(fp);
		return 0;
	}
	if (header.magic != NAVMESHSET_MAGIC)
	{
		fclose(fp);
		return 0;
	}
	if (header.version != NAVMESHSET_VERSION)
	{
		fclose(fp);
		return 0;
	}

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh)
	{
		fclose(fp);
		return 0;
	}
	dtStatus status = mesh->init(&header.params);
	if (dtStatusFailed(status))
	{
		fclose(fp);
		return 0;
	}

	for (int i = 0; i < header.numTiles; ++i)
	{
		NavMeshTileHeader tileHeader;
		readLen = fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (readLen != 1)
		{
			fclose(fp);
			return 0;
		}

		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		readLen = fread(data, tileHeader.dataSize, 1, fp);
		if (readLen != 1)
		{
			dtFree(data);
			fclose(fp);
			return 0;
		}

		mesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);
	}

	fclose(fp);

	return mesh;
}

void coords_rd(float* pos) noexcept
{
	float ox = pos[0];
	pos[0] = pos[1];
	pos[1] = pos[2];
	pos[2] = ox;
}
void coords_wow(float* pos) noexcept
{
	float oz = pos[2];
	pos[2] = pos[1];
	pos[1] = pos[0];
	pos[0] = oz;
}

std::vector<vector3> find_path(float* data, float* path, int* sz_path)
{
	auto start = &data[0];
	auto end = &data[3];

	float rd_start[3]{};
	dtVcopy(rd_start, start);
	coords_rd(rd_start);

	float rd_end[3]{};
	dtVcopy(rd_end, end);
	coords_rd(rd_end);

	float extents[3]{ 6.0f, 6.0f, 6.0f };

	dtQueryFilter filter;
	dtPolyRef start_poly, end_poly;

	filter.setIncludeFlags(POLYFLAGS_WALK);
	filter.setExcludeFlags(0);

	query->findNearestPoly(rd_start, extents, &filter, &start_poly, rd_start);
	query->findNearestPoly(rd_end, extents, &filter, &end_poly, rd_end);

	auto status = query->findPath(start_poly, end_poly, rd_start, rd_end, &filter, poly_path, sz_path, MAX_POLYS);

	std::vector<vector3> results{};

	if (dtStatusSucceed(status))
	{
		dtStatus straight_status = query->findStraightPath
		(
			rd_start,
			rd_end,
			poly_path,
			*sz_path,
			path,
			nullptr,
			nullptr,
			sz_path,
			MAX_POLYS
		);

		if (dtStatusSucceed(straight_status))
		{
			for (int i = 0; i < *sz_path * 3; i += 3)
			{
				auto waypoint_rd = &path[i];
				float waypoint[3];
				dtVcopy(waypoint, waypoint_rd);
				coords_wow(waypoint);

				results.emplace_back(waypoint);
			}
		}
	}

	return results;
}

std::vector<vector3> calculate_path(float* data) {
	float* path = new float[MAX_POLYS];
	int sz_path = 0;

	auto results = find_path(data, path, &sz_path);

	printf("Path Size: %lli\n", results.size());

	delete[] path;

	return results;
}

int load_mesh()
{
	std::filesystem::path source_path(__FILE__);

	// WARNING: Make sure it's production ready IO
	auto mesh_name = "all_tiles_navmesh.bin";
	auto parent_path = source_path.parent_path().parent_path().append("maps_retail\\").string();
	auto mesh_path = parent_path + mesh_name;

	std::cout << "Loaded Mesh: " << mesh_path << std::endl;

	mesh = load_tiles(mesh_path.c_str());

	if (!mesh) {
		std::cerr << "Failed to initalize mesh" << std::endl;
		return -1;
	}

	query = dtAllocNavMeshQuery();

	if (!query) {
		std::cerr << "Failed to allocate mesh query!" << std::endl;

		return -1;
	}

	if (query->init(mesh, MAX_POLYS) != DT_SUCCESS) {
		std::cerr << "Failed to init mesh!" << std::endl;

		dtFree(query);
		return -1;
	}

	return 1;
}

// Function to get window handle from PID
HWND getWindowHandleFromPID(DWORD pid) {
	HWND hwnd = NULL;
	do {
		hwnd = FindWindowEx(NULL, hwnd, NULL, NULL);
		DWORD windowPID;
		GetWindowThreadProcessId(hwnd, &windowPID);
		if (windowPID == pid) {
			printf("Found window with PID: %d\n", pid);
			return hwnd;
		}
	} while (hwnd != NULL);
	return NULL;
}



SSL_CTX* init_server_ctx()
{
	const SSL_METHOD* method;
	SSL_CTX* ctx;

	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	method = TLS_server_method();
	ctx = SSL_CTX_new(method);
	if (!ctx)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}

	return ctx;
}

void load_certificates(SSL_CTX* ctx, const char* cert_file, const char* key_file)
{
	// WARNING: Make sure paths are correct for production/local
	std::filesystem::path source_path(__FILE__);
	std::filesystem::path parent_path = source_path.parent_path();
	std::filesystem::path ssl_dir = parent_path.parent_path().append("ssl\\");

	auto cert_path = ssl_dir.string() + cert_file;
	auto key_path = ssl_dir.string() + key_file;

	if (SSL_CTX_use_certificate_file(ctx, cert_path.c_str(), SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key_path.c_str(), SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}

	if (!SSL_CTX_check_private_key(ctx))
	{
		fprintf(stderr, "Private key does not match the public certificate\n");
		abort();
	}
}

//NAV REQUEST FROM GAME HANDLING
void navigation_transmission(SSL*& ssl) {
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
}

// Navigation socket
void handle_navigation_client(SOCKET client_socket, SSL_CTX* ssl_ctx)
{
	SSL* ssl = SSL_new(ssl_ctx);
	SSL_set_fd(ssl, (int)client_socket);

	if (SSL_accept(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
		SSL_free(ssl);
		closesocket(client_socket);
		return;
	}

	navigation_transmission(ssl);

	SSL_free(ssl);
	closesocket(client_socket);
}

void handle_navigation_connections(SOCKET navigation_socket, SSL_CTX* ssl_ctx) {
	while (true) {
		sockaddr_in client{};
		int client_size = sizeof(client);
		SOCKET client_socket = accept(navigation_socket, (sockaddr*)&client, &client_size);
		if (client_socket != INVALID_SOCKET) {
			std::thread(handle_navigation_client, client_socket, ssl_ctx).detach();
		}
	}
}

// Message transmission with timestamp
//std::chrono::system_clock::time_point message_transmission(SSL*& ssl) {
//	char buffer[1024] = { 0 };
//	fd_set readfds;
//	FD_ZERO(&readfds);
//	FD_SET(SSL_get_fd(ssl), &readfds);
//	struct timeval timeout;
//	timeout.tv_sec = 5; // Timeout after 5 seconds
//	timeout.tv_usec = 0;
//
//	if (select(SSL_get_fd(ssl) + 1, &readfds, NULL, NULL, &timeout) > 0) {
//		int valread = SSL_read(ssl, buffer, sizeof(buffer) - 1);
//		std::chrono::system_clock::time_point timestamp;
//		if (valread > 0) {
//			buffer[valread] = '\0'; // Null-terminate the buffer
//			printf("Received message: %s\n", buffer);
//			timestamp = std::chrono::system_clock::now();
//
//			// Convert timestamp to string
//			std::time_t tt = std::chrono::system_clock::to_time_t(timestamp);
//			char timestamp_str[26];
//			ctime_s(timestamp_str, sizeof timestamp_str, &tt);
//
//			// Send timestamp back to the game
//			int send = SSL_write(ssl, timestamp_str, strlen(timestamp_str));
//			if (send <= 0) {
//				ERR_print_errors_fp(stderr);
//			}
//		}
//		else {
//			std::cerr << "Error reading from SSL socket" << std::endl;
//		}
//		return timestamp;
//	}
//	else {
//		std::cerr << "Timeout or error on SSL_read" << std::endl;
//		return std::chrono::system_clock::time_point();
//	}
//}
// Message transmission with timestamp and window resizing

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


std::chrono::system_clock::time_point message_transmission(SSL*& ssl) {
	char buffer[1024] = { 0 };
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(SSL_get_fd(ssl), &readfds);
	struct timeval timeout;
	timeout.tv_sec = 5; // Timeout after 5 seconds
	timeout.tv_usec = 0;

	if (select(SSL_get_fd(ssl) + 1, &readfds, NULL, NULL, &timeout) > 0) {
		int valread = SSL_read(ssl, buffer, sizeof(buffer) - 1);
		std::chrono::system_clock::time_point timestamp;
		if (valread > 0) {
			buffer[valread] = '\0'; // Null-terminate the buffer
			std::cout << "Received message: " << buffer << std::endl;
			timestamp = std::chrono::system_clock::now();
			std::cout << "Current timestamp: " << timestamp.time_since_epoch().count() << std::endl;

			// Parse JSON from buffer
			nlohmann::json j = nlohmann::json::parse(buffer);
			ACCOUNT account(j);

			std::cout << "Received from message from PID: " << account.pid << std::endl;
			// Get window handle from PID
			HWND hwnd = getWindowHandleFromPID(account.pid);
			std::cout << "Window handle obtained from PID: " << hwnd << std::endl;

			// If PID is not already in the map, add it
			if (pidMap.find(account.pid) == pidMap.end()) {
				// Check if any existing pid has the same account email
				for (auto it = pidMap.begin(); it != pidMap.end(); ) {
					if (it->second->accountEmail == account.accountEmail.value_or("")) {
						// Delete that entire pid before adding in the new pid
						std::cout << "Deleting PID#" << it->first << " from map." << std::endl; // Print the PID that is being deleted
						delete it->second;
						it = pidMap.erase(it);
					}
					else {
						++it;
					}
				}

				PIDData* data = new PIDData; // Allocate new PIDData on the heap
				data->hwnd = hwnd;
				data->timestamp = timestamp;
				// Initialize other fields as needed
				data->accountEmail = account.accountEmail.value_or(""); // Initialize accountEmail as empty if not present
				pidMap[account.pid] = data;
				std::cout << "Adding PID#" << account.pid << " to map, new map: ";
				for (const auto& pid : pidMap) {
					std::cout << pid.first << " ";
				}
				std::cout << std::endl;
			}

			// Check if accountEmail is empty, if so, request for update
			if (pidMap[account.pid]->accountEmail.empty()) {
				std::string updateRequest = "ACCOUNT_EMAIL_UPDATE_REQUEST";
				nlohmann::json updateRequestJson;
				updateRequestJson["pid"] = account.pid;
				updateRequestJson["request"] = updateRequest;
				std::string combinedMessage = updateRequestJson.dump();
				int send = SSL_write(ssl, combinedMessage.c_str(), combinedMessage.length());
				if (send <= 0) {
					ERR_print_errors_fp(stderr);
				}
				else {
					std::cout << "Combined message sent successfully." << std::endl;
				}
			}
		}
		else {
			std::cerr << "Error reading from SSL socket" << std::endl;
		}
		return timestamp;
	}
	else {
		std::cerr << "Timeout or error on SSL_read" << std::endl;
		return std::chrono::system_clock::time_point();
	}
}

// Message socket with SSL
void handle_message_client(SOCKET client_socket, SSL_CTX* ssl_ctx) {
	SSL* ssl = SSL_new(ssl_ctx);
	SSL_set_fd(ssl, (int)client_socket);

	if (SSL_accept(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
		SSL_free(ssl);
		closesocket(client_socket);
		return;
	}

	message_transmission(ssl);

	SSL_free(ssl);
	closesocket(client_socket);
}

void handle_message_connections(SOCKET message_socket, SSL_CTX* ssl_ctx) {
	while (true) {
		sockaddr_in client{};
		int client_size = sizeof(client);
		SOCKET client_socket = accept(message_socket, (sockaddr*)&client, &client_size);
		if (client_socket != INVALID_SOCKET) {
			std::thread(handle_message_client, client_socket, ssl_ctx).detach();
		}
	}
}
void request_transmission(SSL*& ssl) {
    char buffer[1024] = { 0 };
    int valread = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (valread > 0) {
        buffer[valread] = '\0'; // Null-terminate the buffer
        std::cout << "Received request: " << buffer << std::endl;

        // Parse JSON from buffer
        nlohmann::json j = nlohmann::json::parse(buffer);
        ACCOUNT account(j);

        std::cout << "Received request from PID: " << account.pid << std::endl;

        // If PID is in the map, process the request
        if (pidMap.find(account.pid) != pidMap.end()) {
            // Process the request based on the account details
            // This is where you would add your request processing code
            // ...

            // Send a response back to the client
            std::string response = "Response to request";
            int send = SSL_write(ssl, response.c_str(), response.length());
            if (send <= 0) {
                ERR_print_errors_fp(stderr);
            }
        }
    }
    else {
        std::cerr << "Error reading from SSL socket" << std::endl;
    }
}

void handle_request_client(SOCKET client_socket, SSL_CTX* ssl_ctx)
{
    SSL* ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, (int)client_socket);

    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        closesocket(client_socket);
        return;
    }

    char buffer[1024] = { 0 };
    int valread = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (valread > 0) {
        buffer[valread] = '\0'; // Null-terminate the buffer
        std::string accountEmail(buffer);

        // Search for the account email in the pidMap
        for (const auto& pid : pidMap) {
            if (pid.second->accountEmail == accountEmail) {
                // Convert PIDData to JSON and send it back
                nlohmann::json j;
                j["pid"] = pid.first;
                j["hwnd"] = (int)pid.second->hwnd;
                j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(pid.second->timestamp.time_since_epoch()).count();
                j["accountEmail"] = pid.second->accountEmail;
                // Add other fields as needed
                // j["password"] = pid.second->password;
                // j["characterName"] = pid.second->characterName;
                // j["level"] = pid.second->level;
                // j["health"] = pid.second->health;
                // j["position"] = pid.second->position;

                std::string response = j.dump();
                int send = SSL_write(ssl, response.c_str(), response.length());
                if (send <= 0) {
                    ERR_print_errors_fp(stderr);
                }
                break;
            }
        }
    }

    request_transmission(ssl);

    SSL_free(ssl);
    closesocket(client_socket);
}

void handle_request_connections(SOCKET request_socket, SSL_CTX* ssl_ctx) {
    while (true) {
        sockaddr_in client{};
        int client_size = sizeof(client);
        SOCKET client_socket = accept(request_socket, (sockaddr*)&client, &client_size);
        if (client_socket != INVALID_SOCKET) {
            std::thread(handle_request_client, client_socket, ssl_ctx).detach();
        }
    }
}


void cleanup() {
	// Cleanup OpenSSL context
	SSL_CTX_free(ssl_ctx);

	// Cleanup listening socket
	closesocket(navigation_socket);
	closesocket(message_socket);

	// Cleanup Winsock
	WSACleanup();

	// Free mesh and query
	dtFreeNavMeshQuery(query);
	dtFreeNavMesh(mesh);
}

int main()
{
	load_mesh();

	WSADATA ws_data{};

	// Initialize OpenSSL
	ssl_ctx = init_server_ctx();
	load_certificates(ssl_ctx, "cert.crt", "private.key");

	// Initialize Winsock
	if (!ws_data.wVersion) {
		WORD ver = MAKEWORD(2, 2);
		int ws_ok = WSAStartup(ver, &ws_data);
		if (ws_ok != 0) {
			std::cerr << "Can't initialize winsock! Quitting" << std::endl;
			return -1;
		}
	}


	// Create and bind the navigation socket
	navigation_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (navigation_socket == INVALID_SOCKET)
	{
		std::cerr << "Can't create socket! Quitting" << std::endl;
		WSACleanup();
		return -1;
	}
	sockaddr_in navigation_hint{};
	navigation_hint.sin_family = AF_INET;
	navigation_hint.sin_port = htons(54000); // Navigation port
	navigation_hint.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(navigation_socket, (sockaddr*)&navigation_hint, sizeof(navigation_hint)) == SOCKET_ERROR)
	{
		std::cerr << "Can't bind socket! Quitting" << std::endl;
		closesocket(navigation_socket);
		WSACleanup();
		return -1;
	}

	// Create and bind the message socket
	message_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (message_socket == INVALID_SOCKET)
	{
		std::cerr << "Can't create socket! Quitting" << std::endl;
		WSACleanup();
		return -1;
	}
	sockaddr_in message_hint{};
	message_hint.sin_family = AF_INET;
	message_hint.sin_port = htons(54001); // Message port
	message_hint.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(message_socket, (sockaddr*)&message_hint, sizeof(message_hint)) == SOCKET_ERROR)
	{
		std::cerr << "Can't bind socket! Quitting" << std::endl;
		closesocket(message_socket);
		WSACleanup();
		return -1;
	}

	// Create and bind the request socket
	request_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (request_socket == INVALID_SOCKET)
	{
		std::cerr << "Can't create socket! Quitting" << std::endl;
		WSACleanup();
		return -1;
	}
	sockaddr_in request_hint{};
	request_hint.sin_family = AF_INET;
	request_hint.sin_port = htons(REQUEST_SOCKET); // Request port
	request_hint.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(request_socket, (sockaddr*)&request_hint, sizeof(request_hint)) == SOCKET_ERROR)
	{
		std::cerr << "Can't bind socket! Quitting" << std::endl;
		closesocket(request_socket);
		WSACleanup();
		return -1;
	}

	
	// Listen on both sockets
	listen(navigation_socket, SOMAXCONN);
	listen(message_socket, SOMAXCONN);
	listen(request_socket, SOMAXCONN);
	
	// Start threads for handling connections
	std::thread navigation_thread([=]() { handle_navigation_connections(navigation_socket, ssl_ctx); });
	std::thread message_thread([=]() { handle_message_connections(message_socket, ssl_ctx); });
	std::thread request_thread([=]() { handle_request_connections(request_socket, ssl_ctx); });


	// Wait for threads to finish forever
	navigation_thread.join();
	message_thread.join();

	// Register cleanup function to be called on exit
	atexit(cleanup);
	
	return 0;
}