#include "../include/event_buffer.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>

namespace siem {

    namespace fs = std::filesystem;

    EventBuffer::EventBuffer(size_t max_memory_events,
                             bool disk_backup,
                             const std::string& disk_path)
    : max_memory_events(max_memory_events)
    , use_disk_backup(disk_backup)
    , disk_path(disk_path) {

        if (use_disk_backup) {
            ensure_disk_directory();
            load_from_disk();
        }
    }

    EventBuffer::~EventBuffer() {
        if (use_disk_backup && !memory_buffer.empty()) {
            save_to_disk();
        }
    }

    void EventBuffer::add_event(const SecurityEvent& event) {
        std::lock_guard<std::mutex> lock(buffer_mutex);

        memory_buffer.push_back(event);

        if (memory_buffer.size() > max_memory_events && use_disk_backup) {
            save_to_disk();
            memory_buffer.clear();
        }

        buffer_cv.notify_one();
    }

    Vector<SecurityEvent> EventBuffer::get_batch(size_t batch_size) {
        std::unique_lock<std::mutex> lock(buffer_mutex);

        if (memory_buffer.size() < batch_size) {
            buffer_cv.wait_for(lock, std::chrono::seconds(1));
        }

        Vector<SecurityEvent> batch;
        size_t count = std::min(batch_size, memory_buffer.size());

        for (size_t i = 0; i < count; ++i) {
            batch.push_back(memory_buffer[i]);
        }

        for (size_t i = 0; i < count; ++i) {
            memory_buffer.erase(0);
        }

        return batch;
    }

    void EventBuffer::save_to_disk() {
        if (!use_disk_backup) return;

        try {
            std::string filename = get_dump_filename();
            std::ofstream file(filename, std::ios::binary);

            if (!file.is_open()) {
                std::cerr << "[ERROR] Cannot open file for writing: " << filename << std::endl;
                return;
            }

            json j = json::array();
            for (const auto& event : memory_buffer) {
                j.push_back(event.to_json());
            }

            file << j.dump();
            file.close();

            std::cout << "[INFO] Saved " << memory_buffer.size() << " events to disk" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to save buffer to disk: " << e.what() << std::endl;
        }
    }

    void EventBuffer::load_from_disk() {
        if (!use_disk_backup) return;

        try {
            std::string filename = get_dump_filename();
            if (!fs::exists(filename)) return;

            std::ifstream file(filename);
            if (!file.is_open()) return;

            json j;
            file >> j;

            if (j.is_array()) {
                memory_buffer.clear();
                for (const auto& event_json : j) {
                    memory_buffer.push_back(SecurityEvent(event_json));
                }

                std::cout << "[INFO] Loaded " << memory_buffer.size() << " events from disk" << std::endl;

                fs::remove(filename);
            }

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to load buffer from disk: " << e.what() << std::endl;
        }
    }

    void EventBuffer::clear() {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        memory_buffer.clear();

        if (use_disk_backup) {
            std::string filename = get_dump_filename();
            if (fs::exists(filename)) {
                fs::remove(filename);
            }
        }
    }

    size_t EventBuffer::size() const {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        return memory_buffer.size();
    }

    bool EventBuffer::empty() const {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        return memory_buffer.empty();
    }

    void EventBuffer::ensure_disk_directory() const {
        try {
            if (!disk_path.empty()) {
                fs::create_directories(disk_path);
            }
        } catch (...) {
        }
    }

    std::string EventBuffer::get_dump_filename() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << disk_path << "/buffer_"
        << std::put_time(std::gmtime(&time_t_now), "%Y%m%d_%H%M%S")
        << ".json";

        return ss.str();
    }

}
