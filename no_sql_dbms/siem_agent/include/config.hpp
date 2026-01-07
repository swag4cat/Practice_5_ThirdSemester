#pragma once

#include <string>
#include <vector>
#include "../../parcer/json.hpp"

using json = nlohmann::json;

namespace siem {

    struct LogSource {
        std::string name;
        bool enabled;
        std::string path;
        std::vector<std::string> users;
    };

    class Config {
    public:
        Config() = default;

        bool load(const std::string& config_path);

        bool load_from_json(const json& j);

        bool save(const std::string& config_path) const;

        int get_poll_interval() const { return poll_interval; }

        const std::string& get_host() const { return host; }
        int get_port() const { return port; }
        const std::string& get_agent_id() const { return agent_id; }
        const std::vector<LogSource>& get_sources() const { return sources; }
        int get_batch_size() const { return batch_size; }
        int get_send_interval() const { return send_interval; }
        int get_max_retries() const { return max_retries; }
        int get_retry_delay() const { return retry_delay; }
        int get_max_memory_events() const { return max_memory_events; }
        bool get_disk_backup() const { return disk_backup; }
        const std::string& get_disk_path() const { return disk_path; }
        bool get_check_rotation() const { return check_rotation; }
        bool get_save_position() const { return save_position; }
        const std::string& get_position_file() const { return position_file; }

    private:
        std::string host = "127.0.0.1";
        int port = 8080;
        std::string agent_id = "agent-ubuntu-01";
        std::vector<LogSource> sources;

        int batch_size = 100;
        int send_interval = 30;
        int max_retries = 3;
        int retry_delay = 5;

        int max_memory_events = 1000;
        bool disk_backup = true;
        std::string disk_path = "/tmp/siem_buffer";

        int poll_interval = 1;
        bool check_rotation = true;
        bool save_position = true;
        std::string position_file = "/var/lib/siem-agent/positions.json";
    };

}
