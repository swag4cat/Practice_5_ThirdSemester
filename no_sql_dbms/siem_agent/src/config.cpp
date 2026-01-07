#include "../include/config.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace siem {

    bool Config::load(const std::string& config_path) {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Cannot open config file: " << config_path << std::endl;
            return false;
        }

        try {
            json j;
            file >> j;
            return load_from_json(j);
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to parse config: " << e.what() << std::endl;
            return false;
        }
    }

    bool Config::load_from_json(const json& j) {
        try {
            if (j.contains("server")) {
                host = j["server"].value("host", "127.0.0.1");
                port = j["server"].value("port", 8080);
            }

            if (j.contains("agent")) {
                agent_id = j["agent"].value("id", "agent-ubuntu-01");
            }

            if (j.contains("sources") && j["sources"].is_array()) {
                sources.clear();
                for (const auto& source_json : j["sources"]) {
                    LogSource source;
                    source.name = source_json.value("name", "");
                    source.enabled = source_json.value("enabled", true);
                    source.path = source_json.value("path", "");

                    if (source.path.empty()) {
                        source.path = source_json.value("path_pattern", "");
                    }

                    if (source_json.contains("users") && source_json["users"].is_array()) {
                        for (const auto& user : source_json["users"]) {
                            source.users.push_back(user.get<std::string>());
                        }
                    }

                    sources.push_back(source);
                }
            }

            if (j.contains("sender")) {
                batch_size = j["sender"].value("batch_size", 100);
                send_interval = j["sender"].value("send_interval", 30);
                max_retries = j["sender"].value("max_retries", 3);
                retry_delay = j["sender"].value("retry_delay", 5);
            }

            if (j.contains("buffer")) {
                max_memory_events = j["buffer"].value("max_memory_events", 1000);
                disk_backup = j["buffer"].value("disk_backup", true);
                disk_path = j["buffer"].value("disk_path", "/tmp/siem_buffer");
            }

            if (j.contains("monitoring")) {
                poll_interval = j["monitoring"].value("poll_interval", 1);
                check_rotation = j["monitoring"].value("check_rotation", true);
                save_position = j["monitoring"].value("save_position", true);
                position_file = j.value("position_file", "/var/lib/siem-agent/positions.json");
            }

            std::cout << "[INFO] Config loaded successfully" << std::endl;
            std::cout << "  Agent ID: " << agent_id << std::endl;
            std::cout << "  Server: " << host << ":" << port << std::endl;
            std::cout << "  Sources: " << sources.size() << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to load config from JSON: " << e.what() << std::endl;
            return false;
        }
    }

    bool Config::save(const std::string& config_path) const {
        try {
            json j;

            j["server"]["host"] = host;
            j["server"]["port"] = port;

            j["agent"]["id"] = agent_id;

            j["sources"] = json::array();
            for (const auto& source : sources) {
                json source_json;
                source_json["name"] = source.name;
                source_json["enabled"] = source.enabled;
                source_json["path"] = source.path;
                if (!source.users.empty()) {
                    source_json["users"] = source.users;
                }
                j["sources"].push_back(source_json);
            }

            j["sender"]["batch_size"] = batch_size;
            j["sender"]["send_interval"] = send_interval;
            j["sender"]["max_retries"] = max_retries;
            j["sender"]["retry_delay"] = retry_delay;

            j["buffer"]["max_memory_events"] = max_memory_events;
            j["buffer"]["disk_backup"] = disk_backup;
            j["buffer"]["disk_path"] = disk_path;

            std::ofstream file(config_path);
            if (!file.is_open()) {
                std::cerr << "[ERROR] Cannot open file for writing: " << config_path << std::endl;
                return false;
            }

            file << j.dump(2);
            file.close();

            std::cout << "[INFO] Config saved to: " << config_path << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to save config: " << e.what() << std::endl;
            return false;
        }
    }

}
