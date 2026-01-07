#pragma once

#include "event.hpp"
#include "config.hpp"
#include "../../include/vector.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace siem {
    class Config;
    class EventBuffer;
    class SecurityEvent;
}

namespace siem {

    class DBSender {
    public:
        DBSender(const Config& config, EventBuffer& buffer);
        ~DBSender();

        void start();

        void stop();

        bool send_immediately(const Vector<SecurityEvent>& events);

        bool is_connected() const;

    private:
        void run();
        bool connect_to_server();
        void disconnect();
        bool send_json(const json& j);
        bool receive_response();

        const Config& config_ref;
        EventBuffer& buffer_ref;

        std::thread sender_thread;
        std::atomic<bool> running{false};

        int sock_fd = -1;
        struct sockaddr_in server_addr;

        mutable std::mutex socket_mutex;
        std::condition_variable cv;
    };

}
