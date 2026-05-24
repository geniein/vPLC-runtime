#ifndef PLC_SCHEDULER_HPP
#define PLC_SCHEDULER_HPP

#include "core/PlcLoader.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

class PlcScheduler {
public:
    struct Stats {
        unsigned long total_ticks = 0;
        double target_cycle_ms = 20.0;
        double last_scan_time_ms = 0.0;
        double avg_scan_time_ms = 0.0;
        double max_scan_time_ms = 0.0;
        double jitter_ms = 0.0;
    };

    PlcScheduler(PlcLoader& loader, double cycle_time_ms = 20.0);
    ~PlcScheduler();

    bool start();
    void stop();

    bool isRunning() const { return is_running_; }
    Stats getStats() const;
    const PlcLoader& getLoader() const { return loader_; }

private:
    void loop();

    PlcLoader& loader_;
    double target_cycle_ms_;

    std::atomic<bool> is_running_;
    std::thread thread_;

    // Thread-safe statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;
};

#endif // PLC_SCHEDULER_HPP
