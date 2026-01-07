#pragma once

#include <string>
#include "../../parcer/json.hpp"

using json = nlohmann::json;

namespace siem {

    class SecurityEvent {
    public:
        SecurityEvent() = default;

        SecurityEvent(const std::string& source, const std::string& event_type,
                      const std::string& severity, const std::string& raw_log);

        SecurityEvent(const json& j);

        json to_json() const;

        json to_network_json(const std::string& agent_id) const;

        const std::string& get_timestamp() const { return timestamp; }
        const std::string& get_hostname() const { return hostname; }
        const std::string& get_source() const { return source; }
        const std::string& get_event_type() const { return event_type; }
        const std::string& get_severity() const { return severity; }
        const std::string& get_user() const { return user; }
        const std::string& get_process() const { return process; }
        const std::string& get_command() const { return command; }
        const std::string& get_raw_log() const { return raw_log; }

        void set_timestamp(const std::string& ts) { timestamp = ts; }
        void set_hostname(const std::string& name) { hostname = name; }
        void set_source(const std::string& src) { source = src; }
        void set_event_type(const std::string& type) { event_type = type; }
        void set_severity(const std::string& sev) { severity = sev; }
        void set_user(const std::string& u) { user = u; }
        void set_process(const std::string& p) { process = p; }
        void set_command(const std::string& cmd) { command = cmd; }
        void set_raw_log(const std::string& log) { raw_log = log; }

    private:
        void init_timestamp();
        std::string get_current_hostname() const;

        std::string timestamp;
        std::string hostname;
        std::string source;
        std::string event_type;
        std::string severity;
        std::string user;
        std::string process;
        std::string command;
        std::string raw_log;
    };

}
