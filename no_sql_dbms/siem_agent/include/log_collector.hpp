#pragma once

#include "position_manager.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <sys/inotify.h>

namespace siem {
    class SecurityEvent;
    class Config;
    class EventBuffer;
    struct LogSource;
    class PositionManager;
    struct FilePosition;
}

namespace siem {

// ========== Парсеры ==========

class AuditdParser {
public:
    static SecurityEvent parse_line(const std::string& line);

private:
    static std::string extract_audit_field(const std::string& line, const std::string& field);
    static std::string determine_audit_event_type(const std::string& msg);
    static std::string determine_audit_severity(const std::string& event_type);
};

class SyslogParser {
public:
    static SecurityEvent parse_line(const std::string& line);

private:
    static bool is_security_event(const std::string& line);
    static std::string extract_syslog_field(const std::string& line, const std::string& field);
};

class BashHistoryParser {
public:
    static SecurityEvent parse_line(const std::string& line,
                                    const std::string& username,
                                    const std::string& hostname);
};

// ========== Сборщик логов ==========

class LogCollector {
public:
    LogCollector(EventBuffer& buffer, const Config& config);
    ~LogCollector();

    void start();
    void stop();

    bool is_running() const { return running; }

private:
    void run();

    void initialize_inotify();
    void add_inotify_watch(const std::string& path);
    void monitor_inotify_events();

    void poll_for_changes();
    void check_file_for_changes(const LogSource& source);

    void handle_file_modification(const std::string& path);
    void handle_file_rotation(const std::string& path);

    void initial_scan();
    void process_source(const LogSource& source);
    void process_log_file(const LogSource& source,
                         const std::string& path,
                         const std::string& username = "");

    EventBuffer& buffer_ref;
    const Config& config_ref;

    std::thread collector_thread;
    std::atomic<bool> running{false};

    PositionManager position_manager;

    int inotify_fd;
    std::unordered_map<int, std::string> watch_descriptors;
};

}
