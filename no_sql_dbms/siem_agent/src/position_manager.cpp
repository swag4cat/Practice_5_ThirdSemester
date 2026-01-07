#include "../include/position_manager.hpp"
#include <iostream>
#include <fstream>
#include "../../parcer/json.hpp"

using json = nlohmann::json;

namespace siem {

namespace fs = std::filesystem;

PositionManager::PositionManager(const std::string& position_file)
    : position_file_(position_file) {
    std::cout << "[DEBUG] PositionManager initialized with file: "
              << position_file_ << std::endl;
}

bool PositionManager::load_positions() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!fs::exists(position_file_)) {
        std::cout << "[INFO] Position file does not exist, starting fresh: "
                 << position_file_ << std::endl;
        return true;
    }

    try {
        std::ifstream file(position_file_);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Cannot open position file: " << position_file_ << std::endl;
            return false;
        }

        json j;
        file >> j;

        positions_.clear();
        for (auto it = j.begin(); it != j.end(); ++it) {
            FilePosition pos;
            pos.filename = it.key();
            pos.inode = it->value("inode", "");

            long saved_position = it->value("position", 0L);
            if (saved_position < 0) {
                std::cout << "[WARN] Invalid position (" << saved_position
                         << ") for file " << pos.filename << ", resetting to 0" << std::endl;
                saved_position = 0;
            }
            pos.last_position = saved_position;

            pos.last_modification = it->value("modification", 0L);
            positions_[pos.filename] = pos;

            std::cout << "[DEBUG] Loaded position for " << pos.filename
                     << ": inode=" << pos.inode
                     << ", pos=" << pos.last_position << std::endl;
        }

        std::cout << "[INFO] Loaded " << positions_.size() << " file positions" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to load positions: " << e.what() << std::endl;
        return false;
    }
}

bool PositionManager::save_positions() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        fs::path dir = fs::path(position_file_).parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            std::cout << "[INFO] Creating directory for position file: " << dir << std::endl;
            fs::create_directories(dir);
        }

        std::ofstream file(position_file_);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Cannot create position file: " << position_file_ << std::endl;
            return false;
        }

        json j;
        for (const auto& [filename, pos] : positions_) {
            j[filename]["inode"] = pos.inode;
            j[filename]["position"] = pos.last_position;
            j[filename]["modification"] = pos.last_modification;
        }

        file << j.dump(2);
        std::cout << "[DEBUG] Saved " << positions_.size()
                 << " positions to " << position_file_ << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to save positions to " << position_file_
                 << ": " << e.what() << std::endl;
        return false;
    }
}

FilePosition PositionManager::get_position(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = positions_.find(filename);
    if (it != positions_.end()) {
        return it->second;
    }

    FilePosition new_pos;
    new_pos.filename = filename;
    return new_pos;
}

void PositionManager::update_position(const std::string& filename,
                                     const std::string& inode,
                                     off_t position,
                                     time_t modification_time) {
    std::lock_guard<std::mutex> lock(mutex_);

    FilePosition& pos = positions_[filename];
    pos.filename = filename;
    pos.inode = inode;
    pos.last_position = position;
    pos.last_modification = modification_time;

    static int save_counter = 0;
    save_counter++;
    if (save_counter >= 10) {
        save_positions();
        save_counter = 0;
    }
}

void PositionManager::remove_position(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    positions_.erase(filename);
    std::cout << "[INFO] Removed position tracking for: " << filename << std::endl;
}

}
