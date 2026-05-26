#include "WebServer.hpp"
#include "WebUI.hpp"
#include "json.hpp" // Header-only json library in src/3rdparty
#include <iostream>

using json = nlohmann::json;

WebServer::WebServer(PlcTagManager& tag_manager, const std::string& host, uint16_t port, bool manual_mode)
    : tag_manager_(tag_manager), host_(host), port_(port), manual_mode_(manual_mode), is_running_(false) {
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (is_running_) {
        return true;
    }

    // Set up API routes
    server_.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(WebUI::INDEX_HTML, "text/html; charset=utf-8");
    });

    server_.Get("/api/tags", [this](const httplib::Request&, httplib::Response& res) {
        try {
            auto tags = tag_manager_.getAllTags();
            json j = json::array();
            for (const auto& tag : tags) {
                json item;
                item["name"] = tag.name;
                item["area"] = tag.area;
                item["address"] = tag.address;
                item["type"] = tag.type;
                item["description"] = tag.description;
                item["value"] = tag_manager_.readTagValue(tag);
                j.push_back(item);
            }
            res.set_content(j.dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("Internal Error: ") + e.what(), "text/plain");
        }
    });

    server_.Post("/api/tags", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            PlcTag tag;
            tag.name = body.value("name", "");
            tag.area = body.value("area", "");
            tag.address = body.value("address", static_cast<uint16_t>(0));
            tag.type = body.value("type", "");
            tag.description = body.value("description", "");

            if (tag.name.empty() || tag.area.empty() || tag.type.empty()) {
                res.status = 400;
                res.set_content("Invalid tag details. Required fields missing.", "text/plain");
                return;
            }

            if (tag_manager_.addTag(tag)) {
                res.status = 200;
                res.set_content("Success", "text/plain");
            } else {
                res.status = 400;
                res.set_content("Failed to add tag. Name might be duplicate or address out of bounds.", "text/plain");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("JSON Parsing Error: ") + e.what(), "text/plain");
        }
    });

    server_.Delete("/api/tags", [this](const httplib::Request& req, httplib::Response& res) {
        std::string name = req.get_param_value("name");
        if (name.empty()) {
            res.status = 400;
            res.set_content("Missing parameter 'name'", "text/plain");
            return;
        }

        if (tag_manager_.removeTag(name)) {
            res.status = 200;
            res.set_content("Success", "text/plain");
        } else {
            res.status = 400;
            res.set_content("Tag not found", "text/plain");
        }
    });

    server_.Post("/api/tags/write", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string value = body.value("value", "");

            if (name.empty() || value.empty()) {
                res.status = 400;
                res.set_content("Missing 'name' or 'value' fields.", "text/plain");
                return;
            }

            if (tag_manager_.writeTagValue(name, value)) {
                res.status = 200;
                res.set_content("Success", "text/plain");
            } else {
                res.status = 400;
                res.set_content("Failed to write tag value. Tag might not exist or parsing failed.", "text/plain");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("JSON Parsing Error: ") + e.what(), "text/plain");
        }
    });

    server_.Get("/api/mappings", [this](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("mappings.json");
        if (!file.is_open()) {
            res.set_content("[]", "application/json; charset=utf-8");
            return;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        res.set_content(ss.str(), "application/json; charset=utf-8");
    });

    server_.Post("/api/mappings", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            if (!body.is_array()) {
                res.status = 400;
                res.set_content("Mappings must be a JSON array.", "text/plain");
                return;
            }

            std::ofstream file("mappings.json");
            if (!file.is_open()) {
                res.status = 500;
                res.set_content("Failed to open mappings.json for writing.", "text/plain");
                return;
            }
            file << body.dump(4);
            file.close();

            if (tag_manager_.getMemory().loadMappings("mappings.json")) {
                res.status = 200;
                res.set_content("Success", "text/plain");
            } else {
                res.status = 400;
                res.set_content("Failed to parse some mappings, but saved file.", "text/plain");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("JSON Parsing Error: ") + e.what(), "text/plain");
        }
    });

    server_.Get("/api/memory/read", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string address = req.get_param_value("address");
            if (address.empty()) {
                res.status = 400;
                res.set_content("Missing parameter 'address'", "text/plain");
                return;
            }

            auto& memory = tag_manager_.getMemory();
            auto ref = memory.parseAddress(address);
            uint16_t val = memory.readValue(ref);

            json j;
            j["address"] = address;
            j["value"] = val;
            res.set_content(j.dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("Error reading memory: ") + e.what(), "text/plain");
        }
    });

    server_.Post("/api/memory/write", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string address = body.value("address", "");
            int val_int = body.value("value", -1);

            if (address.empty() || val_int < 0) {
                res.status = 400;
                res.set_content("Missing 'address' or 'value' fields.", "text/plain");
                return;
            }

            auto& memory = tag_manager_.getMemory();
            auto ref = memory.parseAddress(address);
            memory.writeValue(ref, static_cast<uint16_t>(val_int));
            memory.syncMappings();

            res.status = 200;
            res.set_content("Success", "text/plain");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("Error writing memory: ") + e.what(), "text/plain");
        }
    });

    server_.Get("/api/system", [this](const httplib::Request&, httplib::Response& res) {
        json j;
        j["mode"] = manual_mode_ ? "MANUAL" : "AUTO";
        j["cycleTimeMs"] = 20.0;
        res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    is_running_ = true;
    server_thread_ = std::make_unique<std::thread>(&WebServer::run, this);
    
    std::cout << "[WebServer] Running on http://" << host_ << ":" << port_ << std::endl;
    return true;
}

void WebServer::stop() {
    if (!is_running_) {
        return;
    }

    is_running_ = false;
    server_.stop();

    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    server_thread_.reset();
    std::cout << "[WebServer] Stopped." << std::endl;
}

void WebServer::run() {
    if (!server_.listen(host_, port_)) {
        std::cerr << "[WebServer] Failed to listen on " << host_ << ":" << port_ << std::endl;
        is_running_ = false;
    }
}
