#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include "core/PlcTagManager.hpp"
#include "httplib.h" // header-only in src/3rdparty

class WebServer {
public:
    WebServer(PlcTagManager& tag_manager, const std::string& host = "0.0.0.0", uint16_t port = 8080, bool manual_mode = false);
    ~WebServer();

    bool start();
    void stop();

    bool isRunning() const { return is_running_; }

private:
    void run();

    PlcTagManager& tag_manager_;
    std::string host_;
    uint16_t port_;
    bool manual_mode_;

    httplib::Server server_;
    std::unique_ptr<std::thread> server_thread_;
    std::atomic<bool> is_running_;
};

#endif // WEB_SERVER_HPP
