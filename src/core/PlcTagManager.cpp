#include "PlcTagManager.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include "json.hpp" // Header-only JSON library in src/3rdparty
#include <mutex>

using json = nlohmann::json;

PlcTagManager::PlcTagManager(PlcMemory& memory, const std::string& config_path)
    : memory_(memory), config_path_(config_path) {
    loadConfig();
}

bool PlcTagManager::addTag(const PlcTag& tag) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Check duplicate name
    auto it = std::find_if(tags_.begin(), tags_.end(), [&tag](const PlcTag& t) {
        return t.name == tag.name;
    });
    if (it != tags_.end()) {
        std::cerr << "[PlcTagManager] Tag with name " << tag.name << " already exists." << std::endl;
        return false;
    }

    // Validate memory boundaries
    if (tag.area == "Coils" && tag.address >= memory_.getCoilsSize()) return false;
    if (tag.area == "DiscreteInputs" && tag.address >= memory_.getDiscreteInputsSize()) return false;
    if (tag.area == "InputRegisters" && tag.address >= memory_.getInputRegistersSize()) return false;
    if (tag.area == "HoldingRegisters" && tag.address >= memory_.getHoldingRegistersSize()) return false;

    tags_.push_back(tag);
    lock.unlock(); // release lock before saving to avoid deadlock if saveConfig is called inside lock
    saveConfig();
    return true;
}

bool PlcTagManager::removeTag(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = std::find_if(tags_.begin(), tags_.end(), [&name](const PlcTag& t) {
        return t.name == name;
    });
    if (it == tags_.end()) {
        return false;
    }
    tags_.erase(it);
    lock.unlock();
    saveConfig();
    return true;
}

std::vector<PlcTag> PlcTagManager::getAllTags() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return tags_;
}

bool PlcTagManager::hasTag(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = std::find_if(tags_.begin(), tags_.end(), [&name](const PlcTag& t) {
        return t.name == name;
    });
    return it != tags_.end();
}

std::string PlcTagManager::readTagValue(const PlcTag& tag) const {
    if (tag.area == "Coils") {
        return memory_.readCoil(tag.address) ? "1" : "0";
    } else if (tag.area == "DiscreteInputs") {
        return memory_.readDiscreteInput(tag.address) ? "1" : "0";
    } else if (tag.area == "InputRegisters") {
        return std::to_string(memory_.readInputRegister(tag.address));
    } else if (tag.area == "HoldingRegisters") {
        return std::to_string(memory_.readHoldingRegister(tag.address));
    }
    return "0";
}

bool PlcTagManager::writeTagValue(const std::string& name, const std::string& value_str) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = std::find_if(tags_.begin(), tags_.end(), [&name](const PlcTag& t) {
        return t.name == name;
    });
    if (it == tags_.end()) {
        return false;
    }

    const PlcTag& tag = *it;
    try {
        if (tag.area == "Coils") {
            bool val = (value_str == "1" || value_str == "true" || value_str == "ON");
            memory_.writeCoil(tag.address, val);
        } else if (tag.area == "DiscreteInputs") {
            bool val = (value_str == "1" || value_str == "true" || value_str == "ON");
            memory_.writeDiscreteInput(tag.address, val);
        } else if (tag.area == "InputRegisters") {
            uint16_t val = static_cast<uint16_t>(std::stoi(value_str));
            memory_.writeInputRegister(tag.address, val);
        } else if (tag.area == "HoldingRegisters") {
            uint16_t val = static_cast<uint16_t>(std::stoi(value_str));
            memory_.writeHoldingRegister(tag.address, val);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PlcTagManager] Error parsing value '" << value_str << "' for tag " << name << ": " << e.what() << std::endl;
        return false;
    }
}

bool PlcTagManager::loadConfig() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        std::cout << "[PlcTagManager] No existing tag config found. Starting fresh." << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;
        tags_.clear();
        for (const auto& item : j) {
            PlcTag tag;
            tag.name = item.value("name", "");
            tag.area = item.value("area", "");
            tag.address = item.value("address", static_cast<uint16_t>(0));
            tag.type = item.value("type", "");
            tag.description = item.value("description", "");
            
            // Validation check
            if (tag.name.empty() || tag.area.empty() || tag.type.empty()) continue;
            
            tags_.push_back(tag);
        }
        std::cout << "[PlcTagManager] Loaded " << tags_.size() << " tags from " << config_path_ << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PlcTagManager] Error parsing configuration file: " << e.what() << std::endl;
        return false;
    }
}

bool PlcTagManager::saveConfig() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::ofstream file(config_path_);
    if (!file.is_open()) {
        std::cerr << "[PlcTagManager] Failed to open " << config_path_ << " for writing." << std::endl;
        return false;
    }

    try {
        json j = json::array();
        for (const auto& tag : tags_) {
            json item;
            item["name"] = tag.name;
            item["area"] = tag.area;
            item["address"] = tag.address;
            item["type"] = tag.type;
            item["description"] = tag.description;
            j.push_back(item);
        }
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PlcTagManager] Error saving configuration: " << e.what() << std::endl;
        return false;
    }
}
