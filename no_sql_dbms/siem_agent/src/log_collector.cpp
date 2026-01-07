#include "../include/log_collector.hpp"
#include "../include/event.hpp"
#include "../include/event_buffer.hpp"
#include "../include/config.hpp"
#include <iostream>
#include <fstream>
#include <regex>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <pwd.h>
#include <chrono>
#include <fcntl.h>
#include <poll.h>

using json = nlohmann::json;

namespace siem {

namespace fs = std::filesystem;

// ========== AuditdParser ==========

SecurityEvent AuditdParser::parse_line(const std::string& line) {
    SecurityEvent event("auditd", "", "", line);

    std::string msg = extract_audit_field(line, "msg");
    std::string uid = extract_audit_field(line, "uid");
    std::string auid = extract_audit_field(line, "auid");
    std::string pid = extract_audit_field(line, "pid");
    std::string exe = extract_audit_field(line, "exe");
    std::string comm = extract_audit_field(line, "comm");
    std::string a0 = extract_audit_field(line, "a0");
    std::string a1 = extract_audit_field(line, "a1");

    std::string event_type = determine_audit_event_type(msg);
    std::string severity = determine_audit_severity(event_type);

    event.set_event_type(event_type);
    event.set_severity(severity);

    if (!auid.empty() && auid != "-1") {
        try {
            uid_t user_id = std::stoi(auid);
            struct passwd* pw = getpwuid(user_id);
            if (pw) {
                event.set_user(pw->pw_name);
            }
        } catch (...) {
            event.set_user("auid:" + auid);
        }
    }

    if (event.get_user().empty() && !uid.empty() && uid != "-1") {
        try {
            uid_t user_id = std::stoi(uid);
            struct passwd* pw = getpwuid(user_id);
            if (pw) {
                event.set_user(pw->pw_name);
            }
        } catch (...) {
            event.set_user("uid:" + uid);
        }
    }

    if (!comm.empty()) {
        event.set_process(comm);
    } else if (!exe.empty()) {
        event.set_process(fs::path(exe).filename().string());
    }

    if (!a0.empty() && a0.find("/") != std::string::npos) {
        event.set_command(a0);
    } else if (!a1.empty() && a1.find("/") != std::string::npos) {
        event.set_command(a1);
    }

    return event;
}

std::string AuditdParser::extract_audit_field(const std::string& line, const std::string& field) {
    std::regex pattern(field + "=([^\\s\"]+|\"[^\"]*\")");
    std::smatch match;

    if (std::regex_search(line, match, pattern) && match.size() > 1) {
        std::string value = match[1].str();
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        return value;
    }

    return "";
}

std::string AuditdParser::determine_audit_event_type(const std::string& msg) {
    std::string lower_msg = msg;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

    if (lower_msg.find("user_login") != std::string::npos) return "user_login";
    if (lower_msg.find("user_logout") != std::string::npos) return "user_logout";
    if (lower_msg.find("user_auth") != std::string::npos) return "user_auth";
    if (lower_msg.find("cred_acq") != std::string::npos) return "credential_acquire";
    if (lower_msg.find("execve") != std::string::npos) return "command_execution";
    if (lower_msg.find("syscall") != std::string::npos) return "syscall";
    if (lower_msg.find("path") != std::string::npos) return "file_access";
    if (lower_msg.find("config_change") != std::string::npos) return "config_change";
    if (lower_msg.find("service_start") != std::string::npos) return "service_start";
    if (lower_msg.find("service_stop") != std::string::npos) return "service_stop";

    return "audit_event";
}

std::string AuditdParser::determine_audit_severity(const std::string& event_type) {
    if (event_type == "user_login" || event_type == "user_logout") return "low";
    if (event_type == "user_auth" || event_type == "credential_acquire") return "medium";
    if (event_type == "command_execution" || event_type == "config_change" ||
        event_type == "service_start" || event_type == "service_stop") return "high";

    return "info";
}

// ========== SyslogParser ==========

SecurityEvent SyslogParser::parse_line(const std::string& line) {
    SecurityEvent event("syslog", "", "", line);

    if (!is_security_event(line)) {
        event.set_event_type("system_log");
        event.set_severity("info");
        return event;
    }

    std::string process = extract_syslog_field(line, "process");

    std::string event_type = "system_event";
    std::string severity = "info";

    std::string lower_line = line;
    std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);

    if (lower_line.find("fail") != std::string::npos ||
        lower_line.find("error") != std::string::npos ||
        lower_line.find("denied") != std::string::npos ||
        lower_line.find("invalid") != std::string::npos ||
        lower_line.find("refused") != std::string::npos) {
        severity = "high";
        event_type = "auth_failure";
    }

    if (lower_line.find("sudo") != std::string::npos) {
        event_type = "sudo_command";
        severity = "medium";

        std::regex user_pattern(R"(sudo:\s+(\w+)\s+)");
        std::smatch match;
        if (std::regex_search(line, match, user_pattern) && match.size() > 1) {
            event.set_user(match[1].str());
        }

        std::regex cmd_pattern(R"(COMMAND=(\/.+))");
        if (std::regex_search(line, match, cmd_pattern) && match.size() > 1) {
            event.set_command(match[1].str());
        }
    }

    if (lower_line.find("sshd") != std::string::npos) {
        event_type = "ssh_event";
        event.set_process("sshd");

        if (lower_line.find("accepted") != std::string::npos) {
            severity = "medium";
            event_type = "ssh_login";

            std::regex ssh_pattern(R"(Accepted\s+\w+\s+for\s+(\w+))");
            std::smatch match;
            if (std::regex_search(line, match, ssh_pattern) && match.size() > 1) {
                event.set_user(match[1].str());
            }
        } else if (lower_line.find("failed") != std::string::npos) {
            severity = "high";
            event_type = "ssh_failed_login";

            std::regex fail_pattern(R"(Failed\s+\w+\s+for\s+(\w+))");
            std::smatch match;
            if (std::regex_search(line, match, fail_pattern) && match.size() > 1) {
                event.set_user(match[1].str());
            }
        } else if (lower_line.find("disconnect") != std::string::npos) {
            severity = "low";
            event_type = "ssh_disconnect";
        }
    }

    if (lower_line.find("cron") != std::string::npos) {
        event_type = "cron_job";
        event.set_process("cron");
    }

    if (lower_line.find("kernel") != std::string::npos) {
        event_type = "kernel_event";
        severity = "medium";
    }

    event.set_event_type(event_type);
    event.set_severity(severity);

    if (!process.empty()) {
        event.set_process(process);
    }

    return event;
}

bool SyslogParser::is_security_event(const std::string& line) {
    std::string lower_line = line;
    std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);

    return (lower_line.find("sudo") != std::string::npos ||
            lower_line.find("sshd") != std::string::npos ||
            lower_line.find("auth") != std::string::npos ||
            lower_line.find("login") != std::string::npos ||
            lower_line.find("failed") != std::string::npos ||
            lower_line.find("password") != std::string::npos ||
            lower_line.find("cron") != std::string::npos ||
            lower_line.find("kernel") != std::string::npos ||
            lower_line.find("session") != std::string::npos);
}

std::string SyslogParser::extract_syslog_field(const std::string& line, const std::string& field) {
    if (field == "process") {
        std::regex pattern(R"((\w+)\[\d+\]:)");
        std::smatch match;
        if (std::regex_search(line, match, pattern) && match.size() > 1) {
            return match[1].str();
        }

        std::regex pattern2(R"((\w+):\s)");
        if (std::regex_search(line, match, pattern2) && match.size() > 1) {
            return match[1].str();
        }
    }

    return "";
}

// ========== BashHistoryParser ==========

SecurityEvent BashHistoryParser::parse_line(const std::string& line,
                                           const std::string& username,
                                           const std::string& hostname) {
    SecurityEvent event("bash_history", "command_execution", "medium", line);

    event.set_user(username);
    event.set_process("bash");
    event.set_command(line);

    std::string lower_line = line;
    std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);

    if (lower_line.find("sudo") == 0 ||
        lower_line.find("su ") == 0 ||
        lower_line.find("passwd") != std::string::npos ||
        lower_line.find("chmod") != std::string::npos ||
        lower_line.find("chown") != std::string::npos ||
        lower_line.find("rm ") != std::string::npos ||
        lower_line.find("dd ") != std::string::npos ||
        lower_line.find("mkfs") != std::string::npos ||
        lower_line.find("fdisk") != std::string::npos ||
        lower_line.find("iptables") != std::string::npos ||
        lower_line.find("service") == 0 ||
        lower_line.find("systemctl") == 0) {
        event.set_severity("high");
    } else if (lower_line.find("ssh ") != std::string::npos ||
               lower_line.find("scp ") != std::string::npos ||
               lower_line.find("wget") != std::string::npos ||
               lower_line.find("curl") != std::string::npos ||
               lower_line.find("netcat") != std::string::npos ||
               lower_line.find("nc ") != std::string::npos) {
        event.set_severity("medium");
    }

    return event;
}

// ========== LogCollector ==========

LogCollector::LogCollector(EventBuffer& buffer, const Config& config)
    : buffer_ref(buffer), config_ref(config),
      position_manager(config.get_position_file()),
      inotify_fd(-1) {

    position_manager.load_positions();
}

LogCollector::~LogCollector() {
    stop();

    if (inotify_fd >= 0) {
        for (const auto& [wd, path] : watch_descriptors) {
            inotify_rm_watch(inotify_fd, wd);
        }
        close(inotify_fd);
    }
}

void LogCollector::start() {
    if (running) return;

    running = true;

    initialize_inotify();

    collector_thread = std::thread(&LogCollector::run, this);

    std::cout << "[INFO] LogCollector started with inotify monitoring" << std::endl;
}

void LogCollector::stop() {
    if (!running) return;

    running = false;

    if (collector_thread.joinable()) {
        collector_thread.join();
    }

    position_manager.save_positions();

    std::cout << "[INFO] LogCollector stopped" << std::endl;
}

void LogCollector::initialize_inotify() {
    inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd < 0) {
        std::cerr << "[ERROR] Failed to initialize inotify: " << strerror(errno) << std::endl;
        return;
    }

    for (const auto& source : config_ref.get_sources()) {
        if (!source.enabled) continue;

        if (source.name == "bash_history") {
            for (const auto& username : source.users) {
                std::string history_file = "/home/" + username + "/.bash_history";
                add_inotify_watch(history_file);
            }
        } else {
            add_inotify_watch(source.path);
        }
    }
}

void LogCollector::add_inotify_watch(const std::string& path) {
    if (!fs::exists(path)) {
        std::cout << "[WARN] File does not exist, cannot watch: " << path << std::endl;
        return;
    }

    int wd = inotify_add_watch(inotify_fd, path.c_str(),
                               IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_CREATE);
    if (wd < 0) {
        std::cerr << "[ERROR] Failed to add watch for " << path << ": " << strerror(errno) << std::endl;
        return;
    }

    watch_descriptors[wd] = path;
    std::cout << "[INFO] Watching file: " << path << " (wd=" << wd << ")" << std::endl;
}

void LogCollector::run() {
    std::cout << "[INFO] Starting log collection..." << std::endl;

    initial_scan();

    while (running) {
        if (inotify_fd >= 0) {
            monitor_inotify_events();
        } else {
            poll_for_changes();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    position_manager.save_positions();
}

void LogCollector::monitor_inotify_events() {
    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(inotify_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int ready = select(inotify_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (ready <= 0) {
        return;
    }

    ssize_t len = read(inotify_fd, buffer, sizeof(buffer));
    if (len <= 0) {
        return;
    }

    char* ptr = buffer;
    while (ptr < buffer + len) {
        struct inotify_event* event = reinterpret_cast<struct inotify_event*>(ptr);

        auto it = watch_descriptors.find(event->wd);
        if (it != watch_descriptors.end()) {
            std::string path = it->second;

            if (event->mask & IN_MODIFY) {
                handle_file_modification(path);
            } else if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                handle_file_rotation(path);
            }
        }

        ptr += sizeof(struct inotify_event) + event->len;
    }
}

void LogCollector::poll_for_changes() {
    for (const auto& source : config_ref.get_sources()) {
        if (!source.enabled) continue;

        check_file_for_changes(source);
    }
}

void LogCollector::check_file_for_changes(const LogSource& source) {
    static std::unordered_map<std::string, time_t> last_check_times;

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    auto last_check = last_check_times[source.name];
    if (now_time_t - last_check < 1) {
        return;
    }

    last_check_times[source.name] = now_time_t;

    if (source.name == "bash_history") {
        for (const auto& username : source.users) {
            std::string history_file = "/home/" + username + "/.bash_history";
            if (fs::exists(history_file)) {
                struct stat st;
                if (stat(history_file.c_str(), &st) == 0) {
                    FilePosition pos = position_manager.get_position(history_file);

                    if (st.st_mtime > pos.last_modification ||
                        std::to_string(st.st_ino) != pos.inode) {
                        process_log_file(source, history_file, username);
                    }
                }
            }
        }
    } else if (fs::exists(source.path)) {
        struct stat st;
        if (stat(source.path.c_str(), &st) == 0) {
            FilePosition pos = position_manager.get_position(source.path);

            if (st.st_mtime > pos.last_modification ||
                std::to_string(st.st_ino) != pos.inode) {
                process_log_file(source, source.path);
            }
        }
    }
}

void LogCollector::handle_file_modification(const std::string& path) {
    for (const auto& source : config_ref.get_sources()) {
        if (!source.enabled) continue;

        if (source.path == path) {
            process_log_file(source, path);
            return;
        }

        if (source.name == "bash_history") {
            for (const auto& username : source.users) {
                std::string history_file = "/home/" + username + "/.bash_history";
                if (history_file == path) {
                    process_log_file(source, path, username);
                    return;
                }
            }
        }
    }
}

void LogCollector::handle_file_rotation(const std::string& path) {
    std::cout << "[INFO] File rotation detected: " << path << std::endl;

    for (auto it = watch_descriptors.begin(); it != watch_descriptors.end(); ) {
        if (it->second == path) {
            inotify_rm_watch(inotify_fd, it->first);
            it = watch_descriptors.erase(it);
        } else {
            ++it;
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (fs::exists(path)) {
        add_inotify_watch(path);
    }

    position_manager.remove_position(path);

    for (const auto& source : config_ref.get_sources()) {
        if (!source.enabled) continue;

        if (source.path == path) {
            process_log_file(source, path);
            return;
        }
    }
}

void LogCollector::initial_scan() {
    std::cout << "[INFO] Performing initial scan of log files..." << std::endl;

    for (const auto& source : config_ref.get_sources()) {
        if (!source.enabled) continue;

        try {
            process_source(source);
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to process source " << source.name
                     << ": " << e.what() << std::endl;
        }
    }
}

void LogCollector::process_source(const LogSource& source) {
    if (source.name == "bash_history") {
        for (const auto& username : source.users) {
            std::string history_file = "/home/" + username + "/.bash_history";
            if (fs::exists(history_file)) {
                process_log_file(source, history_file, username);
            } else {
                std::cout << "[INFO] Bash history file not found for user "
                         << username << ": " << history_file << std::endl;
            }
        }
    } else {
        if (fs::exists(source.path)) {
            process_log_file(source, source.path);
        } else {
            std::cout << "[WARN] Log file does not exist: " << source.path << std::endl;
        }
    }
}

void LogCollector::process_log_file(const LogSource& source,
                                   const std::string& path,
                                   const std::string& username) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        std::cerr << "[ERROR] Cannot stat file: " << path << std::endl;
        return;
    }

    FilePosition pos = position_manager.get_position(path);
    std::string current_inode = std::to_string(st.st_ino);

    std::cout << "[DEBUG] Processing " << path
              << ", inode: " << current_inode
              << ", saved inode: " << (pos.inode.empty() ? "(none)" : pos.inode)
              << ", last pos: " << pos.last_position
              << ", file size: " << st.st_size << std::endl;

    if (pos.inode.empty() || current_inode != pos.inode) {
        if (pos.inode.empty()) {
            std::cout << "[INFO] First time seeing file: " << path << std::endl;
        } else {
            std::cout << "[INFO] File rotation detected (inode changed): " << path
                      << " (" << pos.inode << " -> " << current_inode << ")" << std::endl;
        }
        pos.last_position = 0;
        pos.inode = current_inode;
    }

    if (st.st_size < pos.last_position) {
        std::cout << "[INFO] File truncated: " << path << std::endl;
        pos.last_position = 0;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Cannot open file: " << path << std::endl;
        return;
    }

    if (pos.last_position > 0) {
        file.seekg(pos.last_position);
        if (file.fail()) {
            std::cerr << "[WARN] Failed to seek to position " << pos.last_position
                     << " in file " << path << ", reading from beginning" << std::endl;
            file.clear();
            file.seekg(0);
            pos.last_position = 0;
        }
    }

    std::string line;
    size_t lines_read = 0;

    while (std::getline(file, line) && running) {
        if (line.empty()) continue;

        line.erase(std::remove_if(line.begin(), line.end(),
                                  [](char c) { return c == '\0' || (c >= 0 && c < 32 && c != '\t' && c != '\n' && c != '\r'); }),
                   line.end());

        if (line.empty()) continue;

        SecurityEvent event;

        try {
            if (source.name == "auditd") {
                event = AuditdParser::parse_line(line);
            } else if (source.name == "syslog" || source.name == "auth") {
                event = SyslogParser::parse_line(line);
            } else if (source.name == "bash_history") {
                event = BashHistoryParser::parse_line(line, username, event.get_hostname());
            } else {
                event = SecurityEvent(source.name, "log_entry", "info", line);
            }

            buffer_ref.add_event(event);
            lines_read++;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to parse log line: " << e.what()
                     << "\nLine: " << line << std::endl;
        }
    }

    std::streampos current_pos = file.tellg();

    if (current_pos == std::streampos(-1)) {
        current_pos = st.st_size;
        std::cout << "[DEBUG] tellg() returned -1, using file size as position: "
                  << current_pos << std::endl;
    }

    pos.last_position = current_pos;
    pos.inode = current_inode;
    pos.last_modification = st.st_mtime;

    position_manager.update_position(path, pos.inode, pos.last_position, pos.last_modification);

    if (lines_read > 0) {
        std::cout << "[INFO] Read " << lines_read << " new lines from " << path
                 << ", new position: " << pos.last_position << std::endl;
    } else {
        std::cout << "[DEBUG] No new lines in " << path
                 << ", position: " << pos.last_position << std::endl;
    }

    file.close();
}

}
