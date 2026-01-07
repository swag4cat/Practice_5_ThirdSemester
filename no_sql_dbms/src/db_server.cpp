#include "../include/collection.hpp"
#include "../include/hash_map.hpp"
#include "../parcer/json.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <atomic>

using json = nlohmann::json;

struct ClientInfo {
    int socket;
    std::string address;
    std::chrono::steady_clock::time_point connect_time;
    std::string database;
    int request_count;
};

class DBServer {
private:
    int port;
    std::string db_dir;
    HashMap<Collection*> collections;
    HashMap<std::shared_mutex*> db_mutexes;
    std::mutex collections_mutex;
    std::atomic<int> client_count{0};

    HashMap<ClientInfo*> connected_clients;
    std::mutex clients_mutex;

public:
    DBServer(int p, const std::string& dir) : port(p), db_dir(dir), client_count(0) {}

    ~DBServer() {
        std::cout << "Saving all collections and cleaning up..." << std::endl;

        auto collection_items = collections.items();
        for (auto& pair : collection_items) {
            if (pair.second) {
                pair.second->save();
                delete pair.second;
            }
        }

        auto mutex_items = db_mutexes.items();
        for (auto& pair : mutex_items) {
            delete pair.second;
        }

        auto client_items = connected_clients.items();
        for (auto& pair : client_items) {
            delete pair.second;
        }

        std::cout << "Server shutdown complete" << std::endl;
    }

    void start() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 10) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        std::cout << "DB Server listening on port " << port << std::endl;
        std::cout << "Database directory: " << db_dir << std::endl;

        while (true) {
            int client_socket = accept(server_fd, nullptr, nullptr);
            if (client_socket < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                }
                perror("accept");
                continue;
            }

            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            int current_count = ++client_count;

            add_client(client_socket, current_count);

            std::cout << "New client connected. Total clients: " << current_count << std::endl;
            print_clients_info();

            std::thread client_thread(&DBServer::handle_client, this, client_socket);
            client_thread.detach();
        }
    }

private:
    void add_client(int client_socket, int client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        ClientInfo* client = new ClientInfo{
            client_socket,
            "client_" + std::to_string(client_id),
            std::chrono::steady_clock::now(),
            "",
            0
        };
        std::string client_key = "client_" + std::to_string(client_socket);
        connected_clients.put(client_key, client);
    }

    void update_client_database(int client_socket, const std::string& db_name) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::string client_key = "client_" + std::to_string(client_socket);
        ClientInfo* client = nullptr;
        if (connected_clients.get(client_key, client)) {
            client->database = db_name;
            client->request_count++;
        }
    }

    void remove_client(int client_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::string client_key = "client_" + std::to_string(client_socket);
        ClientInfo* client = nullptr;
        if (connected_clients.get(client_key, client)) {
            connected_clients.remove(client_key);
            delete client;
            std::cout << "Client " << client_key << " removed from HashMap" << std::endl;
        }
    }

    void print_clients_info() {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto clients = connected_clients.items();
        std::cout << "Connected clients (" << clients.size() << "):" << std::endl;
        for (const auto& pair : clients) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - pair.second->connect_time);
            std::cout << "  • " << pair.first << " - DB: "
            << (pair.second->database.empty() ? "none" : pair.second->database)
            << ", requests: " << pair.second->request_count
            << ", connected: " << duration.count() << "s" << std::endl;
        }
    }

    void handle_client(int client_socket) {
        char buffer[4096] = {0};
        std::cout << "Client handler started for socket " << client_socket << std::endl;

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);

            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    std::cout << "Client disconnected normally" << std::endl;
                } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                } else {
                    std::cout << "Read error, disconnecting client" << std::endl;
                }
                break;
            }

            std::cout << "Received request from client " << client_socket << std::endl;

            try {
                json request = json::parse(buffer);
                json response = process_request(request);

                if (request.contains("database")) {
                    update_client_database(client_socket, request["database"]);
                }

                std::string response_str = response.dump() + "\n";

                int bytes_sent = send(client_socket, response_str.c_str(), response_str.length(), 0);
                if (bytes_sent <= 0) {
                    if (errno == EWOULDBLOCK || errno == EAGAIN) {
                        bytes_sent = send(client_socket, response_str.c_str(), response_str.length(), 0);
                        if (bytes_sent <= 0) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "Error processing request: " << e.what() << std::endl;
                json error_response = {
                    {"status", "error"},
                    {"message", std::string("Server error: ") + e.what()}
                };
                std::string error_str = error_response.dump() + "\n";
                send(client_socket, error_str.c_str(), error_str.length(), 0);
            }
        }

        remove_client(client_socket);
        close(client_socket);
        --client_count;
        std::cout << "Client handler finished for socket " << client_socket << std::endl;
    }

    json process_request(const json& request) {
        if (!request.contains("database") || !request.contains("operation")) {
            return {{"status", "error"}, {"message", "Invalid request format"}};
        }

        std::string db_name = request["database"];
        std::string operation = request["operation"];

        if (db_name.empty()) {
            return {{"status", "error"}, {"message", "Database name cannot be empty"}};
        }

        Collection* coll = get_collection(db_name);
        if (!coll) {
            return {{"status", "error"}, {"message", "Failed to create or access collection"}};
        }

        std::shared_mutex* db_mutex = get_db_mutex(db_name);
        if (!db_mutex) {
            return {{"status", "error"}, {"message", "Failed to get database mutex"}};
        }

        try {
            if (operation == "insert" || operation == "delete") {
                auto start = std::chrono::steady_clock::now();
                bool locked = false;

                while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
                    if (db_mutex->try_lock()) {
                        locked = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                if (!locked) {
                    return {{"status", "error"}, {"message", "Database lock timeout"}};
                }

                std::lock_guard<std::shared_mutex> write_lock(*db_mutex, std::adopt_lock);
                return execute_write_operation(coll, request, operation);

            } else if (operation == "find") {
                std::shared_lock<std::shared_mutex> read_lock(*db_mutex);
                return execute_read_operation(coll, request);

            } else {
                return {{"status", "error"}, {"message", "Unknown operation: " + operation}};
            }
        } catch (const std::exception& e) {
            return {{"status", "error"}, {"message", std::string("Operation failed: ") + e.what()}};
        }
    }

    json execute_write_operation(Collection* coll, const json& request, const std::string& operation) {
        if (operation == "insert") {
            if (!request.contains("data") || !request["data"].is_array()) {
                return {{"status", "error"}, {"message", "Insert operation requires data array"}};
            }

            std::vector<std::string> inserted_ids;
            for (const auto& doc : request["data"]) {
                if (!doc.is_object()) {
                    return {{"status", "error"}, {"message", "Document must be a JSON object"}};
                }

                try {
                    std::string id = coll->insert(doc);
                    inserted_ids.push_back(id);
                } catch (const std::exception& e) {
                    return {{"status", "error"}, {"message", std::string("Insert failed: ") + e.what()}};
                }
            }

            coll->save();

            return {
                {"status", "success"},
                {"message", "Inserted " + std::to_string(inserted_ids.size()) + " documents"},
                {"data", inserted_ids},
                {"count", inserted_ids.size()}
            };

        } else if (operation == "delete") {
            if (!request.contains("query")) {
                return {{"status", "error"}, {"message", "Delete operation requires query"}};
            }

            try {
                int deleted_count = coll->remove(request["query"]);

                if (deleted_count > 0) {
                    coll->save();
                }

                return {
                    {"status", "success"},
                    {"message", "Deleted " + std::to_string(deleted_count) + " documents"},
                    {"count", deleted_count}
                };
            } catch (const std::exception& e) {
                return {{"status", "error"}, {"message", std::string("Delete failed: ") + e.what()}};
            }
        }

        return {{"status", "error"}, {"message", "Unknown write operation"}};
    }

    json execute_read_operation(Collection* coll, const json& request) {
        if (!request.contains("query")) {
            return {{"status", "error"}, {"message", "Find operation requires query"}};
        }

        try {
            auto results = coll->find(request["query"]);
            std::vector<json> result_docs;
            for (const auto& doc : results) {
                result_docs.push_back(doc);
            }

            return {
                {"status", "success"},
                {"message", "Found " + std::to_string(result_docs.size()) + " documents"},
                {"data", result_docs},
                {"count", result_docs.size()}
            };
        } catch (const std::exception& e) {
            return {{"status", "error"}, {"message", std::string("Find failed: ") + e.what()}};
        }
    }

    Collection* get_collection(const std::string& db_name) {
        std::lock_guard<std::mutex> lock(collections_mutex);

        Collection* coll = nullptr;
        if (!collections.get(db_name, coll)) {
            std::cout << "Creating new collection: " << db_name << std::endl;
            coll = new Collection(db_dir, db_name);
            collections.put(db_name, coll);
        }
        return coll;
    }

    std::shared_mutex* get_db_mutex(const std::string& db_name) {
        std::shared_mutex* mutex = nullptr;
        if (!db_mutexes.get(db_name, mutex)) {
            mutex = new std::shared_mutex();
            db_mutexes.put(db_name, mutex);
        }
        return mutex;
    }
};

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <database_directory>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string db_dir = argv[2];

    std::cout << "Starting DB Server on port " << port << " with data directory: " << db_dir << std::endl;

    try {
        DBServer server(port, db_dir);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Server fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
