#include "../include/agent.hpp"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <filesystem>

namespace siem {

    namespace fs = std::filesystem;

    std::atomic<SIEMAgent*> SIEMAgent::instance_ptr{nullptr};

    SIEMAgent::SIEMAgent() {
        instance_ptr = this;
    }

    SIEMAgent::~SIEMAgent() {
        stop();
        instance_ptr = nullptr;
    }

    bool SIEMAgent::init(const std::string& config_path) {
        try {
            std::cout << "Initializing SIEM Agent..." << std::endl;

            config = std::make_unique<Config>();
            if (!config->load(config_path)) {
                std::cerr << "[ERROR] Failed to load configuration from: "
                << config_path << std::endl;
                return false;
            }

            if (!setup_directories()) {
                return false;
            }

            buffer = std::make_unique<EventBuffer>(
                config->get_max_memory_events(),
                                                   config->get_disk_backup(),
                                                   config->get_disk_path()
            );

            collector = std::make_unique<LogCollector>(*buffer, *config);
            sender = std::make_unique<DBSender>(*config, *buffer);

            std::cout << "SIEM Agent initialized successfully" << std::endl;
            std::cout << "  Agent ID: " << config->get_agent_id() << std::endl;
            std::cout << "  Server: " << config->get_host() << ":"
            << config->get_port() << std::endl;
            std::cout << "  Sources: " << config->get_sources().size() << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to initialize SIEM Agent: "
            << e.what() << std::endl;
            return false;
        }
    }

    void SIEMAgent::run() {
        if (running) {
            std::cerr << "[ERROR] Agent is already running" << std::endl;
            return;
        }

        running = true;

        std::cout << "Starting SIEM Agent..." << std::endl;

        setup_signal_handlers();

        try {
            collector->start();
            sender->start();

            std::cout << "SIEM Agent started successfully" << std::endl;
            std::cout << "Press Ctrl+C to stop..." << std::endl;

            while (running && !stopping) {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                static time_t last_stats_time = 0;
                time_t now = time(nullptr);

                if (now - last_stats_time >= 60) {
                    std::cout << "[STATS] Buffer size: " << buffer->size()
                    << " events" << std::endl;
                    last_stats_time = now;
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Error in agent main loop: "
            << e.what() << std::endl;
        }

        cleanup();
    }

    void SIEMAgent::stop() {
        if (!running) return;

        std::cout << "Stopping SIEM Agent..." << std::endl;
        stopping = true;

        if (collector) {
            collector->stop();
        }

        if (sender) {
            sender->stop();
        }

        if (buffer) {
            buffer->save_to_disk();
        }

        running = false;
        stopping = false;

        std::cout << "SIEM Agent stopped" << std::endl;
    }

    void SIEMAgent::cleanup() {
        std::cout << "Cleaning up SIEM Agent..." << std::endl;
        stop();
        std::cout << "Cleanup completed" << std::endl;
    }

    void SIEMAgent::daemonize() {
        std::cout << "Daemonizing SIEM Agent..." << std::endl;

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "[ERROR] First fork failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        if (setsid() < 0) {
            std::cerr << "[ERROR] Failed to create new session" << std::endl;
            exit(EXIT_FAILURE);
        }

        pid = fork();
        if (pid < 0) {
            std::cerr << "[ERROR] Second fork failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        if (chdir("/") < 0) {
            std::cerr << "[ERROR] Failed to change directory to /" << std::endl;
            exit(EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null < 0) {
            exit(EXIT_FAILURE);
        }

        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);

        std::cout << "[INFO] SIEM Agent running as daemon (PID: " << getpid() << ")" << std::endl;
    }

    bool SIEMAgent::setup_directories() {
        try {
            if (config->get_disk_backup()) {
                std::string local_buffer = "./siem_agent/buffer";
                fs::create_directories(local_buffer);
                std::cout << "[INFO] Created buffer directory: "
                << local_buffer << std::endl;
            }

            fs::path config_dir = "siem_agent/configs";
            if (!fs::exists(config_dir)) {
                fs::create_directories(config_dir);
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to create directories: "
            << e.what() << std::endl;
            return false;
        }
    }

    void SIEMAgent::setup_signal_handlers() {
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGHUP, &sa, nullptr);
    }

    void SIEMAgent::signal_handler(int signal) {
        SIEMAgent* instance = instance_ptr.load();

        if (instance) {
            std::cout << "\n[INFO] Received signal " << signal
            << ", shutting down..." << std::endl;
            instance->stop();
        }
    }

}
