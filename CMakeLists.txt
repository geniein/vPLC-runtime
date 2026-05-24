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
include_directories(src /opt/homebrew/include)
link_directories(/opt/homebrew/lib)

# Threading support
find_package(Threads REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/core/PlcMemory.cpp
    src/core/PlcLoader.cpp
    src/core/PlcScheduler.cpp
    src/modbus/ModbusServer.cpp
    src/tui/PlcTui.cpp
    src/s7/S7Server.cpp
    src/mc/McServer.cpp
    src/xgt/XgtServer.cpp
    src/mqtt/MqttPublisher.cpp
)

# Executable
add_executable(vPlc ${SOURCES})
target_link_libraries(vPlc PRIVATE Threads::Threads snap7)
