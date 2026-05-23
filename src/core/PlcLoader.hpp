#ifndef PLC_LOADER_HPP
#define PLC_LOADER_HPP

#include "core/PlcMemory.hpp"
#include <string>
#include <vector>
#include <memory>

class PlcLoader {
public:
    struct VarBinding {
        enum class Type {
            DISCRETE_INPUT,  // %IX -> Bool (stored as uint8_t in MatIEC)
            COIL,            // %QX -> Bool (stored as uint8_t in MatIEC)
            INPUT_REGISTER,  // %IW -> Word (uint16_t)
            HOLDING_REGISTER // %MW -> Word (uint16_t)
        };

        Type type;
        uint16_t plc_address;
        void* sym_ptr;
        std::string name;
    };

    PlcLoader(PlcMemory& memory);
    ~PlcLoader();

    // Load shared library (.dylib or .so)
    bool loadLibrary(const std::string& path);
    
    // Unload library and clean up
    void unloadLibrary();

    // Run the lifecycle methods in the library
    void initLogic();
    void runLogic(unsigned long tick);

    // Sync input registers/coils from PlcMemory to the DLL memory space
    void syncInputsToDll();

    // Sync output registers/coils from the DLL memory space back to PlcMemory
    void syncOutputsFromDll();

    bool isLoaded() const { return library_handle_ != nullptr; }
    const std::string& getLibraryPath() const { return library_path_; }
    const std::vector<VarBinding>& getBindings() const { return bindings_; }

private:
    // Resolve standard MatIEC symbols
    void resolveBindings();

    PlcMemory& memory_;
    std::string library_path_;
    void* library_handle_;

    // Function pointers for MatIEC lifecycle hooks
    void (*config_init_fn_)(void);
    void (*config_run_fn_)(unsigned long);

    // Variable bindings resolved from the library
    std::vector<VarBinding> bindings_;
};

#endif // PLC_LOADER_HPP
