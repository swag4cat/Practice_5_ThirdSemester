#pragma once

#include "event.hpp"
#include "../../include/vector.hpp"
#include <string>
#include <mutex>
#include <condition_variable>

namespace siem {

    class EventBuffer {
    public:
        EventBuffer(size_t max_memory_events = 1000,
                    bool disk_backup = true,
                    const std::string& disk_path = "/tmp/siem_buffer");
        ~EventBuffer();

        void add_event(const SecurityEvent& event);
        Vector<SecurityEvent> get_batch(size_t batch_size);
        void save_to_disk();
        void load_from_disk();
        void clear();
        size_t size() const;
        bool empty() const;

    private:
        void ensure_disk_directory() const;
        std::string get_dump_filename() const;

        Vector<SecurityEvent> memory_buffer;
        size_t max_memory_events;
        bool use_disk_backup;
        std::string disk_path;

        mutable std::mutex buffer_mutex;
        std::condition_variable buffer_cv;
    };

}
