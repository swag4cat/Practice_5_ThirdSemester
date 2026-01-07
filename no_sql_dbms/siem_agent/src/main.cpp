#include "../include/agent.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "               SIEM Agent               " << std::endl;
    std::cout << "========================================" << std::endl;

    std::string config_path = "configs/agent_config.json";
    bool run_as_daemon = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--daemon") {
            run_as_daemon = true;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --config <path>   Path to configuration file" << std::endl;
            std::cout << "  --daemon          Run as daemon" << std::endl;
            std::cout << "  --help            Show this help" << std::endl;
            return 0;
        }
    }

    try {
        siem::SIEMAgent agent;

        if (!agent.init(config_path)) {
            std::cerr << "[ERROR] Failed to initialize agent" << std::endl;
            return 1;
        }

        if (run_as_daemon) {
            std::cout << "Running as daemon..." << std::endl;
            agent.daemonize();

        }

        agent.run();

        std::cout << "SIEM Agent finished" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
}
