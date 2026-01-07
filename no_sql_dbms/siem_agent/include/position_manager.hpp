#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace siem {

    struct FilePosition {
        std::string filename;
        std::string inode;
        off_t last_position;
        time_t last_modification;

        FilePosition() : last_position(0), last_modification(0) {}
    };

    class PositionManager {
    public:
        PositionManager(const std::string& position_file);

        bool load_positions();
        bool save_positions();
        FilePosition get_position(const std::string& filename);
        void update_position(const std::string& filename,
                             const std::string& inode,
                             off_t position,
                             time_t modification_time);
        void remove_position(const std::string& filename);

    private:
        std::string position_file_;
        std::unordered_map<std::string, FilePosition> positions_;
        mutable std::mutex mutex_;
    };

}
