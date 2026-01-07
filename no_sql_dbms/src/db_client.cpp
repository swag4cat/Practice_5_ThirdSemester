#include "../parcer/json.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>

using json = nlohmann::json;

class DBClient {
private:
    std::string host;
    int port;
    std::string database;
    int sock = -1;
    bool connected = false;

public:
    DBClient(const std::string& h, int p, const std::string& db)
    : host(h), port(p), database(db) {}

    ~DBClient() {
        disconnect();
    }

    bool connect() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Socket creation error" << std::endl;
            return false;
        }

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address: " << host << std::endl;
            if (host == "localhost") {
                std::cout << "Trying 127.0.0.1 instead..." << std::endl;
                if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
                    std::cerr << "Invalid address: 127.0.0.1" << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        }

        std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
            if (::connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
                connected = true;
                std::cout << "Connected to " << host << ":" << port << " database: " << database << std::endl;
                return true;
            }
            std::cout << "Connection attempt failed, retrying..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cerr << "Connection timeout after 10 seconds" << std::endl;
        return false;
    }

    void disconnect() {
        if (sock >= 0) {
            close(sock);
            sock = -1;
            connected = false;
        }
    }

    bool reconnect() {
        disconnect();
        std::cout << "Attempting to reconnect..." << std::endl;
        return connect();
    }

    json send_request(const json& request) {
        if (!connected) {
            throw std::runtime_error("Not connected to server");
        }

        std::string request_str = request.dump() + "\n";
        int bytes_sent = send(sock, request_str.c_str(), request_str.length(), 0);

        if (bytes_sent <= 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                throw std::runtime_error("Send timeout");
            }
            connected = false;
            throw std::runtime_error("Server disconnected during send");
        }

        char buffer[4096] = {0};
        int bytes_read = read(sock, buffer, sizeof(buffer) - 1);

        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                connected = false;
                throw std::runtime_error("Server closed connection");
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                throw std::runtime_error("Receive timeout");
            } else {
                connected = false;
                throw std::runtime_error("Read error");
            }
        }

        try {
            return json::parse(buffer);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Invalid response from server: ") + e.what());
        }
    }

    void interactive_mode() {
        std::cout << "NoSQL DB Client Interactive Mode" << std::endl;
        std::cout << "Commands: INSERT, FIND, DELETE, QUIT" << std::endl;
        std::cout << "Example: INSERT users {\"name\": \"Alice\", \"age\": 25}" << std::endl;
        std::cout << "Type 'QUIT' to exit" << std::endl;

        while (true) {
            std::cout << "> ";
            std::string command;
            std::getline(std::cin, command);

            if (command.empty()) continue;

            if (command == "QUIT" || command == "quit") {
                break;
            }

            try {
                process_command(command);
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                if (!connected) {
                    std::cerr << "Attempting to recover connection..." << std::endl;
                    if (reconnect()) {
                        std::cout << "Reconnected successfully. You can continue working." << std::endl;
                    } else {
                        std::cerr << "Reconnection failed. Please restart the client." << std::endl;
                        break;
                    }
                }
            }
        }
    }

    void single_command_mode(const std::string& command) {
        try {
            process_command(command);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

private:
    void process_command(const std::string& command) {
        size_t space1 = command.find(' ');
        if (space1 == std::string::npos) {
            std::cerr << "Invalid command format. Use: OPERATION collection_name {json_data}" << std::endl;
            return;
        }

        std::string operation = command.substr(0, space1);
        std::string rest = command.substr(space1 + 1);

        size_t space2 = rest.find(' ');
        if (space2 == std::string::npos) {
            std::cerr << "Invalid command format. Use: OPERATION collection_name {json_data}" << std::endl;
            return;
        }

        std::string collection = rest.substr(0, space2);
        std::string json_str = rest.substr(space2 + 1);

        json request = {
            {"database", database},
            {"operation", to_lower(operation)}
        };

        if (operation == "INSERT") {
            try {
                json doc = json::parse(json_str);
                request["data"] = json::array({doc});
            } catch (const std::exception& e) {
                std::cerr << "Invalid JSON document: " << e.what() << std::endl;
                return;
            }
        } else if (operation == "FIND" || operation == "DELETE") {
            try {
                request["query"] = json::parse(json_str);
            } catch (const std::exception& e) {
                std::cerr << "Invalid JSON query: " << e.what() << std::endl;
                return;
            }
        } else {
            std::cerr << "Unknown operation: " << operation << std::endl;
            std::cerr << "Supported operations: INSERT, FIND, DELETE" << std::endl;
            return;
        }

        json response = send_request(request);
        std::cout << response.dump(2) << std::endl;
    }

    std::string to_lower(const std::string& str) {
        std::string result = str;
        for (char& c : result) {
            c = std::tolower(c);
        }
        return result;
    }
};

int main(int argc, char** argv) {
    if (argc < 7) {
        std::cerr << "Usage: " << argv[0] << " --host <host> --port <port> --database <db_name>" << std::endl;
        std::cerr << "For interactive mode: " << argv[0] << " --host localhost --port 8080 --database my_database" << std::endl;
        std::cerr << "For single command: " << argv[0] << " --host localhost --port 8080 --database my_database --command \"INSERT users {\\\"name\\\": \\\"Alice\\\"}\"" << std::endl;
        return 1;
    }

    std::string host = "localhost";
    int port = 8080;
    std::string database;
    std::string command;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--database" && i + 1 < argc) {
            database = argv[++i];
        } else if (arg == "--command" && i + 1 < argc) {
            command = argv[++i];
        }
    }

    if (database.empty()) {
        std::cerr << "Database name is required" << std::endl;
        return 1;
    }

    DBClient client(host, port, database);
    if (!client.connect()) {
        return 1;
    }

    if (!command.empty()) {
        client.single_command_mode(command);
    } else {
        client.interactive_mode();
    }

    return 0;
}
