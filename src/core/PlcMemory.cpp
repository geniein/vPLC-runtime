#include "PlcMemory.hpp"
#include <stdexcept>
#include <iostream>

PlcMemory::PlcMemory(size_t coils_size,
                      size_t discrete_inputs_size,
                      size_t input_registers_size,
                      size_t holding_registers_size)
    : coils_(coils_size, false),
      discrete_inputs_(discrete_inputs_size, false),
      input_registers_(input_registers_size, 0),
      holding_registers_(holding_registers_size, 0) {
    std::cout << "[PlcMemory] Memory initialized with sizes - Coils: " << coils_size
              << ", Discrete Inputs: " << discrete_inputs_size
              << ", Input Registers: " << input_registers_size
              << ", Holding Registers: " << holding_registers_size << std::endl;
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
