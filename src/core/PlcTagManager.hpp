#ifndef PLC_TAG_MANAGER_HPP
#define PLC_TAG_MANAGER_HPP

#include <string>
#include <vector>
#include <shared_mutex>
#include <memory>
#include "PlcMemory.hpp"

struct PlcTag {
    std::string name;
    std::string area; // "Coils", "DiscreteInputs", "InputRegisters", "HoldingRegisters"
    uint16_t address;
    std::string type; // "BOOL", "INT16", "UINT16"
    std::string description;
};

class PlcTagManager {
public:
    PlcTagManager(PlcMemory& memory, const std::string& config_path = "tags.json");
    ~PlcTagManager() = default;

    // CRUD Operations for Tags
    bool addTag(const PlcTag& tag);
    bool removeTag(const std::string& name);
    std::vector<PlcTag> getAllTags() const;
    bool hasTag(const std::string& name) const;

    // Memory Value Operations
    std::string readTagValue(const PlcTag& tag) const;
    bool writeTagValue(const std::string& name, const std::string& value_str);

    // Save and Load from config_path
    bool loadConfig();
    bool saveConfig() const;

private:
    PlcMemory& memory_;
    std::string config_path_;
    std::vector<PlcTag> tags_;
    mutable std::shared_mutex mutex_; // protects tags_ vector
};

#endif // PLC_TAG_MANAGER_HPP
