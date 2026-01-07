#include "../include/event.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <cstring>

using json = nlohmann::json;

namespace siem {

    SecurityEvent::SecurityEvent(const std::string& source,
                                 const std::string& event_type,
                                 const std::string& severity,
                                 const std::string& raw_log)
    : source(source), event_type(event_type), severity(severity), raw_log(raw_log) {
        init_timestamp();
        hostname = get_current_hostname();
    }

    SecurityEvent::SecurityEvent(const json& j) {
        timestamp = j.value("timestamp", "");
        hostname = j.value("hostname", "");
        source = j.value("source", "");
        event_type = j.value("event_type", "");
        severity = j.value("severity", "");
        user = j.value("user", "");
        process = j.value("process", "");
        command = j.value("command", "");
        raw_log = j.value("raw_log", "");
    }

    void SecurityEvent::init_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
            timestamp = ss.str();
    }

    std::string SecurityEvent::get_current_hostname() const {
        char buffer[256];
        if (gethostname(buffer, sizeof(buffer)) == 0) {
            return std::string(buffer);
        }
        return "unknown-host";
    }

    json SecurityEvent::to_json() const {
        return {
            {"timestamp", timestamp},
            {"hostname", hostname},
            {"source", source},
            {"event_type", event_type},
            {"severity", severity},
            {"user", user},
            {"process", process},
            {"command", command},
            {"raw_log", raw_log}
        };
    }

    json SecurityEvent::to_network_json(const std::string& agent_id) const {
        json event_json = to_json();

        return {
            {"agent_id", agent_id},
            {"timestamp", timestamp},
            {"events", json::array({event_json})}
        };
    }

}
