#pragma once

#include "config.hpp"
#include "event_buffer.hpp"
#include "log_collector.hpp"
#include "db_sender.hpp"
#include <atomic>
#include <memory>
#include <csignal>

namespace siem {

    class SIEMAgent {
    public:
        SIEMAgent();
        ~SIEMAgent();

        bool init(const std::string& config_path = "configs/agent_config.json");
        void run();
        void stop();

        bool is_running() const { return running; }

        void daemonize();

        static void setup_signal_handlers();

    private:
        void cleanup();
        bool setup_directories();

        std::unique_ptr<Config> config;
        std::unique_ptr<EventBuffer> buffer;
        std::unique_ptr<LogCollector> collector;
        std::unique_ptr<DBSender> sender;

        std::atomic<bool> running{false};
        std::atomic<bool> stopping{false};

        static std::atomic<SIEMAgent*> instance_ptr;
        static void signal_handler(int signal);
    };

}
