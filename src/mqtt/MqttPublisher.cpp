#include "mqtt/MqttPublisher.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

MqttPublisher::MqttPublisher(PlcMemory& memory, PlcLoader& loader, const std::string& broker_ip, int port)
    : memory_(memory),
      loader_(loader),
      broker_ip_(broker_ip),
      port_(port),
      mosq_(nullptr),
      is_running_(false),
      is_connected_(false) {
    
    // Initialize mosquitto library
    mosquitto_lib_init();
}

MqttPublisher::~MqttPublisher() {
    stop();
    mosquitto_lib_cleanup();
}

bool MqttPublisher::start() {
    if (is_running_) return true;

    // Create a new mosquitto client instance
    mosq_ = mosquitto_new("vplc_telemetry_publisher", true, nullptr);
    if (!mosq_) {
        std::cerr << "[MQTT] Failed to create mosquitto instance." << std::endl;
        return false;
    }

    // Connect to the broker (blocking connect in start sequence)
    std::cout << "[MQTT] Connecting to broker at " << broker_ip_ << ":" << port_ << "..." << std::endl;
    int rc = mosquitto_connect(mosq_, broker_ip_.c_str(), port_, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Connection failed with error code: " << rc << " (" << mosquitto_strerror(rc) << ")" << std::endl;
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        return false;
    }

    is_connected_ = true;
    is_running_ = true;
    std::cout << "[MQTT] Successfully connected to broker!" << std::endl;

    // Launch background publishing thread
    thread_ = std::thread(&MqttPublisher::publishLoop, this);
    return true;
}

void MqttPublisher::stop() {
    if (!is_running_) return;

    is_running_ = false;

    if (thread_.joinable()) {
        thread_.join();
    }

    if (mosq_) {
        std::cout << "[MQTT] Disconnecting from broker..." << std::endl;
        mosquitto_disconnect(mosq_);
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
    }

    is_connected_ = false;
    std::cout << "[MQTT] Stopped publisher successfully." << std::endl;
}

void MqttPublisher::publishLoop() {
    while (is_running_) {
        // Run network loop for keeping alive and processing background packets
        int rc = mosquitto_loop(mosq_, 0, 1);
        if (rc != MOSQ_ERR_SUCCESS) {
            // Reconnect if loop fails
            is_connected_ = false;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            mosquitto_reconnect(mosq_);
            continue;
        }

        is_connected_ = true;

        // Check active simulation mode from loaded library path
        std::string lib_path = loader_.getLibraryPath();
        bool is_assembly = (lib_path.find("assembly") != std::string::npos);

        std::string topic;
        std::stringstream payload;

        if (is_assembly) {
            // Read active metrics for Automotive Twins
            uint16_t completed_cars = memory_.readHoldingRegister(0);  // %MW0
            uint16_t conveyor_speed = memory_.readHoldingRegister(1);  // %MW1
            uint16_t encoder_pos    = memory_.readInputRegister(0);    // %IW0

            topic = "vplc/assembly/state";
            payload << "{"
                    << "\"completed_cars\":" << completed_cars << ","
                    << "\"conveyor_speed\":" << conveyor_speed << ","
                    << "\"encoder_position\":" << encoder_pos
                    << "}";
        } else {
            // Read active metrics for Water Tank Twins (Optional, but elegant!)
            uint16_t water_level = memory_.readInputRegister(0);      // %IW0
            uint16_t set_point   = memory_.readHoldingRegister(1);    // %MW1
            uint16_t pump_starts = memory_.readHoldingRegister(0);    // %MW0

            topic = "vplc/tank/state";
            payload << "{"
                    << "\"water_level\":" << water_level << ","
                    << "\"set_point\":" << set_point << ","
                    << "\"pump_starts\":" << pump_starts
                    << "}";
        }

        std::string payload_str = payload.str();
        
        // Publish JSON payload (QoS 0, retain = false)
        int pub_rc = mosquitto_publish(
            mosq_, 
            nullptr, 
            topic.c_str(), 
            static_cast<int>(payload_str.length()), 
            payload_str.c_str(), 
            0, 
            false
        );

        if (pub_rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "[MQTT] Publish failed: " << mosquitto_strerror(pub_rc) << std::endl;
        }

        // Sleep for 500ms
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
