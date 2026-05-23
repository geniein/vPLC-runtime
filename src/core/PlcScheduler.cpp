#include "PlcScheduler.hpp"
#include <iostream>
#include <cmath>

PlcScheduler::PlcScheduler(PlcLoader& loader, double cycle_time_ms)
    : loader_(loader),
      target_cycle_ms_(cycle_time_ms),
      is_running_(false) {
    stats_.target_cycle_ms = cycle_time_ms;
}

PlcScheduler::~PlcScheduler() {
    stop();
}

bool PlcScheduler::start() {
    if (is_running_) return true;

    is_running_ = true;
    thread_ = std::thread(&PlcScheduler::loop, this);
    std::cout << "[PlcScheduler] Scheduler thread started with cycle: " << target_cycle_ms_ << " ms" << std::endl;
    return true;
}

void PlcScheduler::stop() {
    if (!is_running_) return;

    is_running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    std::cout << "[PlcScheduler] Scheduler thread stopped" << std::endl;
}

PlcScheduler::Stats PlcScheduler::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void PlcScheduler::loop() {
    unsigned long tick = 0;
    auto prev_start = std::chrono::steady_clock::now();
    auto next_wakeup = std::chrono::steady_clock::now();

    // Run custom initialization before loop starts
    loader_.initLogic();
    loader_.syncOutputsFromDll();

    while (is_running_) {
        // Record cycle start time
        auto start = std::chrono::steady_clock::now();
        tick++;

        // 1. Sync Modbus inputs to shared library space
        loader_.syncInputsToDll();

        // 2. Execute PLC Logic
        loader_.runLogic(tick);

        // 3. Sync outputs from shared library space back to Modbus registers
        loader_.syncOutputsFromDll();

        // Record execution end time
        auto end = std::chrono::steady_clock::now();
        
        // Calculate Scan Time
        double scan_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Calculate Jitter (cycle start time variation)
        double jitter = 0.0;
        if (tick > 1) {
            double actual_cycle_ms = std::chrono::duration<double, std::milli>(start - prev_start).count();
            jitter = std::abs(actual_cycle_ms - target_cycle_ms_);
        }
        prev_start = start;

        // Update thread-safe statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_ticks = tick;
            stats_.last_scan_time_ms = scan_time_ms;
            
            if (tick == 1) {
                stats_.avg_scan_time_ms = scan_time_ms;
                stats_.max_scan_time_ms = scan_time_ms;
                stats_.jitter_ms = 0.0;
            } else {
                // Running averages to avoid overflows
                stats_.avg_scan_time_ms = stats_.avg_scan_time_ms + (scan_time_ms - stats_.avg_scan_time_ms) / tick;
                stats_.max_scan_time_ms = std::max(stats_.max_scan_time_ms, scan_time_ms);
                stats_.jitter_ms = stats_.jitter_ms + (jitter - stats_.jitter_ms) / tick;
            }
        }

        // Calculate next tick wakeup time point (prevents drift!)
        next_wakeup += std::chrono::microseconds(static_cast<long long>(target_cycle_ms_ * 1000));
        
        // If we missed our deadline, catch up by resetting next_wakeup to now
        auto now = std::chrono::steady_clock::now();
        if (next_wakeup < now) {
            next_wakeup = now;
        }

        // Sleep until next scheduled wakeup
        std::this_thread::sleep_until(next_wakeup);
    }
}
