#include "PlcMemory.hpp"
#include "json.hpp" // Header-only JSON library in src/3rdparty
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

PlcMemory::PlcMemory(size_t coils_size,
                      size_t discrete_inputs_size,
                      size_t input_registers_size,
                      size_t holding_registers_size)
    : coils_(coils_size, false),
      discrete_inputs_(discrete_inputs_size, false),
      input_registers_(input_registers_size, 0),
      holding_registers_(holding_registers_size, 0),
      s7_pe_(2048, false),
      s7_pa_(2048, false),
      s7_db1_(1024, 0),
      s7_db2_(1024, 0),
      mc_x_(2048, false),
      mc_y_(2048, false),
      mc_m_(2048, false),
      mc_l_(2048, false),
      mc_b_(2048, false),
      mc_f_(2048, false),
      mc_sm_(2048, false),
      mc_d_(1024, 0),
      mc_w_(1024, 0),
      mc_r_(1024, 0),
      mc_zr_(1024, 0),
      mc_sd_(1024, 0),
      ls_i_(2048, false),
      ls_q_(2048, false),
      ls_m_(2048, false),
      ls_w_(1024, 0) {
    std::cout << "[PlcMemory] Memory initialized with sizes - Coils: " << coils_size
              << ", Discrete Inputs: " << discrete_inputs_size
              << ", Input Registers: " << input_registers_size
              << ", Holding Registers: " << holding_registers_size << std::endl;
    
    // Automatically load mappings at startup
    loadMappings("mappings.json");
}

// --- Coils (Read/Write) ---
bool PlcMemory::readCoil(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= coils_.size()) {
        return false; // Out of bounds, safe default
    }
    return coils_[address];
}

std::vector<bool> PlcMemory::readCoils(uint16_t start_address, uint16_t count) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<bool> result;
    if (start_address + count > coils_.size() || count == 0) {
        return result; // Empty result if invalid range
    }
    result.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        result.push_back(coils_[start_address + i]);
    }
    return result;
}

void PlcMemory::writeCoil(uint16_t address, bool value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < coils_.size()) {
        coils_[address] = value;
    }
}

void PlcMemory::writeCoils(uint16_t start_address, const std::vector<bool>& values) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (start_address + values.size() <= coils_.size()) {
        for (size_t i = 0; i < values.size(); ++i) {
            coils_[start_address + i] = values[i];
        }
    }
}

// --- Discrete Inputs (Read-Only) ---
bool PlcMemory::readDiscreteInput(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= discrete_inputs_.size()) {
        return false;
    }
    return discrete_inputs_[address];
}

std::vector<bool> PlcMemory::readDiscreteInputs(uint16_t start_address, uint16_t count) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<bool> result;
    if (start_address + count > discrete_inputs_.size() || count == 0) {
        return result;
    }
    result.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        result.push_back(discrete_inputs_[start_address + i]);
    }
    return result;
}

void PlcMemory::writeDiscreteInput(uint16_t address, bool value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < discrete_inputs_.size()) {
        discrete_inputs_[address] = value;
    }
}

void PlcMemory::writeDiscreteInputs(uint16_t start_address, const std::vector<bool>& values) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (start_address + values.size() <= discrete_inputs_.size()) {
        for (size_t i = 0; i < values.size(); ++i) {
            discrete_inputs_[start_address + i] = values[i];
        }
    }
}

// --- Input Registers (Read-Only 16-bit) ---
uint16_t PlcMemory::readInputRegister(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= input_registers_.size()) {
        return 0;
    }
    return input_registers_[address];
}

std::vector<uint16_t> PlcMemory::readInputRegisters(uint16_t start_address, uint16_t count) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<uint16_t> result;
    if (start_address + count > input_registers_.size() || count == 0) {
        return result;
    }
    result.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        result.push_back(input_registers_[start_address + i]);
    }
    return result;
}

void PlcMemory::writeInputRegister(uint16_t address, uint16_t value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < input_registers_.size()) {
        input_registers_[address] = value;
    }
}

void PlcMemory::writeInputRegisters(uint16_t start_address, const std::vector<uint16_t>& values) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (start_address + values.size() <= input_registers_.size()) {
        for (size_t i = 0; i < values.size(); ++i) {
            input_registers_[start_address + i] = values[i];
        }
    }
}

// --- Holding Registers (Read/Write 16-bit) ---
uint16_t PlcMemory::readHoldingRegister(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= holding_registers_.size()) {
        return 0;
    }
    return holding_registers_[address];
}

std::vector<uint16_t> PlcMemory::readHoldingRegisters(uint16_t start_address, uint16_t count) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<uint16_t> result;
    if (start_address + count > holding_registers_.size() || count == 0) {
        return result;
    }
    result.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        result.push_back(holding_registers_[start_address + i]);
    }
    return result;
}

void PlcMemory::writeHoldingRegister(uint16_t address, uint16_t value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < holding_registers_.size()) {
        holding_registers_[address] = value;
    }
}

void PlcMemory::writeHoldingRegisters(uint16_t start_address, const std::vector<uint16_t>& values) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (start_address + values.size() <= holding_registers_.size()) {
        for (size_t i = 0; i < values.size(); ++i) {
            holding_registers_[start_address + i] = values[i];
        }
    }
}

// --- Siemens S7 Buffers Getters/Setters ---
bool PlcMemory::readS7PE(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= s7_pe_.size()) return false;
    return s7_pe_[address];
}

void PlcMemory::writeS7PE(uint16_t address, bool value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < s7_pe_.size()) s7_pe_[address] = value;
}

bool PlcMemory::readS7PA(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= s7_pa_.size()) return false;
    return s7_pa_[address];
}

void PlcMemory::writeS7PA(uint16_t address, bool value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < s7_pa_.size()) s7_pa_[address] = value;
}

uint16_t PlcMemory::readS7DB1(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= s7_db1_.size()) return 0;
    return s7_db1_[address];
}

void PlcMemory::writeS7DB1(uint16_t address, uint16_t value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < s7_db1_.size()) s7_db1_[address] = value;
}

uint16_t PlcMemory::readS7DB2(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= s7_db2_.size()) return 0;
    return s7_db2_[address];
}

void PlcMemory::writeS7DB2(uint16_t address, uint16_t value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < s7_db2_.size()) s7_db2_[address] = value;
}

// --- MC Buffers Getters/Setters ---
bool PlcMemory::readMCBit(const std::string& area, uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (area == "X") return address < mc_x_.size() ? mc_x_[address] : false;
    if (area == "Y") return address < mc_y_.size() ? mc_y_[address] : false;
    if (area == "M") return address < mc_m_.size() ? mc_m_[address] : false;
    if (area == "L") return address < mc_l_.size() ? mc_l_[address] : false;
    if (area == "B") return address < mc_b_.size() ? mc_b_[address] : false;
    if (area == "F") return address < mc_f_.size() ? mc_f_[address] : false;
    if (area == "SM") return address < mc_sm_.size() ? mc_sm_[address] : false;
    return false;
}

void PlcMemory::writeMCBit(const std::string& area, uint16_t address, bool value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (area == "X" && address < mc_x_.size()) mc_x_[address] = value;
    else if (area == "Y" && address < mc_y_.size()) mc_y_[address] = value;
    else if (area == "M" && address < mc_m_.size()) mc_m_[address] = value;
    else if (area == "L" && address < mc_l_.size()) mc_l_[address] = value;
    else if (area == "B" && address < mc_b_.size()) mc_b_[address] = value;
    else if (area == "F" && address < mc_f_.size()) mc_f_[address] = value;
    else if (area == "SM" && address < mc_sm_.size()) mc_sm_[address] = value;
}

uint16_t PlcMemory::readMCWord(const std::string& area, uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (area == "D") return address < mc_d_.size() ? mc_d_[address] : 0;
    if (area == "W") return address < mc_w_.size() ? mc_w_[address] : 0;
    if (area == "R") return address < mc_r_.size() ? mc_r_[address] : 0;
    if (area == "ZR") return address < mc_zr_.size() ? mc_zr_[address] : 0;
    if (area == "SD") return address < mc_sd_.size() ? mc_sd_[address] : 0;
    return 0;
}

void PlcMemory::writeMCWord(const std::string& area, uint16_t address, uint16_t value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (area == "D" && address < mc_d_.size()) mc_d_[address] = value;
    else if (area == "W" && address < mc_w_.size()) mc_w_[address] = value;
    else if (area == "R" && address < mc_r_.size()) mc_r_[address] = value;
    else if (area == "ZR" && address < mc_zr_.size()) mc_zr_[address] = value;
    else if (area == "SD" && address < mc_sd_.size()) mc_sd_[address] = value;
}

// --- LS Buffers Getters/Setters ---
bool PlcMemory::readLSBit(const std::string& area, uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (area == "I") return address < ls_i_.size() ? ls_i_[address] : false;
    if (area == "Q") return address < ls_q_.size() ? ls_q_[address] : false;
    if (area == "M") return address < ls_m_.size() ? ls_m_[address] : false;
    return false;
}

void PlcMemory::writeLSBit(const std::string& area, uint16_t address, bool value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (area == "I" && address < ls_i_.size()) ls_i_[address] = value;
    else if (area == "Q" && address < ls_q_.size()) ls_q_[address] = value;
    else if (area == "M" && address < ls_m_.size()) ls_m_[address] = value;
}

uint16_t PlcMemory::readLSWord(uint16_t address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (address >= ls_w_.size()) return 0;
    return ls_w_[address];
}

void PlcMemory::writeLSWord(uint16_t address, uint16_t value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (address < ls_w_.size()) ls_w_[address] = value;
}

// --- Dynamic Mapping Engine ---

MemoryRef PlcMemory::parseAddress(const std::string& addr_str) const {
    MemoryRef ref;
    ref.index = 0;
    ref.is_bit = true;
    
    // Split by '.'
    std::vector<std::string> parts;
    std::stringstream ss(addr_str);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }
    
    if (parts.size() < 3) {
        throw std::runtime_error("Invalid address format: " + addr_str);
    }
    
    ref.protocol = parts[0];
    ref.area = parts[1];
    
    if (ref.protocol == "MODBUS") {
        if (ref.area == "COIL" || ref.area == "C") {
            ref.area = "COIL";
            ref.is_bit = true;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        } else if (ref.area == "DISCRETE_INPUT" || ref.area == "DI" || ref.area == "I") {
            ref.area = "DI";
            ref.is_bit = true;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        } else if (ref.area == "INPUT_REGISTER" || ref.area == "IR") {
            ref.area = "IR";
            ref.is_bit = false;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        } else if (ref.area == "HOLDING_REGISTER" || ref.area == "HR") {
            ref.area = "HR";
            ref.is_bit = false;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        }
    } else if (ref.protocol == "S7") {
        if (ref.area == "PE") {
            ref.is_bit = true;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        } else if (ref.area == "PA") {
            ref.is_bit = true;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        } else if (ref.area == "DB1" || ref.area == "DB2") {
            if (parts.size() == 4 && parts[2] == "W") {
                ref.is_bit = false;
                ref.index = static_cast<uint16_t>(std::stoi(parts[3]));
            } else {
                ref.is_bit = false;
                ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
            }
        }
    } else if (ref.protocol == "MC") {
        std::string area = ref.area;
        if (area == "X" || area == "Y" || area == "M" || area == "L" || area == "B" || area == "F" || area == "SM") {
            ref.is_bit = true;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        } else if (area == "D" || area == "W" || area == "R" || area == "ZR" || area == "SD") {
            ref.is_bit = false;
            if (parts.size() == 4 && parts[2] == "W") {
                ref.index = static_cast<uint16_t>(std::stoi(parts[3]));
            } else {
                ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
            }
        }
    } else if (ref.protocol == "LS") {
        std::string area = ref.area;
        if (area == "I" || area == "Q" || area == "M") {
            ref.is_bit = true;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        } else if (area == "W") {
            ref.is_bit = false;
            ref.index = static_cast<uint16_t>(std::stoi(parts[2]));
        }
    }
    
    return ref;
}

uint16_t PlcMemory::readValue(const MemoryRef& ref) const {
    if (ref.protocol == "MODBUS") {
        if (ref.area == "COIL") return readCoil(ref.index) ? 1 : 0;
        if (ref.area == "DI") return readDiscreteInput(ref.index) ? 1 : 0;
        if (ref.area == "IR") return readInputRegister(ref.index);
        if (ref.area == "HR") return readHoldingRegister(ref.index);
    } else if (ref.protocol == "S7") {
        if (ref.area == "PE") return readS7PE(ref.index) ? 1 : 0;
        if (ref.area == "PA") return readS7PA(ref.index) ? 1 : 0;
        if (ref.area == "DB1") return readS7DB1(ref.index);
        if (ref.area == "DB2") return readS7DB2(ref.index);
    } else if (ref.protocol == "MC") {
        if (ref.is_bit) {
            return readMCBit(ref.area, ref.index) ? 1 : 0;
        } else {
            return readMCWord(ref.area, ref.index);
        }
    } else if (ref.protocol == "LS") {
        if (ref.is_bit) {
            return readLSBit(ref.area, ref.index) ? 1 : 0;
        } else {
            return readLSWord(ref.index);
        }
    }
    return 0;
}

void PlcMemory::writeValue(const MemoryRef& ref, uint16_t value) {
    if (ref.protocol == "MODBUS") {
        if (ref.area == "COIL") writeCoil(ref.index, value != 0);
        else if (ref.area == "DI") writeDiscreteInput(ref.index, value != 0);
        else if (ref.area == "IR") writeInputRegister(ref.index, value);
        else if (ref.area == "HR") writeHoldingRegister(ref.index, value);
    } else if (ref.protocol == "S7") {
        if (ref.area == "PE") writeS7PE(ref.index, value != 0);
        else if (ref.area == "PA") writeS7PA(ref.index, value != 0);
        else if (ref.area == "DB1") writeS7DB1(ref.index, value);
        else if (ref.area == "DB2") writeS7DB2(ref.index, value);
    } else if (ref.protocol == "MC") {
        if (ref.is_bit) {
            writeMCBit(ref.area, ref.index, value != 0);
        } else {
            writeMCWord(ref.area, ref.index, value);
        }
    } else if (ref.protocol == "LS") {
        if (ref.is_bit) {
            writeLSBit(ref.area, ref.index, value != 0);
        } else {
            writeLSWord(ref.index, value);
        }
    }
}

bool PlcMemory::loadMappings(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mappings_mutex_);
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[PlcMemory] Failed to open mappings file: " << path << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;
        
        std::vector<MappingRule> new_mappings;
        for (const auto& item : j) {
            std::string src_raw = item.value("src", "");
            std::string dst_raw = item.value("dst", "");
            if (src_raw.empty() || dst_raw.empty()) continue;
            
            try {
                MappingRule rule;
                rule.src_raw = src_raw;
                rule.dst_raw = dst_raw;
                rule.src = parseAddress(src_raw);
                rule.dst = parseAddress(dst_raw);
                rule.initialized = false;
                
                new_mappings.push_back(rule);
            } catch (const std::exception& ex) {
                std::cerr << "[PlcMemory] Error parsing mapping rule (" << src_raw << " -> " << dst_raw << "): " << ex.what() << std::endl;
            }
        }
        
        mappings_ = std::move(new_mappings);
        std::cout << "[PlcMemory] Successfully loaded " << mappings_.size() << " dynamic mapping rules from " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PlcMemory] Error parsing mappings JSON: " << e.what() << std::endl;
        return false;
    }
}

void PlcMemory::syncMappings() {
    std::unique_lock<std::shared_mutex> lock(mappings_mutex_);
    for (auto& rule : mappings_) {
        try {
            uint16_t curr_src = readValue(rule.src);
            uint16_t curr_dst = readValue(rule.dst);
            
            if (!rule.initialized) {
                rule.last_src_val = curr_src;
                rule.last_dst_val = curr_dst;
                if (curr_src != curr_dst) {
                    writeValue(rule.dst, curr_src);
                    rule.last_dst_val = curr_src;
                }
                rule.initialized = true;
            } else {
                if (curr_src != rule.last_src_val) {
                    // Source changed, update destination
                    writeValue(rule.dst, curr_src);
                    rule.last_src_val = curr_src;
                    rule.last_dst_val = curr_src;
                } else if (curr_dst != rule.last_dst_val) {
                    // Destination changed, update source
                    writeValue(rule.src, curr_dst);
                    rule.last_src_val = curr_dst;
                    rule.last_dst_val = curr_dst;
                } else if (curr_src != curr_dst) {
                    // Out of sync but neither changed exclusively, sync source to destination
                    writeValue(rule.dst, curr_src);
                    rule.last_src_val = curr_src;
                    rule.last_dst_val = curr_src;
                }
            }
        } catch (const std::exception& e) {
            // Keep going, avoid crashing the PLC cycle
        }
    }
}

std::vector<MappingRule> PlcMemory::getMappings() const {
    std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
    return mappings_;
}
