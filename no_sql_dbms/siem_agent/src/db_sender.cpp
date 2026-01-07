#include "../include/db_sender.hpp"
#include "../include/event.hpp"
#include "../include/event_buffer.hpp"
#include <iostream>
#include <chrono>
#include <cstring>
#include <unistd.h>

using json = nlohmann::json;

namespace siem {

    DBSender::DBSender(const Config& config, EventBuffer& buffer)
    : config_ref(config), buffer_ref(buffer) {

        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config.get_port());

        if (inet_pton(AF_INET, config.get_host().c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "[ERROR] Invalid server address: " << config.get_host() << std::endl;
        }
    }

    DBSender::~DBSender() {
        stop();
        disconnect();
    }

    void DBSender::start() {
        if (running) return;

        running = true;
        sender_thread = std::thread(&DBSender::run, this);

        std::cout << "[INFO] DBSender started" << std::endl;
    }

    void DBSender::stop() {
        if (!running) return;

        running = false;
        cv.notify_all();

        if (sender_thread.joinable()) {
            sender_thread.join();
        }

        std::cout << "[INFO] DBSender stopped" << std::endl;
    }

    void DBSender::run() {
        while (running) {
            Vector<SecurityEvent> batch = buffer_ref.get_batch(config_ref.get_batch_size());

            if (!batch.empty()) {
                std::cout << "[INFO] Sending batch of " << batch.size() << " events to collection 'security_events'" << std::endl;

                json request = {
                    {"database", "security_events"},
                    {"operation", "insert"},
                    {"data", json::array()}
                };

                for (const auto& event : batch) {
                    request["data"].push_back(event.to_json());
                }

                std::cout << "[DEBUG] Request: " << request.dump().substr(0, 200) << std::endl;

                bool sent = false;
                for (int attempt = 0; attempt < config_ref.get_max_retries() && !sent; ++attempt) {
                    if (attempt > 0) {
                        std::cout << "[WARN] Retry attempt " << attempt << " for sending batch" << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(config_ref.get_retry_delay()));
                    }

                    sent = send_json(request);

                    if (sent) {
                        std::cout << "[INFO] Batch sent successfully" << std::endl;
                    }
                }

                if (!sent) {
                    std::cerr << "[ERROR] Failed to send batch after "
                    << config_ref.get_max_retries() << " attempts" << std::endl;
                }
            }

            std::unique_lock<std::mutex> lock(socket_mutex);
            cv.wait_for(lock, std::chrono::seconds(config_ref.get_send_interval()),
                        [this]() { return !running; });
        }
    }

    bool DBSender::connect_to_server() {
        std::lock_guard<std::mutex> lock(socket_mutex);

        if (sock_fd >= 0) {
            disconnect();
        }

        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            std::cerr << "[ERROR] Cannot create socket" << std::endl;
            return false;
        }

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "[ERROR] Cannot connect to server "
            << config_ref.get_host() << ":" << config_ref.get_port() << std::endl;
            close(sock_fd);
            sock_fd = -1;
            return false;
        }

        std::cout << "[INFO] Connected to server "
        << config_ref.get_host() << ":" << config_ref.get_port() << std::endl;

        return true;
    }

    void DBSender::disconnect() {
        std::lock_guard<std::mutex> lock(socket_mutex);

        if (sock_fd >= 0) {
            close(sock_fd);
            sock_fd = -1;
            std::cout << "[INFO] Disconnected from server" << std::endl;
        }
    }

    bool DBSender::send_json(const json& j) {
        if (sock_fd < 0 && !connect_to_server()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(socket_mutex);

        try {
            std::string json_str = j.dump() + "\n";

            std::cout << "[DEBUG] Sending JSON (" << json_str.length() << " bytes)..." << std::endl;

            ssize_t bytes_sent = send(sock_fd, json_str.c_str(), json_str.length(), 0);

            if (bytes_sent <= 0) {
                std::cerr << "[ERROR] Send failed, bytes sent: " << bytes_sent << std::endl;
                disconnect();
                return false;
            }

            std::cout << "[DEBUG] Sent " << bytes_sent << " bytes, waiting for response..." << std::endl;

            char buffer[4096] = {0};
            ssize_t bytes_received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0) {
                std::cout << "[DEBUG] Received response (" << bytes_received << " bytes): "
                << std::string(buffer, bytes_received) << std::endl;

                try {
                    json response = json::parse(std::string(buffer, bytes_received));

                    if (response.contains("status")) {
                        std::string status = response["status"].get<std::string>();
                        if (status == "success") {
                            return true;
                        } else {
                            std::cerr << "[ERROR] Server returned error: "
                            << response.dump() << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Failed to parse response: " << e.what()
                    << "\nResponse: " << std::string(buffer, bytes_received) << std::endl;
                }
            } else if (bytes_received == 0) {
                std::cerr << "[ERROR] Server closed connection" << std::endl;
                disconnect();
            } else {
                std::cerr << "[ERROR] Receive failed, errno: " << errno << std::endl;
                disconnect();
            }

            return false;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to send JSON: " << e.what() << std::endl;
            disconnect();
            return false;
        }
    }

    bool DBSender::send_immediately(const Vector<SecurityEvent>& events) {
        if (events.empty()) return true;

        json request = {
            {"database", "security_events"},
            {"operation", "insert"},
            {"data", json::array()}
        };

        for (const auto& event : events) {
            request["data"].push_back(event.to_json());
        }

        return send_json(request);
    }

    bool DBSender::is_connected() const {
        return sock_fd >= 0;
    }

}
