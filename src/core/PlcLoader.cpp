#include "PlcLoader.hpp"
#include <dlfcn.h>
#include <iostream>
#include <sstream>

PlcLoader::PlcLoader(PlcMemory& memory)
    : memory_(memory),
      library_handle_(nullptr),
      config_init_fn_(nullptr),
      config_run_fn_(nullptr) {}

PlcLoader::~PlcLoader() {
    unloadLibrary();
}

bool PlcLoader::loadLibrary(const std::string& path) {
    unloadLibrary();

    if (path.empty()) {
        std::cout << "[PlcLoader] No shared library specified. Running without logic (MANUAL/PASS-THROUGH mode)." << std::endl;
        return true;
    }

    std::cout << "[PlcLoader] Loading shared library: " << path << std::endl;

    // Load dynamic library
    library_handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!library_handle_) {
        std::cerr << "[PlcLoader] Error loading library: " << dlerror() << std::endl;
        return false;
    }

    library_path_ = path;

    // Resolve MatIEC standard lifecycle hooks
    config_init_fn_ = reinterpret_cast<void(*)(void)>(dlsym(library_handle_, "config_init__"));
    if (!config_init_fn_) {
        // Try fallback to just "init"
        config_init_fn_ = reinterpret_cast<void(*)(void)>(dlsym(library_handle_, "init"));
    }

    config_run_fn_ = reinterpret_cast<void(*)(unsigned long)>(dlsym(library_handle_, "config_run__"));
    if (!config_run_fn_) {
        // Try fallback to just "run"
        config_run_fn_ = reinterpret_cast<void(*)(unsigned long)>(dlsym(library_handle_, "run"));
    }

    if (!config_init_fn_ || !config_run_fn_) {
        std::cerr << "[PlcLoader] Warning: Lifecycle hooks (config_init__/config_run__) not fully resolved." << std::endl;
    } else {
        std::cout << "[PlcLoader] Lifecycle hooks resolved successfully." << std::endl;
    }

    // Resolve MatIEC variable symbols
    resolveBindings();

    return true;
}

void PlcLoader::unloadLibrary() {
    if (library_handle_) {
        dlclose(library_handle_);
        library_handle_ = nullptr;
    }
    library_path_.clear();
    config_init_fn_ = nullptr;
    config_run_fn_ = nullptr;
    bindings_.clear();
}

void PlcLoader::initLogic() {
    if (config_init_fn_) {
        std::cout << "[PlcLoader] Initializing PLC Logic..." << std::endl;
        config_init_fn_();
    }
}

void PlcLoader::runLogic(unsigned long tick) {
    if (config_run_fn_) {
        config_run_fn_(tick);
    }
}

void PlcLoader::syncInputsToDll() {
    memory_.syncMappings();
    for (auto& binding : bindings_) {
        // PLC Memory (Control outputs/parameters) -> DLL Logic Actuators
        if (binding.type == VarBinding::Type::COIL) {
            // Only sync external Coil writes to DLL if we are in MANUAL Mode!
            // In AUTO Mode, the DLL sequence logic itself controls all coils, so we must not overwrite them.
            bool auto_mode = memory_.readDiscreteInput(0); // %IX0.0 Auto Mode Switch
            if (!auto_mode) {
                bool val = memory_.readCoil(binding.plc_address);
                *static_cast<uint8_t*>(binding.sym_ptr) = val ? 1 : 0;
            }
        }
        else if (binding.type == VarBinding::Type::DISCRETE_INPUT) {
            // Auto Mode Switch (%IX0.0) or Safety Curtain (%IX0.7) can be written by TUI/Gateway
            // We sync only these specific external controls from PLC Memory to DLL
            if (binding.plc_address == 0 || binding.plc_address == 7) {
                bool val = memory_.readDiscreteInput(binding.plc_address);
                *static_cast<uint8_t*>(binding.sym_ptr) = val ? 1 : 0;
            }
        }
        else if (binding.type == VarBinding::Type::HOLDING_REGISTER) {
            // Sync all holding registers from PLC Memory to DLL to make external controls/parameters effective
            uint16_t val = memory_.readHoldingRegister(binding.plc_address);
            *static_cast<uint16_t*>(binding.sym_ptr) = val;
        }
    }
}

void PlcLoader::syncOutputsFromDll() {
    for (auto& binding : bindings_) {
        // DLL Logic Sensors/Status -> PLC Memory Registers
        if (binding.type == VarBinding::Type::DISCRETE_INPUT) {
            // Sync physical sensors computed by DLL back to PLC Memory Discrete Inputs
            // Avoid overwriting external controls (%IX0.0 and %IX0.7)
            if (binding.plc_address != 0 && binding.plc_address != 7) {
                uint8_t val = *static_cast<uint8_t*>(binding.sym_ptr);
                memory_.writeDiscreteInput(binding.plc_address, val != 0);
            }
        }
        else if (binding.type == VarBinding::Type::COIL) {
            // ALWAYS sync DLL-computed coils (outputs) back to PLC Memory
            // In AUTO Mode, this publishes DLL logic's decisions to external monitors (TUI/Gateway).
            // In MANUAL Mode, this does no harm because it's already synced from memory.
            uint8_t val = *static_cast<uint8_t*>(binding.sym_ptr);
            memory_.writeCoil(binding.plc_address, val != 0);
        }
        else if (binding.type == VarBinding::Type::INPUT_REGISTER) {
            // Sync all dynamic input register sensor values (e.g. %IW0 position, %IW1 etc.) from DLL to PLC Memory
            uint16_t val = *static_cast<uint16_t*>(binding.sym_ptr);
            memory_.writeInputRegister(binding.plc_address, val);
        }
        else if (binding.type == VarBinding::Type::HOLDING_REGISTER) {
            // Sync all holding registers computed or updated by DLL back to PLC Memory
            uint16_t val = *static_cast<uint16_t*>(binding.sym_ptr);
            memory_.writeHoldingRegister(binding.plc_address, val);
        }
    }
    memory_.syncMappings();
}

void PlcLoader::resolveBindings() {
    if (!library_handle_) return;

    bindings_.clear();

    // 1. Resolve Discrete Inputs (%IX0.0 to %IX0.63) -> Symbol: __IX0_x (uint8_t)
    for (uint16_t i = 0; i < 64; ++i) {
        std::stringstream ss;
        ss << "__IX0_" << i;
        std::string name = ss.str();
        void* ptr = dlsym(library_handle_, name.c_str());
        if (ptr) {
            bindings_.push_back({VarBinding::Type::DISCRETE_INPUT, i, ptr, name});
        }
    }

    // 2. Resolve Coils / Outputs (%QX0.0 to %QX0.63) -> Symbol: __QX0_x (uint8_t)
    for (uint16_t i = 0; i < 64; ++i) {
        std::stringstream ss;
        ss << "__QX0_" << i;
        std::string name = ss.str();
        void* ptr = dlsym(library_handle_, name.c_str());
        if (ptr) {
            bindings_.push_back({VarBinding::Type::COIL, i, ptr, name});
        }
    }

    // 3. Resolve Input Registers (%IW0 to %IW63) -> Symbol: __IWx (uint16_t)
    for (uint16_t i = 0; i < 64; ++i) {
        std::stringstream ss;
        ss << "__IW" << i;
        std::string name = ss.str();
        void* ptr = dlsym(library_handle_, name.c_str());
        if (ptr) {
            bindings_.push_back({VarBinding::Type::INPUT_REGISTER, i, ptr, name});
        }
    }

    // 4. Resolve Holding Registers (%MW0 to %MW63) -> Symbol: __MWx (uint16_t)
    for (uint16_t i = 0; i < 64; ++i) {
        std::stringstream ss;
        ss << "__MW" << i;
        std::string name = ss.str();
        void* ptr = dlsym(library_handle_, name.c_str());
        if (ptr) {
            bindings_.push_back({VarBinding::Type::HOLDING_REGISTER, i, ptr, name});
        }
    }

    std::cout << "[PlcLoader] Memory bindings resolved: " << bindings_.size() << std::endl;
    for (const auto& binding : bindings_) {
        std::cout << "  - " << binding.name << " bound to PLC address " << binding.plc_address << std::endl;
    }
}
