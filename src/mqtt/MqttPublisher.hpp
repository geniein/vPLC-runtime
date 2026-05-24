#ifndef MQTT_PUBLISHER_HPP
#define MQTT_PUBLISHER_HPP

#include "core/PlcMemory.hpp"
#include "core/PlcLoader.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <mosquitto.h>

class MqttPublisher {
public:
    MqttPublisher(PlcMemory& memory, PlcLoader& loader, const std::string& broker_ip, int port = 1883);
    ~MqttPublisher();

    // Start/Stop the asynchronous MQTT publish thread
    bool start();
    void stop();

    bool isConnected() const { return is_connected_; }

private:
    void publishLoop();

    PlcMemory& memory_;
    PlcLoader& loader_;
    std::string broker_ip_;
    int port_;

    struct mosquitto* mosq_;
    std::atomic<bool> is_running_;
    std::atomic<bool> is_connected_;
    std::thread thread_;
};

#endif // MQTT_PUBLISHER_HPP
