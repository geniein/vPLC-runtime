cmake_minimum_required(VERSION 3.15)
project(vPlc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -pthread)
endif()

# Directories
if(APPLE)
    include_directories(src src/3rdparty /opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
else()
    include_directories(src src/3rdparty /usr/local/include /usr/include)
    link_directories(/usr/local/lib /usr/lib)
endif()

# Threading support
find_package(Threads REQUIRED)

# Source files for Main Executable
set(SOURCES
    src/main.cpp
    src/core/PlcMemory.cpp
    src/core/PlcTagManager.cpp
    src/core/PlcLoader.cpp
    src/core/PlcScheduler.cpp
    src/modbus/ModbusServer.cpp
    src/tui/PlcTui.cpp
    src/s7/S7Server.cpp
    src/mc/McServer.cpp
    src/xgt/XgtServer.cpp
    src/mqtt/MqttPublisher.cpp
    src/web/WebServer.cpp
)

# Executable
add_executable(vPlc ${SOURCES})
target_link_libraries(vPlc PRIVATE Threads::Threads snap7 mosquitto)

# Shared libraries (Simulators)
# CMake will automatically build .so on Linux and .dylib on macOS
add_library(mock_logic SHARED src/logic/mock_logic.cpp)
set_target_properties(mock_logic PROPERTIES PREFIX "lib")
target_include_directories(mock_logic PRIVATE src)

add_library(assembly_logic SHARED src/logic/assembly_logic.cpp)
set_target_properties(assembly_logic PROPERTIES PREFIX "lib")
target_include_directories(assembly_logic PRIVATE src)

